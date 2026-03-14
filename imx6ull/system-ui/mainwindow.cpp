#include "mainwindow.h"

#include "dashboardpage.h"
#include "historypage.h"
#include "lockpage.h"
#include "onenetapiclient.h"
#include "securityuiutils.h"
#include "settingspage.h"
#include "videopage.h"
#include "v4l2camera.h"

#include <QApplication>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkInterface>
#include <QPushButton>
#include <QFile>
#include <QFont>
#include <QSettings>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QStyle>
#include <QTextStream>
#include <QVBoxLayout>

/*
 * 模块说明：主窗口实现。
 * 界面骨架：报警条、左侧导航、顶部状态栏、主内容页栈和底部状态栏都在这里组装。
 */

namespace {

/*
 * 刷新控件样式状态。
 * 说明：当动态属性变化后，通过 unpolish/polish 让 QSS 重新生效。
 */
void refreshWidgetStyle(QWidget *widget)
{
    if (widget == nullptr) {
        return;
    }

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}


} // namespace

/*
 * 构造主窗口并完成页面初始化。
 * 运行流程：加载本地配置、构建 UI、绑定信号槽、启动云端轮询和本地视频采集。
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      rootWidget(new QWidget(this)),
      contentStack(new QStackedWidget(rootWidget)),
    lockPage(new LockPage(contentStack)),
      shellPage(new QWidget(contentStack)),
      alarmBanner(new QFrame(shellPage)),
      alarmBannerLabel(new QLabel(alarmBanner)),
    navDashboardButton(new QPushButton(QStringLiteral("🏠\n首页"), shellPage)),
    navVideoButton(new QPushButton(QStringLiteral("📹\n视频"), shellPage)),
    navHistoryButton(new QPushButton(QStringLiteral("📊\n历史"), shellPage)),
    navSettingsButton(new QPushButton(QStringLiteral("⚙\n设置"), shellPage)),
      topTitleLabel(new QLabel(QStringLiteral("家庭安防系统"), shellPage)),
      topCloudLabel(new QLabel(QStringLiteral("云端：离线"), shellPage)),
      mainPageStack(new QStackedWidget(shellPage)),
    dashboardPage(new DashboardPage(mainPageStack)),
            videoPage(new VideoPage(mainPageStack)),
            historyPage(new HistoryPage(mainPageStack)),
            settingsPage(new SettingsPage(mainPageStack)),
      bottomTimeLabel(new QLabel(QStringLiteral("--"), shellPage)),
            bottomNetworkLabel(new QLabel(QStringLiteral("📶 网络：离线"), shellPage)),
      bottomSafetyLamp(new QLabel(shellPage)),
      bottomSafetyLabel(new QLabel(QStringLiteral("系统安全：正常"), shellPage)),
            oneNetApiClient(new OneNetApiClient(this)),
      camera(new V4l2Camera(this)),
      lockPassword(QStringLiteral("123456")),
            tempThresholdC(35),
      smokeThresholdRaw(1800),
      cloudOnline(false),
            tempDanger(false),
      smokeDanger(false),
      pirDetected(false),
      blinkFlag(false),
            cameraCaptureActive(false),
            historyWindowHours(1),
            backlightMaxValue(0)
{
        /* 固定课程设计目标分辨率，避免布局在运行时被系统拉伸 */
    setFixedSize(800, 480);
    setWindowTitle(QStringLiteral("家庭安防系统"));

        /* 先加载持久化配置，再构建 UI，确保默认值正确回填到控件 */
    loadConfig();
    buildUi();

        /* 首次显示前执行一次自适配，统一各页面间距/字号 */
    updateAdaptiveSpacing();

        /* 绑定导航与设置类交互 */
    connect(navDashboardButton, SIGNAL(clicked()), this, SLOT(onNavDashboard()));
    connect(navVideoButton, SIGNAL(clicked()), this, SLOT(onNavVideo()));
    connect(navHistoryButton, SIGNAL(clicked()), this, SLOT(onNavHistory()));
    connect(navSettingsButton, SIGNAL(clicked()), this, SLOT(onNavSettings()));
    connect(lockPage, SIGNAL(confirmRequested(QString)), this, SLOT(onLockConfirmRequested(QString)));
    connect(settingsPage, SIGNAL(applyThresholdRequested()), this, SLOT(onApplyThresholdClicked()));
    connect(settingsPage, SIGNAL(changePasswordRequested()), this, SLOT(onChangePasswordClicked()));
    connect(videoPage, SIGNAL(captureRequested()), this, SLOT(onCaptureClicked()));
    connect(videoPage, SIGNAL(startStopRequested()), this, SLOT(onStartCameraClicked()));
    connect(settingsPage, SIGNAL(brightnessChanged(int)), this, SLOT(onBrightnessChanged(int)));
    connect(historyPage, SIGNAL(window1hRequested()), this, SLOT(onRange1hClicked()));
    connect(historyPage, SIGNAL(window24hRequested()), this, SLOT(onRange24hClicked()));

    /* 绑定外部数据源：OneNet API 轮询 + 摄像头帧 */
    connect(oneNetApiClient, SIGNAL(connectedChanged(bool)), this, SLOT(onCloudConnectedChanged(bool)));
    connect(oneNetApiClient, SIGNAL(sensorJsonReceived(QString)), this, SLOT(onSensorJsonReceived(QString)));
    connect(oneNetApiClient, SIGNAL(errorOccurred(QString)), this, SLOT(onApiError(QString)));
    connect(camera, SIGNAL(errorOccurred(QString)), this, SLOT(onCameraError(QString)));
    connect(camera, SIGNAL(frameReady(QImage)), this, SLOT(onFrameReady(QImage)));

    /* UI 心跳：统一驱动时钟、离线检测和闪烁效果 */
    connect(&uiTickTimer, SIGNAL(timeout()), this, SLOT(onUiTick()));
    uiTickTimer.start(500);

    /* OneNet 参数保留代码硬编码，设置页只展示脱敏后的摘要。 */
    settingsPage->setThresholdValues(tempThresholdC, smokeThresholdRaw);
    settingsPage->setCloudApiInfoText(oneNetApiClient->runtimeInfoText());
    oneNetApiClient->start();

    /* 初次进入时刷新网络信息和历史窗口 */
    refreshNetworkInfo();
    historyPage->setWindowHours(historyWindowHours);
    refreshHistoryChartsByWindow(historyWindowHours);
}

