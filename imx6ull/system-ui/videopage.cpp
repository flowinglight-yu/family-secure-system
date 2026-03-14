#include "videopage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

/*
 * 模块说明：视频页实现。
 * 设计边界：页面只负责显示和按钮事件上抛，摄像头启停、截图保存和错误处理仍由 MainWindow 统一协调。
 */
VideoPage::VideoPage(QWidget *parent)
    : QWidget(parent),
      videoFrameLabel(new QLabel(QStringLiteral("摄像头未连接"), this)),
      videoTimestampLabel(new QLabel(QStringLiteral("--"), this)),
      videoStateLabel(new QLabel(QStringLiteral("视频状态：待启动"), this)),
      captureButton(new QPushButton(QStringLiteral("📸 截图"), this)),
      startCameraButton(new QPushButton(QStringLiteral("▶ 启动"), this))
{
    buildUi();
}

/* 更新时间水印文本。 */
void VideoPage::setTimestampText(const QString &text)
{
    videoTimestampLabel->setText(text);
}

/* 接收外部传入的原始帧，并按当前显示区缩放。 */
void VideoPage::showFrame(const QImage &image)
{
    currentFrame = image;
    updateFramePixmap();
}

/* 画面不可用时恢复占位文本。 */
void VideoPage::showEmptyFrame(const QString &text)
{
    currentFrame = QImage();
    videoFrameLabel->clear();
    videoFrameLabel->setText(text);
}

/* 刷新视频状态文案。 */
void VideoPage::setVideoStateText(const QString &text)
{
    videoStateLabel->setText(text);
}

/* 根据采集状态同步按钮文本。 */
void VideoPage::setCaptureRunning(bool running)
{
    startCameraButton->setText(running ? QStringLiteral("■ 停止") : QStringLiteral("▶ 启动"));
}

/*
 * 视频页自适配。
 * 说明：沿用主窗口统一计算出的 spacing 和按钮高度，保持页面节奏一致。
 */
void VideoPage::applyAdaptiveMetrics(int spacing, int compactSpacing, int primaryButtonHeight)
{
    auto setSpacingByName = [](QWidget *scope, const QString &name, int value) {
        if (scope == nullptr) {
            return;
        }
        if (QLayout *layout = scope->findChild<QLayout *>(name)) {
            layout->setSpacing(value);
        }
    };

    setSpacingByName(this, QStringLiteral("VideoRootLayout"), spacing);
    setSpacingByName(this, QStringLiteral("VideoCardLayout"), compactSpacing);
    setSpacingByName(this, QStringLiteral("VideoControlLayout"), compactSpacing);

    captureButton->setFixedHeight(primaryButtonHeight);
    startCameraButton->setFixedHeight(primaryButtonHeight);
}

/* 标签尺寸变化后重新渲染缓存帧，避免画面停留在旧比例。 */
void VideoPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateFramePixmap();
}

/* 视频页：左侧画面，右侧控制按钮。 */
void VideoPage::buildUi()
{
    setObjectName(QStringLiteral("VideoPage"));
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setObjectName(QStringLiteral("VideoRootLayout"));
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    /* 左侧视频卡片：上方时间水印，下方实时视频画面。 */
    QFrame *videoContainer = new QFrame(this);
    videoContainer->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *videoLayout = new QVBoxLayout(videoContainer);
    videoLayout->setObjectName(QStringLiteral("VideoCardLayout"));

    videoTimestampLabel->setObjectName(QStringLiteral("VideoWatermark"));
    videoFrameLabel->setAlignment(Qt::AlignCenter);
    videoFrameLabel->setMinimumSize(0, 0);
    videoFrameLabel->setObjectName(QStringLiteral("VideoFrame"));

    videoLayout->addWidget(videoTimestampLabel);
    videoLayout->addWidget(videoFrameLabel, 1);

    /* 右侧控制卡片：截图、启停和状态文案。 */
    QFrame *controlPanel = new QFrame(this);
    controlPanel->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setObjectName(QStringLiteral("VideoControlLayout"));
    captureButton->setObjectName(QStringLiteral("PrimaryButton"));
    startCameraButton->setObjectName(QStringLiteral("PrimaryButton"));

    connect(captureButton, SIGNAL(clicked()), this, SIGNAL(captureRequested()));
    connect(startCameraButton, SIGNAL(clicked()), this, SIGNAL(startStopRequested()));

    controlLayout->addWidget(captureButton);
    controlLayout->addWidget(startCameraButton);
    controlLayout->addWidget(videoStateLabel);
    controlLayout->addStretch();

    layout->addWidget(videoContainer, 4);
    layout->addWidget(controlPanel, 1);
}

/*
 * 按当前标签尺寸等比缩放缓存帧。
 * 说明：页面重绘时直接复用原始帧，避免 MainWindow 参与界面缩放细节。
 */
void VideoPage::updateFramePixmap()
{
    if (currentFrame.isNull()) {
        return;
    }

    videoFrameLabel->setPixmap(QPixmap::fromImage(currentFrame).scaled(videoFrameLabel->size(),
                                                                    Qt::KeepAspectRatio,
                                                                    Qt::SmoothTransformation));
}