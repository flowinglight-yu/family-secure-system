# 家庭安防系统（Family Secure System）

基于 STM32F103C8 + ESP8266 + i.MX6ULL 的嵌入式家庭安防系统。

当前仓库中的代码已经形成可运行的双端架构：

- STM32 端负责采集 DHT11、MQ-2、SR501 数据，本地 OLED 显示，并通过 ESP8266 以 MQTT 方式向 OneNET 上报属性。
- i.MX6ULL 端运行 Qt Widgets 界面程序，通过 OneNET HTTP API 轮询最新属性，同时接入本地 V4L2 摄像头做视频监控。



---

## 系统架构

```
┌──────────────────┐       MQTT        ┌──────────────────┐
│   STM32F103C8    │  ──── ESP8266 ──→ │  OneNET 云平台    │
│  (传感器采集端)   │      WiFi         │  (MQTT + HTTP)   │
│  DHT11 / MQ-2    │                   └────────┬─────────┘
│  SR501 / OLED    │                            │ HTTP API
└──────────────────┘                   ┌────────┴─────────┐
                                       │   i.MX6ULL       │
                                       │  (Qt 5 显示终端)  │
                                       │  V4L2 摄像头采集  │
                                       └──────────────────┘
```

---

## 当前实现的功能

### STM32 传感器节点

| 功能 | 当前实现 |
|------|----------|
| 温湿度采集 | DHT11 单总线读取温度和湿度整数值 |
| 烟雾采集 | MQ-2 模拟量接入 ADC1_CH9，提供 raw 值和 PPM 估算值 |
| 人体检测 | SR501 数字量输入，高电平表示有人 |
| 本地显示 | OLED 4 行显示温度、湿度、烟雾 raw、人感状态 |
| 联网方式 | ESP8266 AT 指令入网，建立到 OneNET MQTT 服务器的 TCP 连接 |
| 云端上报 | 主循环每 200ms 刷新本地数据，每 5s 上报一次属性 |
| 云端订阅 | 订阅 OneNET 属性设置主题，并打印收到的 MQTT 下行内容 |

说明：当前代码已完成“订阅和打印”下行消息，但没有把云端下发指令映射成蜂鸣器、继电器或其他执行器动作。

### i.MX6ULL Qt 显示终端

| 页面/模块 | 当前实现 |
|-----------|----------|
| 锁屏页 | 数字键盘输入，密码要求 4 到 6 位数字 |
| 首页 | 四张卡片显示温湿度、烟雾状态、人感状态、系统在线状态 |
| 视频页 | V4L2 采集本地摄像头，默认设备为 /dev/video2，分辨率 640x480 MJPEG|
| 历史页 | 温湿度双曲线 + 烟雾曲线，支持最近 1 小时和 24 小时窗口 |
| 设置页 | 网络信息、OneNET 运行信息、阈值配置、屏幕亮度、密码修改、版本信息 |
| 云端轮询 | 每 2 秒轮询一次 OneNET 最新属性接口 |
| 离线判断 | 云端最近上报超过 20 秒视为设备离线；UI 超过 5 秒未收到新数据时显示离线遮罩 |
| 告警展示 | 烟雾超阈值、温度超阈值、人感触发、云端离线都会反映到顶部报警条和底部状态灯 |
| 本地持久化 | 锁屏密码、温度阈值、烟雾阈值保存到 security_ui.ini |
| 截图 | 将当前视频帧保存到系统图片目录（/home/用户名/Pictures）下的 family-security-captures 文件夹 |

说明：历史页当前只有曲线图；历史数据仅保存在程序运行期内存中，最多保留最近 48 小时样本，不会落盘。
说明：视频采集不会在程序启动时自动打开，需要用户在视频页点击“启动”按钮后才建立采集链路。

---

## 构建说明

### OneNET 云平台配置

在OneNet创建产品，然后配置好产品物模型，再创建接入设备

#### OneNET 物模型建议
如果要与当前代码保持一致，平台物模型需要以下属性：