/*
 * 析构主窗口。
 * 说明：在窗口销毁前显式停止摄像头和 OneNet 轮询，避免资源残留。
 */
MainWindow::~MainWindow()
{
    /* 析构时显式停止外设/子进程，避免资源泄露 */
    stopCameraCapture();
    oneNetApiClient->stop();
}

/* 构建顶层容器：仅负责挂载锁屏页与主壳页 */
void MainWindow::buildUi()
{
    rootWidget->setObjectName(QStringLiteral("Root"));
    QVBoxLayout *rootLayout = new QVBoxLayout(rootWidget);
    rootLayout->setObjectName(QStringLiteral("RootLayout"));
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(contentStack);

    buildShellPage();

    contentStack->addWidget(lockPage);
    contentStack->addWidget(shellPage);
    contentStack->setCurrentWidget(lockPage);

    setCentralWidget(rootWidget);
}

/* 主壳页：报警条 + 左导航 + 顶栏 + 主内容 + 底栏 */
void MainWindow::buildShellPage()
{
    shellPage->setObjectName(QStringLiteral("ShellPage"));

    QVBoxLayout *shellRoot = new QVBoxLayout(shellPage);
    shellRoot->setObjectName(QStringLiteral("ShellRootLayout"));
    shellRoot->setContentsMargins(0, 0, 0, 0);
    shellRoot->setSpacing(0);

    /* 顶部报警条：危险提示或关键反馈时显示。 */
    alarmBanner->setObjectName(QStringLiteral("AlarmBanner"));
    alarmBanner->setFixedHeight(36);
    QHBoxLayout *bannerLayout = new QHBoxLayout(alarmBanner);
    bannerLayout->setObjectName(QStringLiteral("BannerLayout"));
    bannerLayout->setContentsMargins(12, 0, 12, 0);
    bannerLayout->addWidget(alarmBannerLabel);
    alarmBannerLabel->setObjectName(QStringLiteral("AlarmBannerText"));
    alarmBannerLabel->setText(QStringLiteral("系统正常"));
    alarmBanner->setVisible(false);

    /* 主体区域：左侧导航栏，右侧业务内容区。 */
    QWidget *body = new QWidget(shellPage);
    QHBoxLayout *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setObjectName(QStringLiteral("BodyLayout"));
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    /* 左侧导航栏：四个主页面入口纵向均分。 */
    QFrame *navBar = new QFrame(body);
    navBar->setObjectName(QStringLiteral("NavBar"));
    navBar->setFixedWidth(68);
    QVBoxLayout *navLayout = new QVBoxLayout(navBar);
    navLayout->setObjectName(QStringLiteral("NavLayout"));
    navLayout->setContentsMargins(8, 10, 8, 10);
    navLayout->setSpacing(10);

    navDashboardButton->setObjectName(QStringLiteral("NavButton"));
    navVideoButton->setObjectName(QStringLiteral("NavButton"));
    navHistoryButton->setObjectName(QStringLiteral("NavButton"));
    navSettingsButton->setObjectName(QStringLiteral("NavButton"));

    /* 导航按钮通过 stretch 均匀分布，避免按钮全部挤在顶部。 */
    navLayout->addStretch(1);
    navLayout->addWidget(navDashboardButton);
    navLayout->addStretch(1);
    navLayout->addWidget(navVideoButton);
    navLayout->addStretch(1);
    navLayout->addWidget(navHistoryButton);
    navLayout->addStretch(1);
    navLayout->addWidget(navSettingsButton);
    navLayout->addStretch(1);

    /* 右侧业务区：顶栏、页面栈和底栏三段结构。 */
    QWidget *right = new QWidget(body);
    QVBoxLayout *rightLayout = new QVBoxLayout(right);
    rightLayout->setObjectName(QStringLiteral("RightLayout"));
    rightLayout->setContentsMargins(12, 10, 12, 10);
    rightLayout->setSpacing(10);

    /* 顶栏：显示系统标题和云端状态。 */
    QFrame *topBar = new QFrame(right);
    topBar->setObjectName(QStringLiteral("TopBar"));
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setObjectName(QStringLiteral("TopLayout"));
    topLayout->setContentsMargins(12, 6, 12, 6);
    topTitleLabel->setObjectName(QStringLiteral("TitleText"));
    topCloudLabel->setObjectName(QStringLiteral("TopStatusText"));
    topLayout->addWidget(topTitleLabel);
    topLayout->addStretch();
    topLayout->addWidget(topCloudLabel);

    /* 主页面栈：四个业务页面均已拆成独立页面类。 */

    mainPageStack->addWidget(dashboardPage);
    mainPageStack->addWidget(videoPage);
    mainPageStack->addWidget(historyPage);
    mainPageStack->addWidget(settingsPage);

    /* 底栏：显示时钟、网络状态和综合安全指示。 */
    QFrame *bottomBar = new QFrame(right);
    bottomBar->setObjectName(QStringLiteral("BottomBar"));
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setObjectName(QStringLiteral("BottomLayout"));
    bottomLayout->setContentsMargins(12, 4, 12, 4);
    bottomLayout->addWidget(bottomTimeLabel);
    bottomLayout->addStretch();
    bottomLayout->addWidget(bottomNetworkLabel);

    bottomSafetyLamp->setObjectName(QStringLiteral("SafetyLamp"));
    bottomSafetyLamp->setFixedSize(12, 12);
    bottomLayout->addWidget(bottomSafetyLamp);
    bottomLayout->addWidget(bottomSafetyLabel);

    rightLayout->addWidget(topBar);
    rightLayout->addWidget(mainPageStack, 1);
    rightLayout->addWidget(bottomBar);

    bodyLayout->addWidget(navBar);
    bodyLayout->addWidget(right, 1);

    shellRoot->addWidget(alarmBanner);
    shellRoot->addWidget(body, 1);

    updateNavState(0);
}

