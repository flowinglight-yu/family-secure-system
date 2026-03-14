#include "v4l2camera.h"

#include <QBuffer>
#include <QByteArray>
#include <QDebug>
#include <QImageReader>

#include <cstring>

#ifdef Q_OS_LINUX
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/*
 * 模块说明：V4L2 摄像头采集实现。
 * 运行流程：复用 smart-car-system 的独立线程采集方案，在工作线程内完成 V4L2 流式采集和 JPEG 解码。
 */

/*
 * 构造采集线程对象。
 * 说明：初始化线程状态和默认分辨率，缓冲区布局与 smart-car-system 保持一致。
 */
V4l2CaptureThread::V4l2CaptureThread(QObject *parent)
    : QThread(parent),
      fd(-1),
      running(false),
      initialized(false),
      frameWidth(640),
      frameHeight(480)
{
    memset(buffers, 0, sizeof(buffers));
}

/*
 * 析构采集线程对象。
 * 说明：确保退出线程并释放缓冲区与设备句柄。
 */
V4l2CaptureThread::~V4l2CaptureThread()
{
    shutdown();
}

/*
 * 设置目标分辨率。
 * 说明：用于在初始化设备前同步外层传入的采样尺寸。
 */
void V4l2CaptureThread::setFrameSize(int width, int height)
{
    QMutexLocker locker(&mutex);
    frameWidth = width;
    frameHeight = height;
}

/*
 * 初始化摄像头。
 * 运行流程：按 smart-car-system 的顺序执行 open、QUERYCAP、S_FMT 和 MMAP 初始化。
 */
bool V4l2CaptureThread::initCamera(const QString &device)
{
    QMutexLocker locker(&mutex);

#ifdef Q_OS_LINUX
    if (initialized) {
        cleanup();
    }

    if (!openDevice(device)) {
        return false;
    }

    if (!queryCapability()) {
        cleanup();
        return false;
    }

    if (!setFormat()) {
        cleanup();
        return false;
    }

    if (!initBuffers()) {
        cleanup();
        return false;
    }

    initialized = true;
    return true;
#else
    Q_UNUSED(device)
    emit errorOccurred(QStringLiteral("当前平台不支持 V4L2 摄像头采集"));
    return false;
#endif
}

/*
 * 打开摄像头设备。
 * 说明：沿用 smart-car-system 的阻塞读写打开方式。
 */
bool V4l2CaptureThread::openDevice(const QString &device)
{
#ifdef Q_OS_LINUX
    fd = ::open(device.toLocal8Bit().data(), O_RDWR);
    if (fd < 0) {
        emit errorOccurred(QStringLiteral("Failed to open device %1: %2")
                           .arg(device)
                           .arg(QString::fromLocal8Bit(strerror(errno))));
        return false;
    }
    return true;
#else
    Q_UNUSED(device)
    return false;
#endif
}

/*
 * 查询设备能力。
 * 说明：先校验是否为视频采集设备。
 */
bool V4l2CaptureThread::queryCapability()
{
#ifdef Q_OS_LINUX
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));

    if (::ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        emit errorOccurred(QStringLiteral("Failed to query device capabilities"));
        return false;
    }

    if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
        emit errorOccurred(QStringLiteral("Device is not a video capture device"));
        return false;
    }

    return true;
#else
    return false;
#endif
}

/*
 * 设置采集格式。
 * 说明：固定请求 MJPEG 和调用方传入的分辨率。
 */
bool V4l2CaptureThread::setFormat()
{
#ifdef Q_OS_LINUX
    struct v4l2_format format;
    memset(&format, 0, sizeof(format));

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = frameWidth;
    format.fmt.pix.height = frameHeight;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;

    if (::ioctl(fd, VIDIOC_S_FMT, &format) < 0) {
        emit errorOccurred(QStringLiteral("Failed to set video format"));
        return false;
    }

    if (format.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
        emit errorOccurred(QStringLiteral("Device does not support MJPEG format"));
        return false;
    }

    frameWidth = format.fmt.pix.width;
    frameHeight = format.fmt.pix.height;

    return true;
#else
    return false;
#endif
}

