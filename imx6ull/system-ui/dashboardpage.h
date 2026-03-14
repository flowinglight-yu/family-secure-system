#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

/*
 * 模块说明：首页仪表盘页面声明。
 * 职责：负责温湿度、烟雾、PIR 和系统状态四宫格的展示，不直接参与传感器解析与告警判定。
 */

#include <QWidget>

class QLabel;
class SmokeGaugeWidget;

class DashboardPage : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPage(QWidget *parent = nullptr);

    /* 更新温湿度显示值。 */
    void setTemperatureHumidity(int tempC, int humiPercent);
    /* 更新温度来源说明。 */
    void setTemperatureSourceText(const QString &text);
    /* 更新烟雾原始值、PPM 和仪表盘百分比。 */
    void setSmokeReading(int smokeRaw, double smokePpm, int smokePercent);
    /* 更新烟雾等级文案。 */
    void setSmokeLevelText(const QString &text);
    /* 更新烟雾警示图标可见性。 */
    void setSmokeWarningVisible(bool visible);
    /* 更新 PIR 状态文案。 */
    void setPirStateText(const QString &text);
    /* 更新 PIR 最后检测时间文案。 */
    void setPirLastSeenText(const QString &text);
    /* 更新 WiFi 状态文案。 */
    void setWifiStateText(const QString &text);
    /* 更新 OneNet 状态文案。 */
    void setOneNetStateText(const QString &text);
    /* 控制四张卡片的离线遮罩和烟雾仪表显示。 */
    void setOfflineState(bool sensorOffline);
    /* 根据窗口尺寸同步首页间距和烟雾仪表高度。 */
    void applyAdaptiveMetrics(int spacing, int smokeGaugeHeight);

private:
    /* 构建首页四宫格布局。 */
    void buildUi();

private:
    /* 温度显示文本。 */
    QLabel *tempValueLabel;
    /* 湿度显示文本。 */
    QLabel *humiValueLabel;
    /* 温度来源说明。 */
    QLabel *tempSourceLabel;
    /* 温湿度卡片离线遮罩。 */
    QLabel *tempOfflineMask;

    /* 烟雾仪表盘。 */
    SmokeGaugeWidget *smokeGauge;
    /* 烟雾原始值显示。 */
    QLabel *smokeValueLabel;
    /* 烟雾等级文案。 */
    QLabel *smokeLevelLabel;
    /* 烟雾警示图标。 */
    QLabel *smokeWarnLabel;
    /* 烟雾 PPM 显示。 */
    QLabel *smokePpmLabel;
    /* 烟雾卡片离线遮罩。 */
    QLabel *smokeOfflineMask;

    /* PIR 状态文案。 */
    QLabel *pirStateLabel;
    /* PIR 最后检测时间。 */
    QLabel *pirLastSeenLabel;
    /* PIR 卡片离线遮罩。 */
    QLabel *pirOfflineMask;

    /* WiFi 状态文案。 */
    QLabel *espStateLabel;
    /* OneNet 状态文案。 */
    QLabel *onetStateLabel;
    /* 系统卡片离线遮罩。 */
    QLabel *systemOfflineMask;
};

#endif // DASHBOARDPAGE_H