/* 校验密码：成功进入主壳，失败触发错误动画 */
void MainWindow::onLockConfirmRequested(const QString &pin)
{
    if (pin.size() < 4) {
        setLockError(QStringLiteral("密码长度至少4位"));
        return;
    }

    if (pin == lockPassword) {
        contentStack->setCurrentWidget(shellPage);
        lockPage->resetInput();
        lockPage->setHintText(QStringLiteral("请输入 4-6 位密码"));
        return;
    }

    setLockError(QStringLiteral("密码错误，请重试"));
}

/* 导航：首页 */
void MainWindow::onNavDashboard()
{
    mainPageStack->setCurrentWidget(dashboardPage);
    updateNavState(0);
}

/* 导航：视频 */
void MainWindow::onNavVideo()
{
    mainPageStack->setCurrentWidget(videoPage);
    updateNavState(1);
}

/* 导航：历史 */
void MainWindow::onNavHistory()
{
    mainPageStack->setCurrentWidget(historyPage);
    updateNavState(2);
}

/* 导航：设置 */
void MainWindow::onNavSettings()
{
    mainPageStack->setCurrentWidget(settingsPage);
    updateNavState(3);
}

/* 云连接状态变化：同步顶部、底部与系统状态文案 */
void MainWindow::onCloudConnectedChanged(bool online)
{
    cloudOnline = online;
    topCloudLabel->setText(SecurityUiUtils::cloudStateText(online));
    dashboardPage->setOneNetStateText(SecurityUiUtils::oneNetStateText(online));
    bottomNetworkLabel->setText(SecurityUiUtils::bottomNetworkStateText(online));
    updateGlobalSafetyState();
}

bool MainWindow::parseSensorSnapshot(const QString &jsonText, SensorSnapshot *snapshot) const
{
    if (snapshot == nullptr) {
        return false;
    }

    /* JSON 非对象则忽略，避免脏数据导致界面闪烁。 */
    const QJsonDocument document = QJsonDocument::fromJson(jsonText.toUtf8());
    if (!document.isObject()) {
        return false;
    }

    const QJsonObject object = document.object();
    const QJsonObject params = object.value(QStringLiteral("params")).toObject();

    snapshot->tempC = qRound(SecurityUiUtils::readSensorNumberField(object, params, QStringLiteral("temp")));
    snapshot->humiPercent = qRound(SecurityUiUtils::readSensorNumberField(object, params, QStringLiteral("humi")));
    snapshot->smokeRaw = qRound(SecurityUiUtils::readSensorNumberField(object, params, QStringLiteral("mq2")));
    snapshot->smokePpm = SecurityUiUtils::readSensorNumberField(object, params, QStringLiteral("mq2_ppm"));

    /* 人体字段兼容多种上报命名，统一折叠成一个布尔状态。 */
    const double pirValue = SecurityUiUtils::readFirstAvailableSensorNumber(object,
                                                                            params,
                                                                            QStringList()
                                                                            << QStringLiteral("sr501")
                                                                            << QStringLiteral("pir")
                                                                            << QStringLiteral("human")
                                                                            << QStringLiteral("human_detect"),
                                                                            0.0);
    snapshot->pirDetected = (qRound(pirValue) != 0);
    return true;
}

/*
 * 应用传感器快照到界面状态。
 * 运行流程：刷新实时值、更新阈值判定、同步 PIR 状态，并把样本写入历史缓存。
 */
void MainWindow::applySensorSnapshot(const SensorSnapshot &snapshot, const QDateTime &receivedAt)
{
    lastSensorUpdate = receivedAt;
    updateOfflineMasks(false);

    /* 页面显示直接采用云端上报的整度/整湿值，保持与物模型步长一致。 */
    dashboardPage->setTemperatureHumidity(snapshot.tempC, snapshot.humiPercent);

    const int smokePercent = qBound(0, (snapshot.smokeRaw * 100) / 4095, 100);
    dashboardPage->setSmokeReading(snapshot.smokeRaw, snapshot.smokePpm, smokePercent);

    /* 温度阈值直接使用云端上报的摄氏度值判定，避免和历史图内部缩放量纲混用。 */
    updateThresholdState(snapshot.tempC, snapshot.smokeRaw);
    updatePirDetectionState(snapshot.pirDetected, receivedAt);
    dashboardPage->setWifiStateText(SecurityUiUtils::wifiStateText(cloudOnline));

    /* 追加历史样本并更新综合安全状态。 */
    appendHistory(snapshot.tempC, snapshot.humiPercent, snapshot.smokeRaw);
    updateGlobalSafetyState();
}

/*
 * 更新 PIR 检测状态。
 * 说明：统一处理首页人体状态文本和最后一次检测时间，减少散落分支。
 */