/*
 * 初始化 MMAP 缓冲区。
 * 说明：使用多缓冲区循环采集，减少帧读取阻塞。
 */
bool V4l2CaptureThread::initBuffers()
{
#ifdef Q_OS_LINUX
    struct v4l2_requestbuffers request;
    struct v4l2_buffer buffer;

    memset(&request, 0, sizeof(request));
    memset(&buffer, 0, sizeof(buffer));

    request.count = kFrameBufferCount;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;

    if (::ioctl(fd, VIDIOC_REQBUFS, &request) < 0) {
        emit errorOccurred(QStringLiteral("Failed to request buffers"));
        return false;
    }

    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;

    for (buffer.index = 0; buffer.index < kFrameBufferCount; ++buffer.index) {
        if (::ioctl(fd, VIDIOC_QUERYBUF, &buffer) < 0) {
            emit errorOccurred(QStringLiteral("Failed to query buffer"));
            return false;
        }

        buffers[buffer.index].length = buffer.length;
        buffers[buffer.index].start = static_cast<unsigned char *>(
                    ::mmap(nullptr,
                           buffer.length,
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,
                           fd,
                           buffer.m.offset));

        if (buffers[buffer.index].start == MAP_FAILED) {
            buffers[buffer.index].start = nullptr;
            buffers[buffer.index].length = 0;
            emit errorOccurred(QStringLiteral("Failed to map buffer"));
            return false;
        }
    }

    for (buffer.index = 0; buffer.index < kFrameBufferCount; ++buffer.index) {
        if (::ioctl(fd, VIDIOC_QBUF, &buffer) < 0) {
            emit errorOccurred(QStringLiteral("Failed to queue buffer"));
            return false;
        }
    }

    return true;
#else
    return false;
#endif
}

/*
 * 启动视频流。
 * 说明：初始化完成后再调用 STREAMON，让驱动进入稳定的循环采集状态。
 */
bool V4l2CaptureThread::startStreaming()
{
#ifdef Q_OS_LINUX
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (::ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        emit errorOccurred(QStringLiteral("Failed to start streaming"));
        return false;
    }

    return true;
#else
    return false;
#endif
}

/*
 * 停止视频流。
 * 说明：在采集线程退出或资源释放前停止驱动采集。
 */
void V4l2CaptureThread::stopStreaming()
{
#ifdef Q_OS_LINUX
    if (fd >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ::ioctl(fd, VIDIOC_STREAMOFF, &type);
    }
#endif
}

/*
 * 清理摄像头资源。
 * 说明：解除 mmap、关闭设备并重置初始化状态。
 */
void V4l2CaptureThread::cleanup()
{
#ifdef Q_OS_LINUX
    stopStreaming();

    for (int index = 0; index < kFrameBufferCount; ++index) {
        if (buffers[index].start != nullptr && buffers[index].start != MAP_FAILED) {
            ::munmap(buffers[index].start, buffers[index].length);
            buffers[index].start = nullptr;
            buffers[index].length = 0;
        }
    }

    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
#endif
    initialized = false;
}

/*
 * 启动采集线程。
 * 说明：先 STREAMON，再启动工作线程进入循环取帧。
 */
void V4l2CaptureThread::startCapture()
{
    QMutexLocker locker(&mutex);

    if (!initialized) {
        emit errorOccurred(QStringLiteral("Camera not initialized"));
        return;
    }

    if (!startStreaming()) {
        return;
    }

    running = true;
    start();
}

/*
 * 停止采集线程。
 * 说明：请求线程退出并等待循环结束，避免释放缓冲区时仍在读帧。
 */
void V4l2CaptureThread::stopCapture()
{
    running = false;

    if (isRunning()) {
        quit();
        wait(3000);
    }
}

/*
 * 关闭摄像头并释放底层资源。
 * 说明：stopCapture() 行为补强，确保外层 stop() 后设备句柄被释放。
 */
