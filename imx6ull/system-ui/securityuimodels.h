#ifndef SECURITYUIMODELS_H
#define SECURITYUIMODELS_H

/*
 * 模块说明：安防 UI 共享数据模型。
 * 职责：承载主窗口与页面拆分过程中复用的轻量数据结构，避免继续内嵌在 MainWindow 声明中。
 */

#include <QDateTime>

struct HistorySample
{
    /* 样本时间戳。 */
    QDateTime timestamp;
    /* 温度(℃)。 */
    int tempC;
    /* 湿度(%RH)。 */
    int humiPercent;
    /* 烟雾原始值。 */
    int smokeRaw;
};

struct SensorSnapshot
{
    /* 当前温度(℃)。 */
    int tempC = 0;
    /* 当前湿度(%RH)。 */
    int humiPercent = 0;
    /* 当前烟雾原始值。 */
    int smokeRaw = 0;
    /* 当前烟雾 PPM。 */
    double smokePpm = 0.0;
    /* 当前 PIR 检测状态。 */
    bool pirDetected = false;
};

#endif // SECURITYUIMODELS_H