void MainWindow::updatePirDetectionState(bool detected, const QDateTime &receivedAt)
{
    pirDetected = detected;
    dashboardPage->setPirStateText(SecurityUiUtils::pirStateText(pirDetected));

    if (pirDetected) {
        lastPirSeen = receivedAt;
        dashboardPage->setPirLastSeenText(QStringLiteral("最后检测：%1")
                                          .arg(lastPirSeen.toString(QStringLiteral("hh:mm:ss"))));
    }
}

/* 处理 STM32 上传 JSON：解析、刷新 UI、更新历史与告警状态 */
void MainWindow::onSensorJsonReceived(const QString &jsonText)
{
    SensorSnapshot snapshot;
    if (!parseSensorSnapshot(jsonText, &snapshot)) {
        return;
    }

    applySensorSnapshot(snapshot, QDateTime::currentDateTime());
}

/*
 * 处理云端接口错误。
 * 说明：仅在当前没有温度、烟雾和人体风险时隐藏报警条，避免错误覆盖真实告警。
 */
void MainWindow::onApiError(const QString &message)
{
    Q_UNUSED(message)
    if (!tempDanger && !smokeDanger && !pirDetected) {
        alarmBanner->setVisible(false);
    }
}

/*
 * 视频帧到达：缩放并渲染到显示标签。
 * 运行流程：
 * 1. 若当前帧为空，直接把视频状态切到“无信号”。
 * 2. 保存原始帧，供截图功能直接复用，不依赖界面上的缩略显示。
 * 3. 按当前视频显示区大小等比缩放后再显示，避免画面拉伸变形。
 */
void MainWindow::onFrameReady(const QImage &image)
{
    if (image.isNull()) {
        videoPage->setVideoStateText(QStringLiteral("视频状态：无信号"));
        return;
    }

    lastFrameImage = image;
    videoPage->showFrame(image);
    videoPage->setVideoStateText(QStringLiteral("视频状态：正常"));
}

/*
 * 摄像头错误回调：把底层 V4L2 错误显式反馈到视频页。
 * 说明：底层启动失败或采集中断时，不再停留在“等待”状态，而是直接显示错误原因。
 */
void MainWindow::onCameraError(const QString &message)
{
    cameraCaptureActive = false;
    videoPage->setCaptureRunning(false);
    lastFrameImage = QImage();
    videoPage->showEmptyFrame(QStringLiteral("摄像头未连接"));
    videoPage->setVideoStateText(QStringLiteral("视频状态：错误"));
    setAlarmBanner(QStringLiteral("摄像头启动失败：%1").arg(message), true);
}

/*
 * 视频页启动按钮回调。
 * 说明：只有用户主动点击“启动”按钮时，才开始建立摄像头采集链路。
 */
void MainWindow::onStartCameraClicked()
{
    if (cameraCaptureActive) {
        stopCameraCapture();
        return;
    }

    startCameraCapture();
}

/*
 * 启动摄像头采集。
 * 说明：统一在这里完成状态切换和底层摄像头启动，避免按钮槽里散落重复逻辑。
 */
void MainWindow::startCameraCapture()
{
    videoPage->setVideoStateText(QStringLiteral("视频状态：连接中"));

    if (!camera->start(QStringLiteral("/dev/video2"), 640, 480)) {
        videoPage->setVideoStateText(QStringLiteral("视频状态：启动失败"));
        return;
    }

    cameraCaptureActive = true;
    videoPage->setCaptureRunning(true);
    lastFrameImage = QImage();
    videoPage->showEmptyFrame(QStringLiteral("摄像头未连接"));
    videoPage->setVideoStateText(QStringLiteral("视频状态：等待"));
}

/*
 * 停止摄像头采集。
 * 说明：统一关闭底层采集、清空缓存，并把按钮和视频页恢复到待启动状态。
 */
void MainWindow::stopCameraCapture()
{
    camera->stop();
    cameraCaptureActive = false;
    videoPage->setCaptureRunning(false);
    lastFrameImage = QImage();
    videoPage->showEmptyFrame(QStringLiteral("摄像头未连接"));
    videoPage->setVideoStateText(QStringLiteral("视频状态：已停止"));
}

/*
 * 应用阈值设置并持久化。
 * 运行流程：
 * 1. 先从两个输入框读取整度温度阈值和烟雾阈值。
 * 2. 只有两个值都成功转换成整数时才更新运行态。
 * 3. 保存到本地 ini 后立即生效，后续告警判断会直接使用新阈值。
 */
void MainWindow::onApplyThresholdClicked()
{
    bool okTemp = false;
    bool okSmoke = false;
    const int tempValue = settingsPage->tempThresholdValue(&okTemp);
    const int smokeValue = settingsPage->smokeThresholdValue(&okSmoke);

    if (!okTemp || !okSmoke) {
        setAlarmBanner(QStringLiteral("阈值输入无效"), true);
        return;
    }

    tempThresholdC = tempValue;
    smokeThresholdRaw = smokeValue;
    saveConfig();

    if (!historySamples.isEmpty()) {
        const HistorySample &latest = historySamples.last();
        updateThresholdState(latest.tempC, latest.smokeRaw);
        updateGlobalSafetyState();
    }

    if (!tempDanger && !smokeDanger) {
        setAlarmBanner(QStringLiteral("阈值已保存并生效"), false);
    }
}

/*
 * 修改开机密码并持久化。
 * 运行流程：
 * 1. 先校验旧密码，防止未授权修改。
 * 2. 再校验新密码和确认密码是否一致，避免写入错误配置。
 * 3. 保存成功后清空输入框，防止密码继续停留在界面上。
 */