| 标识符 | 类型 | 说明 |
|--------|------|------|
| temp | int | 温度 |
| humi | int | 湿度 |
| mq2 | int | 烟雾 raw 值 |
| mq2_ppm | float | 烟雾估算浓度 |
| sr501 | bool | 人体检测 |

---

#### 当前上报字段

STM32 实际上报的物模型字段为：

- temp：温度，整数
- humi：湿度，整数
- mq2：MQ-2 ADC 原始值
- mq2_ppm：MQ-2 浓度估算值
- sr501：布尔值，true 表示检测到有人

当前代码 OneJSON 负载格式简化示例如下：

```json
{
   "id": "123",
   "params": {
      "temp": {"value": 26},
      "humi": {"value": 55},
      "mq2": {"value": 320},
      "mq2_ppm": {"value": 2.15},
      "sr501": {"value": true}
   }
}
```

### STM32

当前启动文件中的栈大小已调整为 0x800，可直接沿用现有配置。
1. 使用 Keil MDK 打开 stm32/User/home-secure-system.uvprojx。
2. 根据现场环境修改 stm32/NET/esp8266.h 和 stm32/NET/onenet.h 中的宏定义。
- WiFi 名称和密码：stm32/NET/esp8266.h
- OneNET Product ID、Device Name、Access Key：stm32/NET/onenet.h
3. 编译并下载到 STM32F103C8 开发板。
4. 打开 USART1 调试串口，可查看 WiFi 入网、MQTT 建链和上报过程日志。

### i.MX6ULL / Qt

i.MX6ULL 端 OneNET 参数当前集中硬编码在 imx6ull/system-ui/onenetapiclient.cpp，包括：

- Product ID
- Device Name
- Device Key
- API Token

注意：Qt 端使用的是 HTTP API Token，不是 STM32 MQTT 建链所用的 Access Key。更换设备或平台参数时，两端都要同步修改。

1. 准备 Qt 5 开发环境，确保包含 Core、Gui、Widgets、Network 模块。
2. 进入 `imx6ull/system-ui` 目录执行 `qmake 01_smarthome.pro`。
3. 执行 `make` 编译程序。
4. 将可执行文件、Qt 运行库和资源部署到 i.MX6ULL Linux 系统。
5. 运行前确认以下条件：
   - `onenetapiclient.cpp` 中的 OneNET 参数正确
   - 目标板具备可用 HTTPS/SSL 运行环境
   - 系统时间跟现实时间一致，否则无法接收云端数据
   - 摄像头节点可用，默认是 `/dev/video2`
   - 若需亮度调节，`/sys/class/backlight` 下存在可写节点

当前 qmake 工程依赖：

- `QT += core gui network`
- `greaterThan(QT_MAJOR_VERSION, 4): QT += widgets`
- `CONFIG += c++11`

---

## 项目目录

```text
family-secure-system/
├── README.md
├── stm32/
│   ├── User/                      # 主程序、Keil 工程、中断与配置
│   ├── System/                    # DHT11/MQ2/SR501/OLED/USART 驱动
│   ├── NET/                       # ESP8266、OneNET、MQTT、签名算法、JSON
│   ├── Library/                   # STM32 标准外设库
│   └── Start/                     # 启动文件和系统初始化
└── imx6ull/
    └── system-ui/
        ├── 01_smarthome.pro       # qmake 工程
        ├── main.cpp               # Qt 程序入口
        ├── mainwindow.cpp/.h      # 主界面与业务逻辑
        ├── onenetapiclient.cpp/.h # OneNET HTTP 轮询客户端
        ├── v4l2camera.cpp/.h      # V4L2 摄像头采集
        ├── dashboardpage.cpp/.h   # 首页
        ├── videopage.cpp/.h       # 视频页
        ├── historypage.cpp/.h     # 历史页
        ├── settingspage.cpp/.h    # 设置页
        ├── lockpage.cpp/.h        # 锁屏页
        ├── smokegaugewidget.cpp/.h
        ├── trendchartwidget.cpp/.h
        ├── securityuimodels.h
        ├── securityuiutils.cpp/.h
        ├── style.qss
        └── res.qrc
```

