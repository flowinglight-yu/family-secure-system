#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

/*
 * 模块说明：设置页声明。
 * 职责：负责设置表单布局、表单读取与显示回填，不直接处理配置持久化和系统调用。
 */

#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

    /* 回填阈值输入框。 */
    void setThresholdValues(int tempThresholdC, int smokeThresholdRaw);
    /* 回填亮度滑杆值。 */
    void setBrightnessValue(int value);
    /* 更新网络信息文本。 */
    void setWifiInfoText(const QString &text);
    /* 更新云端 API 信息文本。 */
    void setCloudApiInfoText(const QString &text);
    /* 清空密码输入框。 */
    void clearPasswordInputs();
    /* 读取温度阈值输入。 */
    int tempThresholdValue(bool *ok = nullptr) const;
    /* 读取烟雾阈值输入。 */
    int smokeThresholdValue(bool *ok = nullptr) const;
    /* 读取旧密码输入。 */
    QString oldPassword() const;
    /* 读取新密码输入。 */
    QString newPassword() const;
    /* 读取确认密码输入。 */
    QString confirmPassword() const;
    /* 根据窗口尺寸同步设置页间距和按钮高度。 */
    void applyAdaptiveMetrics(int spacing, int primaryButtonHeight);

signals:
    /* 请求应用阈值。 */
    void applyThresholdRequested();
    /* 请求调整亮度。 */
    void brightnessChanged(int value);
    /* 请求修改密码。 */
    void changePasswordRequested();

private:
    /* 构建设置页布局与信号连接。 */
    void buildUi();

private:
    /* WiFi 信息文本。 */
    QLabel *wifiInfoLabel;
    /* OneNet API 信息文本。 */
    QLabel *cloudApiInfoLabel;
    /* 温度阈值输入框。 */
    QLineEdit *tempThresholdEdit;
    /* 烟雾阈值输入框。 */
    QLineEdit *smokeThresholdEdit;
    /* 应用阈值按钮。 */
    QPushButton *applyThresholdButton;
    /* 亮度滑杆。 */
    QSlider *brightnessSlider;
    /* 旧密码输入框。 */
    QLineEdit *oldPasswordEdit;
    /* 新密码输入框。 */
    QLineEdit *newPasswordEdit;
    /* 确认密码输入框。 */
    QLineEdit *confirmPasswordEdit;
    /* 修改密码按钮。 */
    QPushButton *changePasswordButton;
    /* 版本与平台说明。 */
    QLabel *versionInfoLabel;
};

#endif // SETTINGSPAGE_H