void MainWindow::onChangePasswordClicked()
{
    if (settingsPage->oldPassword() != lockPassword) {
        setAlarmBanner(QStringLiteral("旧密码错误"), true);
        return;
    }

    const QString newPassword = settingsPage->newPassword();
    if (newPassword != settingsPage->confirmPassword()) {
        setAlarmBanner(QStringLiteral("新密码不一致"), true);
        return;
    }

    if (!SecurityUiUtils::isValidLockPassword(newPassword)) {
        setAlarmBanner(QStringLiteral("密码必须为4-6位数字"), true);
        return;
    }

    lockPassword = newPassword;
    settingsPage->clearPasswordInputs();
    saveConfig();
    setAlarmBanner(QStringLiteral("开机密码修改成功"), false);
}

/*
 * 保存截图到系统图片目录（无目录则回退当前目录）。
 * 运行流程：
 * 1. 仅当最近一次摄像头帧有效时才允许截图。
 * 2. 优先使用系统图片目录，若运行环境未配置该目录则退回当前工作目录。
 * 3. 按时间戳生成唯一文件名，避免重复截图互相覆盖。
 */
void MainWindow::onCaptureClicked()
{
    if (lastFrameImage.isNull()) {
        setAlarmBanner(QStringLiteral("当前无可用视频帧"), false);
        return;
    }

    QString basePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (basePath.isEmpty()) {
        basePath = QDir::currentPath();
    }

    const QString targetDir = basePath + QStringLiteral("/family-security-captures");
    QDir().mkpath(targetDir);
    const QString fileName = QStringLiteral("%1/cap_%2.jpg")
            .arg(targetDir)
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));

    if (lastFrameImage.save(fileName, "JPG")) {
        setAlarmBanner(QStringLiteral("截图已保存：%1").arg(fileName), false);
    } else {
        setAlarmBanner(QStringLiteral("截图保存失败：请检查目录权限"), true);
    }
}

/*
 * 亮度滑杆回调：尝试写背光节点。
 * 说明：滑杆只负责把百分比传给底层背光写入函数，真正的节点发现和数值换算都在 applyBrightnessToSystem() 中完成。
 */
void MainWindow::onBrightnessChanged(int value)
{
    /* 亮度控制：优先尝试写入 /sys/class/backlight */
    if (applyBrightnessToSystem(value)) {
        setAlarmBanner(QStringLiteral("亮度已调整：%1%").arg(value), false);
    } else {
        setAlarmBanner(QStringLiteral("亮度设置失败：未找到可写背光节点"), true);
    }
}

/*
 * 设置当前历史窗口范围。
 * 说明：统一处理窗口归一化、按钮勾选状态和图表刷新，避免 1h/24h 两个入口重复代码。
 */
void MainWindow::setHistoryWindow(int hours)
{
    historyWindowHours = (hours <= 1) ? 1 : 24;
    historyPage->setWindowHours(historyWindowHours);
    refreshHistoryChartsByWindow(historyWindowHours);
}

/* 历史窗口切换：1小时 */
void MainWindow::onRange1hClicked()
{
    setHistoryWindow(1);
    setAlarmBanner(QStringLiteral("历史窗口切换到最近1小时"), false);
}

/* 历史窗口切换：24小时 */
void MainWindow::onRange24hClicked()
{
    setHistoryWindow(24);
    setAlarmBanner(QStringLiteral("历史窗口切换到最近24小时"), false);
}

/*
 * UI 心跳任务：时钟、离线判定、状态闪烁与网络刷新。
 * 运行流程：
 * 1. 每 500ms 翻转一次 blinkFlag，用于烟雾警示和人体状态闪烁。
 * 2. 同步更新时间和视频时间水印。
 * 3. 根据 lastSensorUpdate 判断传感器数据是否超过 5 秒未更新，并切换离线遮罩。
 * 4. 若传感器整体离线，则同步清除人体检测告警，避免旧状态长期停留。
 * 5. 每 2 秒刷新一次网络信息，最后统一刷新综合安全状态。
 */
void MainWindow::onUiTick()
{
    static int networkRefreshCounter = 0;

    /* 统一驱动需要闪烁效果的控件。 */
    blinkFlag = !blinkFlag;
    bottomTimeLabel->setText(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
    videoPage->setTimestampText(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));

    if (smokeDanger) {
        dashboardPage->setSmokeWarningVisible(blinkFlag);
    }

    /* 传感器数据超过 5 秒未刷新时，整组首页卡片切到离线态。 */
    bool sensorOffline = true;
    if (lastSensorUpdate.isValid()) {
        const qint64 staleMs = lastSensorUpdate.msecsTo(QDateTime::currentDateTime());
        sensorOffline = (staleMs > 5000);
    }
    updateOfflineMasks(sensorOffline);

    if (sensorOffline) {
        updatePirDetectionState(false, QDateTime());
    }

    dashboardPage->setOneNetStateText(SecurityUiUtils::oneNetStateText(cloudOnline));
    topCloudLabel->setText(SecurityUiUtils::cloudStateText(cloudOnline));

    /* 每 2 秒刷新一次网络信息，避免过于频繁 */
    networkRefreshCounter++;
    if (networkRefreshCounter >= 4) {
        networkRefreshCounter = 0;
        refreshNetworkInfo();
    }

    updateGlobalSafetyState();
}

/* 窗口尺寸变化时统一刷新各页面自适配参数 */
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateAdaptiveSpacing();
}

/*
 * 全局自适配策略：
 * 1) 基于窗口短边计算 spacing、边距、字号和按钮高度。
 * 2) 通过对象名找到各页面布局，避免保存大量布局指针。
 * 3) 统一缩放导航、键盘、图表和设置页间距，保证 800x480 及全屏时观感一致。
 */