---

## STM32 端实现细节

### 主循环节拍

STM32 端主程序的运行流程如下：

1. 初始化 USART1、USART2、OLED、MQ-2、SR501，并循环等待 DHT11 初始化成功。
2. 初始化 ESP8266，接入 WiFi，建立到 `mqtts.heclouds.com:1883` 的 TCP 连接。
3. 完成 OneNET MQTT 鉴权建链，并订阅属性设置主题。
4. 主循环每 200ms 执行一次本地采集与 OLED 刷新。
5. 每累计 25 个周期执行一次属性上报，即每 5 秒上报一次。
6. 若 ESP8266 收到 `+IPD` 数据，则交给 OneNET/MQTT 解析流程处理。



### STM32 硬件引脚

| 模块 | 引脚 | 说明 |
|------|------|------|
| DHT11 Data | PB10 | 单总线数据引脚 |
| MQ-2 AO | PB1 | ADC1_CH9 模拟输入 |
| SR501 DO | PA0 | 数字输入 |
| OLED SCL | PB8 | 软件 I2C 时钟 |
| OLED SDA | PB9 | 软件 I2C 数据 |
| USART1 TX/RX | PA9 / PA10 | 调试串口 |
| USART2 TX/RX | PA2 / PA3 | ESP8266 通信串口 |

说明：实物连接时ESP8266 模块 TX/RX和USB-TTL 模块 TX/RX跟上述 USART 引脚交叉连接（TX接RX，RX接TX）
 
---

## i.MX6ULL Qt 端实现细节

### 界面与交互

- 固定目标分辨率为 `800x480`。
- 使用 Qt Widgets + qmake。
- 顶层结构为“锁屏页 + 主壳页”。
- 主壳页内部切换首页、视频、历史、设置四个业务页面。
- 全局样式通过 `style.qss` 加载。

### 云端数据获取逻辑

- OneNET Host：`https://iot-api.heclouds.com`
- 查询接口：`/thingmodel/query-device-property`
- 轮询周期：2 秒
- 单次请求超时：4 秒
- 若设备最近属性上报时间超过 20 秒，则判定为设备离线
- 若 UI 超过 5 秒没有拿到新的传感器快照，则首页卡片进入离线遮罩态

说明：当前代码区分了“云端请求成功”和“设备实际上报仍然新鲜”两个层次，只有属性上报时间未超时才会显示在线。

Qt 端会把平台属性归一化到统一字段，兼容以下人体检测命名：

- `sr501`
- `pir`
- `human`
- `human_detect`

### 摄像头与亮度

- 默认采集设备：/dev/video2
- 默认分辨率：640x480
- 摄像头输出像素格式：MJPEG
- 抓帧频率：30fps
- 亮度调节通过扫描 /sys/class/backlight 下可写节点实现，仅 Linux 有效

### 本地配置和默认值

UI 当前默认值如下：

- 默认锁屏密码：123456
- 默认温度阈值：35 摄氏度
- 默认烟雾阈值：1800 raw
- 本地配置文件：程序工作目录下的 security_ui.ini

---


## 当前限制与注意事项

- STM32 端已订阅属性设置主题，但还没有执行实际的下行控制动作。
- i.MX6ULL 端历史数据只保存在内存，不会持久化到文件或数据库。
- 视频采集和亮度调节依赖 Linux 设备节点，在 Windows 上可以做桌面编译验证，但对应功能不会生效。
- Qt 端 OneNET HTTP Token 带有效期，到期后需要手动更新。
- 若目标板缺少 SSL 支持或根证书，OneNET HTTPS 请求会直接失败。

---

## 开发工具

- STM32：Keil MDK-ARM + STM32 标准外设库
- UI：Qt 5.12.9 QWidgets + qmake
- 通信：ESP8266 AT、MQTT、OneNET HTTP API

---

## 许可证

本项目仅用于学习参考，不可用于其他用途。
