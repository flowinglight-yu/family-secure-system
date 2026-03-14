#ifndef SECURITYUIUTILS_H
#define SECURITYUIUTILS_H

/*
 * 模块说明：安防 UI 纯工具函数声明。
 * 职责：提供密码校验、传感器字段读取、历史坐标标签和界面状态文本生成等无状态能力。
 */

#include <QString>
#include <QStringList>

class QJsonObject;
class QJsonValue;

namespace SecurityUiUtils {

/* 校验锁屏密码格式是否符合约束。 */
bool isValidLockPassword(const QString &password);
/* 生成历史图表的 X 轴标签。 */
QStringList historyAxisLabels(int hours);
/* 按字段名读取一个数值型传感器字段。 */
double readSensorNumberField(const QJsonObject &object,
                             const QJsonObject &params,
                             const QString &name,
                             double fallback = 0.0);
/* 按候选字段顺序读取第一个可用传感器值。 */
double readFirstAvailableSensorNumber(const QJsonObject &object,
                                      const QJsonObject &params,
                                      const QStringList &names,
                                      double fallback = 0.0);
/* 生成温度来源标签文案。 */
QString tempSourceText(bool tempDanger);
/* 生成 PIR 状态文案。 */
QString pirStateText(bool detected);
/* 生成 WiFi 状态文案。 */
QString wifiStateText(bool online);
/* 生成云端在线状态文案。 */
QString cloudStateText(bool online);
/* 生成 OneNet 在线状态文案。 */
QString oneNetStateText(bool online);
/* 生成底栏网络状态文案。 */
QString bottomNetworkStateText(bool online);
/* 返回本地配置文件路径。 */
QString securityConfigPath();

} // namespace SecurityUiUtils

#endif // SECURITYUIUTILS_H