void MainWindow::updateAdaptiveSpacing()
{
    const int pageMin = qMin(width(), height());
    const int spacing = qBound(6, pageMin / 60, 12);
    const int compactSpacing = qMax(4, spacing - 2);
    const int sideMargin = qBound(6, pageMin / 45, 14);
    const int titleFontPx = qBound(14, pageMin / 26, 18);
    const int statusFontPx = qBound(10, pageMin / 40, 12);
    const int smokeGaugeHeight = qBound(90, pageMin / 4, 110);
    const int navButtonHeight = qBound(36, pageMin / 11, 46);
    const int primaryButtonHeight = qBound(30, pageMin / 14, 38);
    const int keypadButtonHeight = qBound(34, pageMin / 12, 42);

    /* 按对象名设置布局间距，减少重复判空和查找代码。 */
    auto setSpacingByName = [](QWidget *scope, const QString &name, int value) {
        if (scope == nullptr) {
            return;
        }
        if (QLayout *layout = scope->findChild<QLayout *>(name)) {
            layout->setSpacing(value);
        }
    };
    /* 按对象名设置布局边距，统一控制各页面留白。 */
    auto setMarginsByName = [](QWidget *scope, const QString &name, int left, int top, int right, int bottom) {
        if (scope == nullptr) {
            return;
        }
        if (QLayout *layout = scope->findChild<QLayout *>(name)) {
            layout->setContentsMargins(left, top, right, bottom);
        }
    };
    /* 统一标签字号缩放，避免单独维护多个字体对象。 */
    auto setLabelFontPx = [](QLabel *label, int pixelSize) {
        if (label == nullptr) {
            return;
        }
        QFont font = label->font();
        font.setPixelSize(pixelSize);
        label->setFont(font);
    };
    /* 统一按钮高度缩放，保证不同页面的交互控件视觉节奏一致。 */
    auto setButtonHeight = [](QPushButton *button, int height) {
        if (button == nullptr) {
            return;
        }
        button->setFixedHeight(height);
    };

    /* 先调整顶部关键文本和烟雾仪表高度。 */
    setLabelFontPx(topTitleLabel, titleFontPx);
    setLabelFontPx(topCloudLabel, statusFontPx);

    setButtonHeight(navDashboardButton, navButtonHeight);
    setButtonHeight(navVideoButton, navButtonHeight);
    setButtonHeight(navHistoryButton, navButtonHeight);
    setButtonHeight(navSettingsButton, navButtonHeight);
    /* 锁屏页交由页面类自身处理键盘和卡片布局的自适配。 */
    lockPage->applyAdaptiveMetrics(spacing, compactSpacing, sideMargin, keypadButtonHeight);

    /* 主壳页：同步导航宽度、右侧区域间距和顶底栏边距。 */
    setSpacingByName(shellPage, QStringLiteral("NavLayout"), spacing);
    setSpacingByName(shellPage, QStringLiteral("RightLayout"), spacing);
    setMarginsByName(shellPage, QStringLiteral("RightLayout"), sideMargin, sideMargin, sideMargin, sideMargin);
    setMarginsByName(shellPage, QStringLiteral("TopLayout"), sideMargin, 4, sideMargin, 4);
    setMarginsByName(shellPage, QStringLiteral("BottomLayout"), sideMargin, 2, sideMargin, 2);
    if (QFrame *navBar = shellPage->findChild<QFrame *>(QStringLiteral("NavBar"))) {
        navBar->setFixedWidth(qBound(56, width() / 12, 74));
    }

    /* 首页由页面类统一处理四宫格间距和烟雾仪表高度。 */
    dashboardPage->applyAdaptiveMetrics(spacing, smokeGaugeHeight);

    /* 视频页由页面类统一处理按钮高度和布局间距。 */
    videoPage->applyAdaptiveMetrics(spacing, compactSpacing, primaryButtonHeight);

    /* 历史页和设置页交由页面类统一处理布局自适配。 */
    historyPage->applyAdaptiveMetrics(spacing, primaryButtonHeight);
    settingsPage->applyAdaptiveMetrics(spacing, primaryButtonHeight);
}

/* 根据当前页面下标刷新导航激活态样式 */
void MainWindow::updateNavState(int index)
{
    navDashboardButton->setProperty("active", index == 0);
    navVideoButton->setProperty("active", index == 1);
    navHistoryButton->setProperty("active", index == 2);
    navSettingsButton->setProperty("active", index == 3);

    const QList<QPushButton *> navButtons = QList<QPushButton *>()
            << navDashboardButton
            << navVideoButton
            << navHistoryButton
            << navSettingsButton;
    for (int i = 0; i < navButtons.size(); ++i) {
        refreshWidgetStyle(navButtons.at(i));
    }
}

/*
 * 汇总烟雾/人体/云状态，更新底部安全灯与报警条。
 * 判定优先级：烟雾危险 > 人体检测或云离线 > 正常。
 * 这样可以保证真正的危险状态不会被普通提示覆盖。
 */
