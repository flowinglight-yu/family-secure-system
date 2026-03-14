#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/*
 * 模块说明：主窗口类声明。
 * 页面结构：contentStack 在锁屏页和主壳页之间切换，mainPageStack 在首页、视频、历史和设置页之间切换。
 */

#include <QDateTime>
#include <QImage>
#include <QMainWindow>
#include <QStackedWidget>
#include <QTimer>
#include <QVector>

#include "securityuimodels.h"

class QLabel;
class QPushButton;
class QResizeEvent;
class QWidget;
class DashboardPage;
class HistoryPage;
class LockPage;
class OneNetApiClient;
class SettingsPage;
class VideoPage;
class V4l2Camera;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    /* 校验密码并进入主界面 */
    void onLockConfirmRequested(const QString &pin);

    /* 左侧导航切页 */
    void onNavDashboard();
    void onNavVideo();
    void onNavHistory();
    void onNavSettings();

    /* 云端状态与传感器数据回调 */
    void onCloudConnectedChanged(bool online);
    void onSensorJsonReceived(const QString &jsonText);
    void onApiError(const QString &message);
    void onCameraError(const QString &message);
    void onFrameReady(const QImage &image);
    /* 视频页启动按钮：点击后才真正拉起摄像头采集 */
    void onStartCameraClicked();

    /* 设置页交互 */
    void onApplyThresholdClicked();
    void onChangePasswordClicked();
    void onCaptureClicked();
    void onBrightnessChanged(int value);
    void onRange1hClicked();
    void onRange24hClicked();
    void onUiTick();

private:
    /* UI 构建链：按“顶层容器 -> 页面 -> 子区域”逐层搭建界面。 */
    void buildUi();
    void buildShellPage();

    /* 状态刷新与页面交互辅助函数。 */
    void updateNavState(int index);
    void updateGlobalSafetyState();
    void updateThresholdState(int tempC, int smokeRaw);
    void setAlarmBanner(const QString &text, bool danger);
    void setLockError(const QString &message);
    void updateOfflineMasks(bool sensorOffline);
    bool parseSensorSnapshot(const QString &jsonText, SensorSnapshot *snapshot) const;
    void applySensorSnapshot(const SensorSnapshot &snapshot, const QDateTime &receivedAt);
    void updatePirDetectionState(bool detected, const QDateTime &receivedAt);
    void setHistoryWindow(int hours);
    void refreshHistoryChartsByWindow(int hours);
    void refreshNetworkInfo();
    void updateAdaptiveSpacing();
    bool applyBrightnessToSystem(int percent);
    void appendHistory(int tempC, int humiPercent, int smokeRaw);
    /* 统一摄像头启动入口，供启动按钮复用。 */
    void startCameraCapture();
    /* 停止摄像头采集并同步复位视频页状态。 */
    void stopCameraCapture();

    /* 本地配置持久化。 */
    void loadConfig();
    void saveConfig() const;

private:
    /* 根容器与顶层页面栈（锁屏/主壳） */
    QWidget *rootWidget;
    QStackedWidget *contentStack;

    /* 锁屏页状态 */
    LockPage *lockPage;

    /* 主壳页与顶部报警条 */
    QWidget *shellPage;
    QWidget *alarmBanner;
    QLabel *alarmBannerLabel;

    /* 导航按钮 */
    QPushButton *navDashboardButton;
    QPushButton *navVideoButton;
    QPushButton *navHistoryButton;
    QPushButton *navSettingsButton;

    /* 顶部状态栏 */
    QLabel *topTitleLabel;
    QLabel *topCloudLabel;

    /* 主内容页栈 */
    QStackedWidget *mainPageStack;
    DashboardPage *dashboardPage;
    VideoPage *videoPage;
    HistoryPage *historyPage;
    SettingsPage *settingsPage;

    QLabel *bottomTimeLabel;
    QLabel *bottomNetworkLabel;
    QLabel *bottomSafetyLamp;
    QLabel *bottomSafetyLabel;

    /* 业务模块对象 */
    OneNetApiClient *oneNetApiClient;
    V4l2Camera *camera;
    QTimer uiTickTimer;

    /* 运行态缓存与配置 */
    QString lockPassword;
    int tempThresholdC;
    int smokeThresholdRaw;
    bool cloudOnline;
    bool tempDanger;
    bool smokeDanger;
    bool pirDetected;
    bool blinkFlag;
    bool cameraCaptureActive;
    QDateTime lastPirSeen;
    QDateTime lastSensorUpdate;
    QImage lastFrameImage;
    QVector<HistorySample> historySamples;
    int historyWindowHours;
    QString backlightBrightnessPath;
    int backlightMaxValue;
};

#endif // MAINWINDOW_H
