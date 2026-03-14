#ifndef ONENETAPICLIENT_H
#define ONENETAPICLIENT_H

/*
 * 模块说明：OneNet HTTP 轮询客户端声明。
 * 职责：定时拉取设备最新属性，把平台返回的多种 JSON 结构归一化为 UI 可直接消费的统一格式。
 */

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class OneNetApiClient : public QObject
{
    Q_OBJECT

public:
    explicit OneNetApiClient(QObject *parent = nullptr);

    /* 启动轮询并立即发起一次属性查询。 */
    void start();
    /* 停止轮询并复位当前请求状态。 */
    void stop();

    /* 返回当前运行时配置摘要，供设置页直接展示。 */
    QString runtimeInfoText() const;

signals:
    void connectedChanged(bool online);
    void sensorJsonReceived(const QString &jsonText);
    void errorOccurred(const QString &message);

private slots:
    void onPollTimeout();
    void onReplyFinished(QNetworkReply *reply);

private:
    /* 内部实现：发起属性查询并把响应解析成统一 JSON。 */
    void requestLatestProperty();
    bool parsePropertyResponse(const QByteArray &payload, QString *errorMessage);

private:
    QNetworkAccessManager *networkManager;
    QTimer *pollTimer;
    bool requestInFlight;
    bool cloudOnline;
    int consecutiveFailures;
};

#endif // ONENETAPICLIENT_H