void MainWindow::updateGlobalSafetyState()
{
    /* 最高优先级：烟雾超标时，底部安全状态和顶部报警条都进入危险态。 */
    if (smokeDanger) {
        bottomSafetyLabel->setText(QStringLiteral("系统安全：危险"));
        bottomSafetyLamp->setProperty("danger", true);
        bottomSafetyLamp->setProperty("warn", false);
        setAlarmBanner(QStringLiteral("烟雾报警中"), true);
    } else if (tempDanger) {
        bottomSafetyLabel->setText(QStringLiteral("系统安全：危险"));
        bottomSafetyLamp->setProperty("danger", true);
        bottomSafetyLamp->setProperty("warn", false);
        setAlarmBanner(QStringLiteral("温度超出阈值，请检查环境温度"), true);
    /* 次优先级：人体检测或云离线时进入注意态。 */
    } else if (pirDetected || !cloudOnline) {
        bottomSafetyLabel->setText(QStringLiteral("系统安全：注意"));
        bottomSafetyLamp->setProperty("danger", false);
        bottomSafetyLamp->setProperty("warn", true);
        bottomSafetyLamp->setVisible(!pirDetected || blinkFlag);
        /* 人体检测时保留提示；若只是云离线，则不长期占用报警条。 */
        if (pirDetected) {
            setAlarmBanner(QStringLiteral("门口检测到人体"), false);
        } else {
            alarmBanner->setVisible(false);
        }
    } else {
        /* 所有风险都解除后恢复常态显示。 */
        bottomSafetyLabel->setText(QStringLiteral("系统安全：正常"));
        bottomSafetyLamp->setProperty("danger", false);
        bottomSafetyLamp->setProperty("warn", false);
        bottomSafetyLamp->setVisible(true);
        alarmBanner->setVisible(false);
    }

    refreshWidgetStyle(bottomSafetyLamp);
}

/* 根据最新样本刷新温度和烟雾阈值状态，避免阈值保存后还要等下一帧才生效。 */
void MainWindow::updateThresholdState(int tempC, int smokeRaw)
{
    /* 温度阈值按摄氏度整数直接比较，和云端物模型步长保持一致。 */
    tempDanger = (tempC >= tempThresholdC);
    smokeDanger = (smokeRaw >= smokeThresholdRaw);

    dashboardPage->setTemperatureSourceText(SecurityUiUtils::tempSourceText(tempDanger));

    if (smokeDanger) {
        dashboardPage->setSmokeLevelText(QStringLiteral("危险"));
        dashboardPage->setSmokeWarningVisible(blinkFlag);
    } else if (smokeRaw > (smokeThresholdRaw * 7 / 10)) {
        dashboardPage->setSmokeLevelText(QStringLiteral("注意"));
        dashboardPage->setSmokeWarningVisible(false);
    } else {
        dashboardPage->setSmokeLevelText(QStringLiteral("安全"));
        dashboardPage->setSmokeWarningVisible(false);
    }
}

/* 设置报警条文案与危险态样式 */
void MainWindow::setAlarmBanner(const QString &text, bool danger)
{
    alarmBannerLabel->setText(text);
    alarmBanner->setProperty("danger", danger);
    alarmBanner->setVisible(true);
    refreshWidgetStyle(alarmBanner);
}

/*
 * 锁屏错误提示与抖动动画。
 * 运行流程：
 * 1. 先切换提示文案和错误样式。
 * 2. 再对整个锁屏页做一个短时左右抖动，模拟密码错误反馈。
 * 3. 最后清空当前输入，避免错误密码残留在圆点状态中。
 */
void MainWindow::setLockError(const QString &message)
{
    lockPage->setHintText(message, true);
    lockPage->playErrorAnimation();
    lockPage->resetInput();
}

/*
 * 传感器离线遮罩控制。
 * 说明：离线时保留卡片骨架，只覆盖“离线”遮罩并隐藏烟雾仪表，避免用户误把旧数据当成实时值。
 */
void MainWindow::updateOfflineMasks(bool sensorOffline)
{
    dashboardPage->setOfflineState(sensorOffline);
}

/*
 * 写入历史缓存并做窗口外数据裁剪。
 * 运行流程：
 * 1. 为当前一组温湿度/烟雾值打上时间戳后写入样本队列。
 * 2. 只保留最近 48 小时的数据，控制内存占用上限。
 * 3. 追加样本后立即按当前时间窗口重建图表。
 */
void MainWindow::appendHistory(int tempC, int humiPercent, int smokeRaw)
{
    /* 保存带时间戳样本，用于 1h/24h 窗口过滤 */
    HistorySample sample;
    sample.timestamp = QDateTime::currentDateTime();
    sample.tempC = tempC;
    sample.humiPercent = humiPercent;
    sample.smokeRaw = smokeRaw;
    historySamples.append(sample);

    /* 仅保留最近 48 小时的数据，避免内存持续增长 */
    const QDateTime threshold = QDateTime::currentDateTime().addSecs(-48 * 3600);
    while (!historySamples.isEmpty() && historySamples.first().timestamp < threshold) {
        historySamples.removeFirst();
    }

    refreshHistoryChartsByWindow(historyWindowHours);
}

/*
 * 按时间窗口重建曲线数据：过滤样本后分别刷新温湿度图和烟雾图。
 * 说明：左侧图表同时展示温度和湿度两条曲线，右侧图表展示烟雾原始值曲线。
 */
void MainWindow::refreshHistoryChartsByWindow(int hours)
{
    QVector<int> tempValues;
    QVector<int> humiValues;
    QVector<int> smokeValues;

    /* 先根据窗口时长算出样本筛选起点。 */
    const QDateTime fromTime = QDateTime::currentDateTime().addSecs(-hours * 3600);

    /* 只收集窗口内样本，窗口外历史保留在缓存中但不参与本次绘图。 */
    for (int i = 0; i < historySamples.size(); ++i) {
        const HistorySample &sample = historySamples.at(i);
        if (sample.timestamp < fromTime) {
            continue;
        }

        tempValues.append(sample.tempC);
        humiValues.append(sample.humiPercent);
        smokeValues.append(sample.smokeRaw);
    }

    const int maxPoints = (hours <= 1) ? 120 : 300;
    const QStringList axisLabels = SecurityUiUtils::historyAxisLabels(hours);

    historyPage->setTempHumiSeries(QVector<TrendChartWidget::Series>()
                                   << TrendChartWidget::Series(tempValues, QColor(QStringLiteral("#38bdf8")), QStringLiteral("温度"))
                                   << TrendChartWidget::Series(humiValues, QColor(QStringLiteral("#34d399")), QStringLiteral("湿度")),
                                   axisLabels,
                                   maxPoints);

    historyPage->setSmokeSeries(QVector<TrendChartWidget::Series>()
                                << TrendChartWidget::Series(smokeValues, QColor(QStringLiteral("#f59e0b")), QStringLiteral("烟雾")),
                                axisLabels,
                                maxPoints);
}

