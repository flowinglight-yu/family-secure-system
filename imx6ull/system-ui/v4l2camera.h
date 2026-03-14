#ifndef V4L2CAMERA_H
#define V4L2CAMERA_H

/*
 * 模块说明：V4L2 摄像头采集封装。
 * 职责：移植 smart-car-system 中的独立采集线程实现，对外保留 start()/stop() 接口给主窗口调用。
 */

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QString>
#include <QThread>

static const int kFrameBufferCount = 3;

struct CamBufInfo {
    unsigned char *start;
    unsigned long length;
};

class V4l2CaptureThread : public QThread
{
    Q_OBJECT

public:
    explicit V4l2CaptureThread(QObject *parent = nullptr);
    ~V4l2CaptureThread();

    void setFrameSize(int width, int height);
    bool initCamera(const QString &device);
    void startCapture();
    void stopCapture();
    void shutdown();

signals:
    void frameReady(const QImage &image);
    void errorOccurred(const QString &message);

protected:
    void run() override;

private:
    int fd;
    bool running;
    bool initialized;
    CamBufInfo buffers[kFrameBufferCount];
    int frameWidth;
    int frameHeight;
    QMutex mutex;

    bool openDevice(const QString &device);
    bool queryCapability();
    bool setFormat();
    bool initBuffers();
    bool startStreaming();
    void stopStreaming();
    void cleanup();
    QImage decodeJpegToImage(unsigned char *jpegData, unsigned long jpegSize);
};

class V4l2Camera : public QObject
{
    Q_OBJECT

public:
    explicit V4l2Camera(QObject *parent = nullptr);
    ~V4l2Camera();

    /* 启动摄像头采集 */
    bool start(const QString &devicePath, int width, int height);
    /* 停止采集并释放设备 */
    void stop();

signals:
    /* 输出一帧解码后的图像 */
    void frameReady(const QImage &image);
    /* 输出摄像头错误信息 */
    void errorOccurred(const QString &message);
private slots:
    void onCaptureThreadFrameReady(const QImage &image);
    void onCaptureThreadError(const QString &message);

private:
    QString currentDevice;
    int frameWidth;
    int frameHeight;
    V4l2CaptureThread *captureThread;
};

#endif // V4L2CAMERA_H
