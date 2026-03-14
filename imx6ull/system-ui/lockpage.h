#ifndef LOCKPAGE_H
#define LOCKPAGE_H

#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;

/*
 * 模块说明：锁屏页声明。
 * 职责：承载密码输入界面、圆点反馈、提示文本和错误动画，不负责密码校验本身。
 */
class LockPage : public QWidget
{
    Q_OBJECT

public:
    explicit LockPage(QWidget *parent = nullptr);

    /* 清空当前输入并恢复圆点状态。 */
    void resetInput();
    /* 设置提示文案，并按需切换错误样式。 */
    void setHintText(const QString &text, bool error = false);
    /* 播放锁屏页抖动动画，复用原主窗口的错误反馈。 */
    void playErrorAnimation();
    /* 根据窗口尺寸同步键盘高度、卡片边距和布局间距。 */
    void applyAdaptiveMetrics(int spacing, int compactSpacing, int sideMargin, int keypadButtonHeight);

signals:
    /* 用户点击确认后，把当前输入密码上抛给 MainWindow 校验。 */
    void confirmRequested(const QString &pin);

private slots:
    /* 数字键点击：更新输入缓存和圆点显示。 */
    void onDigitButtonClicked();
    /* 清除键点击：重置输入并恢复默认提示。 */
    void onClearButtonClicked();
    /* 确认键点击：仅负责发送当前输入，不做业务判断。 */
    void onConfirmButtonClicked();

private:
    /* 构建锁屏页面布局和按钮连接。 */
    void buildUi();
    /* 按当前输入长度刷新圆点实心/空心状态。 */
    void updateDots();

private:
    /* 页面提示文本。 */
    QLabel *lockHintLabel;
    /* 最多 6 个密码圆点。 */
    QVector<QLabel *> lockDots;
    /* 当前输入缓存。 */
    QString enteredPin;
};

#endif // LOCKPAGE_H