/*
 * 刷新网络信息（接口名/IP/信号）。
 * 运行流程：
 * 1. 遍历本机网络接口，跳过未启用、未运行和回环接口。
 * 2. 在可用接口中寻找 IPv4 地址，找到后即停止继续扫描。
 * 3. 若当前未接入真实无线信号采集，则明确显示“未提供”，避免继续展示演示值。
 */
void MainWindow::refreshNetworkInfo()
{
    QString interfaceName = QStringLiteral("--");
    QString ipAddress = QStringLiteral("--");

    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (int i = 0; i < interfaces.size(); ++i) {
        const QNetworkInterface &iface = interfaces.at(i);
        if (!iface.flags().testFlag(QNetworkInterface::IsUp) ||
            !iface.flags().testFlag(QNetworkInterface::IsRunning) ||
            iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }

        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (int j = 0; j < entries.size(); ++j) {
            const QHostAddress addr = entries.at(j).ip();
            if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
                interfaceName = iface.humanReadableName();
                ipAddress = addr.toString();
                break;
            }
        }

        if (ipAddress != QStringLiteral("--")) {
            break;
        }
    }

    /* 当前界面没有接入真实无线信号采集时，明确标记未提供，避免继续显示演示值。 */
    const QString signalText = (ipAddress == QStringLiteral("--"))
            ? QStringLiteral("--")
            : QStringLiteral("未提供");

    settingsPage->setWifiInfoText(QStringLiteral("WiFi：%1\nIP：%2\n信号：%3")
                                  .arg(interfaceName)
                                  .arg(ipAddress)
                                  .arg(signalText));
}

/*
 * 写系统背光节点实现亮度调整（仅 Linux）。
 * 运行流程：
 * 1. 首次调用时扫描 /sys/class/backlight，找到可读 max_brightness 且可写 brightness 的节点。
 * 2. 读取最大亮度后缓存节点路径和上限值，后续调用直接复用，避免重复扫描文件系统。
 * 3. 把 UI 百分比换算成背光节点需要的整数值并写回系统。
 */
bool MainWindow::applyBrightnessToSystem(int percent)
{
#ifdef Q_OS_LINUX
    if (backlightBrightnessPath.isEmpty()) {
        /* 首次调用时做一次背光节点发现。 */
        QDir backlightDir(QStringLiteral("/sys/class/backlight"));
        const QStringList entries = backlightDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (int i = 0; i < entries.size(); ++i) {
            const QString base = backlightDir.absoluteFilePath(entries.at(i));
            QFile maxFile(base + QStringLiteral("/max_brightness"));
            QFile brightnessFile(base + QStringLiteral("/brightness"));

            if (!maxFile.exists() || !brightnessFile.exists()) {
                continue;
            }

            if (!maxFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }

            bool ok = false;
            const int maxValue = QString::fromUtf8(maxFile.readAll()).trimmed().toInt(&ok);
            maxFile.close();
            if (!ok || maxValue <= 0) {
                continue;
            }

            backlightBrightnessPath = brightnessFile.fileName();
            backlightMaxValue = maxValue;
            break;
        }
    }

    if (backlightBrightnessPath.isEmpty() || backlightMaxValue <= 0) {
        return false;
    }

    /* 把 0~100 的界面百分比映射为驱动节点实际取值。 */
    const int value = qBound(1, backlightMaxValue * percent / 100, backlightMaxValue);
    QFile brightnessFile(backlightBrightnessPath);
    if (!brightnessFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&brightnessFile);
    stream << value;
    brightnessFile.close();
    return true;
#else
    Q_UNUSED(percent)
    return false;
#endif
}

/*
 * 从本地 ini 加载可持久化配置。
 * 说明：构造函数会先调用这里，再构建界面，这样阈值和密码默认值能直接体现在控件初始状态中。
 */
void MainWindow::loadConfig()
{
    /* 配置文件位于程序工作目录 */
    QSettings settings(SecurityUiUtils::securityConfigPath(), QSettings::IniFormat);

    /* 认证配置 */
    lockPassword = settings.value(QStringLiteral("auth/password"), lockPassword).toString();
    if (!SecurityUiUtils::isValidLockPassword(lockPassword)) {
        lockPassword = QStringLiteral("123456");
    }

    /* 阈值配置 */
    tempThresholdC = settings.value(QStringLiteral("threshold/temp_c"), tempThresholdC).toInt();
    smokeThresholdRaw = settings.value(QStringLiteral("threshold/smoke_raw"), smokeThresholdRaw).toInt();
}

/*
 * 将运行时配置写回本地 ini。
 * 说明：当前只保存锁屏密码、温度阈值和烟雾阈值，属于 UI 本地运行参数。
 */
void MainWindow::saveConfig() const
{
    /* 与 loadConfig() 使用同一文件路径，保证读写一致 */
    QSettings settings(SecurityUiUtils::securityConfigPath(), QSettings::IniFormat);

    /* 认证配置 */
    settings.setValue(QStringLiteral("auth/password"), lockPassword);

    /* 阈值配置 */
    settings.setValue(QStringLiteral("threshold/temp_c"), tempThresholdC);
    settings.remove(QStringLiteral("threshold/temp_x10"));
    settings.setValue(QStringLiteral("threshold/smoke_raw"), smokeThresholdRaw);
}
