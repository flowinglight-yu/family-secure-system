#ifndef VIDEOPAGE_H
#define VIDEOPAGE_H

#include <QImage>
#include <QWidget>

class QLabel;
class QPushButton;
class QResizeEvent;

/*
 * 模块说明：视频页声明。
 * 职责：负责画面显示、时间水印和按钮交互，不直接管理摄像头生命周期。
 */
class VideoPage : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPage(QWidget *parent = nullptr);

    /* 更新时间水印文本。 */
    void setTimestampText(const QString &text);
    /* 显示最新视频帧，页面内部负责按当前尺寸缩放。 */
    void showFrame(const QImage &image);
    /* 清空当前画面并显示占位文本。 */
    void showEmptyFrame(const QString &text);
    /* 更新视频状态文案。 */
    void setVideoStateText(const QString &text);
    /* 同步启动按钮文本，反映当前采集开关状态。 */
    void setCaptureRunning(bool running);
    /* 根据窗口尺寸同步布局间距和按钮高度。 */
    void applyAdaptiveMetrics(int spacing, int compactSpacing, int primaryButtonHeight);

signals:
    /* 页面发起截图请求。 */
    void captureRequested();
    /* 页面发起摄像头启停请求。 */
    void startStopRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    /* 构建视频页布局和按钮信号连接。 */
    void buildUi();
    /* 在标签尺寸变化后重绘缩放后的当前帧。 */
    void updateFramePixmap();

private:
    /* 视频画面显示区。 */
    QLabel *videoFrameLabel;
    /* 视频时间水印。 */
    QLabel *videoTimestampLabel;
    /* 视频状态文本。 */
    QLabel *videoStateLabel;
    /* 截图按钮。 */
    QPushButton *captureButton;
    /* 摄像头启停按钮。 */
    QPushButton *startCameraButton;
    /* 当前缓存帧，供缩放重绘复用。 */
    QImage currentFrame;
};

#endif // VIDEOPAGE_H