void V4l2CaptureThread::shutdown()
{
    stopCapture();

    QMutexLocker locker(&mutex);
    cleanup();
}

/*
 * 工作线程主循环。
 * 说明：在独立线程中等待 select、DQBUF、解码并回收缓冲区。
 */
void V4l2CaptureThread::run()
{
#ifdef Q_OS_LINUX
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;

    while (running) {
        fd_set fds;
        struct timeval tv;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        const int ret = ::select(fd + 1, &fds, nullptr, nullptr, &tv);
        if (ret < 0) {
            if (errno != EINTR) {
                emit errorOccurred(QStringLiteral("Select failed"));
                break;
            }
            continue;
        }

        if (ret == 0) {
            continue;
        }

        if (!FD_ISSET(fd, &fds)) {
            continue;
        }

        if (::ioctl(fd, VIDIOC_DQBUF, &buffer) < 0) {
            if (errno != EAGAIN) {
                emit errorOccurred(QStringLiteral("Failed to dequeue buffer"));
                break;
            }
            continue;
        }

        if (buffer.index >= kFrameBufferCount) {
            ::ioctl(fd, VIDIOC_QBUF, &buffer);
            continue;
        }

        const QImage image = decodeJpegToImage(buffers[buffer.index].start, buffer.bytesused);

        if (::ioctl(fd, VIDIOC_QBUF, &buffer) < 0) {
            emit errorOccurred(QStringLiteral("Failed to requeue buffer"));
            break;
        }

        if (!image.isNull()) {
            emit frameReady(image);
        }

        msleep(33);
    }

    stopStreaming();
#endif
}

/*
 * 使用 Qt 内置功能解码 JPEG 数据。
 * 说明：QImageReader 解码路径仅在解码失败时保留控制台错误，便于排障。
 */
QImage V4l2CaptureThread::decodeJpegToImage(unsigned char *jpegData, unsigned long jpegSize)
{
    QByteArray data(reinterpret_cast<const char *>(jpegData), static_cast<int>(jpegSize));
    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer);
    reader.setFormat("JPEG");

    const QImage image = reader.read();
    if (image.isNull()) {
        qWarning() << "[camera] JPEG decode failed, bytes=" << jpegSize << "reason=" << reader.errorString();
        return QImage();
    }

    return image;
}

/*
 * 构造外层摄像头封装对象。
 * 说明：负责承接主窗口调用，并把线程信号透传给 UI。
 */
V4l2Camera::V4l2Camera(QObject *parent)
    : QObject(parent),
      frameWidth(640),
      frameHeight(480),
      captureThread(new V4l2CaptureThread(this))
{
    connect(captureThread, SIGNAL(frameReady(QImage)), this, SLOT(onCaptureThreadFrameReady(QImage)));
    connect(captureThread, SIGNAL(errorOccurred(QString)), this, SLOT(onCaptureThreadError(QString)));
}

/*
 * 析构外层封装对象。
 * 说明：析构前关闭线程和底层设备。
 */
V4l2Camera::~V4l2Camera()
{
    stop();
}

/*
 * 启动摄像头采集。
 * 说明：保持主窗口现有 start() 调用方式，内部改为线程化采集实现。
 */
bool V4l2Camera::start(const QString &devicePath, int width, int height)
{
    stop();

    currentDevice = devicePath;
    frameWidth = width;
    frameHeight = height;

    captureThread->setFrameSize(frameWidth, frameHeight);
    if (!captureThread->initCamera(currentDevice)) {
        return false;
    }

    captureThread->startCapture();
    return true;
}

/*
 * 停止摄像头采集。
 * 说明：统一关闭采集线程和底层资源。
 */
void V4l2Camera::stop()
{
    captureThread->shutdown();
}

void V4l2Camera::onCaptureThreadFrameReady(const QImage &image)
{
    emit frameReady(image);
}

void V4l2Camera::onCaptureThreadError(const QString &message)
{
    emit errorOccurred(message);
}
