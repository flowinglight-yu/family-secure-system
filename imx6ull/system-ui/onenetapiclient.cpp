#include "onenetapiclient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDateTime>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

/*
 * 模块说明：OneNet HTTP 轮询客户端实现。
 * 运行流程：定时请求最新属性，解析平台返回的多种结构，检查上报时间是否过期，再把结果转成统一 params JSON 发给主界面。
 */

namespace {
/* OneNet API 基础配置与轮询参数。 */
const char* const kOneNetApiHost = "https://iot-api.heclouds.com";
const char* const kOneNetLatestPropertyPath = "/thingmodel/query-device-property";

/* 当前项目约定：OneNet 参数在此集中硬编码管理，界面层仅展示脱敏后的摘要。 */
const char* const kOneNetProductId = "";
const char* const kOneNetDeviceName = "";
const char* const kOneNetDeviceKey = "";
//时间截至2026-12-03 16:18:05
const char* const kOneNetApiToken = "";

const int kPollIntervalMs = 2000;
const int kRequestTimeoutMs = 4000;
const int kDeviceDataStaleMs = 20000;

/*
 * 对敏感参数做脱敏显示。
 * 说明：只保留前后少量字符，中间用省略形式替代，避免设置页直接暴露密钥全文。
 */
QString maskSensitiveText(const char *value, int prefixLength = 4, int suffixLength = 4)
{
    const QString text = QString::fromLatin1(value);
    if (text.size() <= (prefixLength + suffixLength)) {
        return QString(text.size(), QChar('*'));
    }

    return text.left(prefixLength)
            + QStringLiteral("...")
            + text.right(suffixLength);
}

/* 把 QNetworkReply 错误码转成稳定的诊断名，便于串口日志快速定位。 */
QString networkErrorName(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::NoError:
        return QStringLiteral("NoError");
    case QNetworkReply::ConnectionRefusedError:
        return QStringLiteral("ConnectionRefusedError");
    case QNetworkReply::RemoteHostClosedError:
        return QStringLiteral("RemoteHostClosedError");
    case QNetworkReply::HostNotFoundError:
        return QStringLiteral("HostNotFoundError");
    case QNetworkReply::TimeoutError:
        return QStringLiteral("TimeoutError");
    case QNetworkReply::OperationCanceledError:
        return QStringLiteral("OperationCanceledError");
    case QNetworkReply::SslHandshakeFailedError:
        return QStringLiteral("SslHandshakeFailedError");
    case QNetworkReply::TemporaryNetworkFailureError:
        return QStringLiteral("TemporaryNetworkFailureError");
    case QNetworkReply::NetworkSessionFailedError:
        return QStringLiteral("NetworkSessionFailedError");
    case QNetworkReply::ProxyConnectionRefusedError:
        return QStringLiteral("ProxyConnectionRefusedError");
    case QNetworkReply::ProxyConnectionClosedError:
        return QStringLiteral("ProxyConnectionClosedError");
    case QNetworkReply::ProxyNotFoundError:
        return QStringLiteral("ProxyNotFoundError");
    case QNetworkReply::ProxyTimeoutError:
        return QStringLiteral("ProxyTimeoutError");
    case QNetworkReply::ProxyAuthenticationRequiredError:
        return QStringLiteral("ProxyAuthenticationRequiredError");
    case QNetworkReply::ContentNotFoundError:
        return QStringLiteral("ContentNotFoundError");
    case QNetworkReply::AuthenticationRequiredError:
        return QStringLiteral("AuthenticationRequiredError");
    case QNetworkReply::ContentAccessDenied:
        return QStringLiteral("ContentAccessDenied");
    case QNetworkReply::ProtocolFailure:
        return QStringLiteral("ProtocolFailure");
    default:
        return QStringLiteral("NetworkError(%1)").arg(static_cast<int>(error));
    }
}

/* 把常见 SSL 错误转换成更适合现场排障的中文说明。 */
QString sslErrorHint(const QSslError &error)
{
    switch (error.error()) {
    case QSslError::CertificateNotYetValid:
        return QStringLiteral("证书尚未生效，通常表示 IMX6ULL 系统时间过早，请先校时");
    case QSslError::CertificateExpired:
        return QStringLiteral("证书已过期，可能是系统时间过晚或系统根证书包过旧");
    case QSslError::HostNameMismatch:
        return QStringLiteral("主机名校验失败，请检查访问域名和 DNS 劫持问题");
    case QSslError::SelfSignedCertificate:
    case QSslError::SelfSignedCertificateInChain:
        return QStringLiteral("证书链不受信任，请检查系统 CA 根证书");
    default:
        break;
    }

    return QString();
}

/* 解析 Token 的过期时间，便于判断是否已经超期。 */
QDateTime oneNetTokenExpireTimeUtc()
{
    const QUrlQuery tokenQuery(QString::fromLatin1(kOneNetApiToken));
    bool ok = false;
    const qint64 epochSeconds = tokenQuery.queryItemValue(QStringLiteral("et")).toLongLong(&ok);
    if (!ok || epochSeconds <= 0) {
        return QDateTime();
    }
    return QDateTime::fromSecsSinceEpoch(epochSeconds, Qt::UTC);
}

/* 打印 HTTPS 运行前置条件，重点观察 SSL 能力、系统时间和证书数量。 */
void logStartupDiagnostics()
{
    const QDateTime nowLocal = QDateTime::currentDateTime();
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    const QDateTime tokenExpireUtc = oneNetTokenExpireTimeUtc();

    qInfo().noquote() << QStringLiteral("[onenet] 启动诊断 local=%1 utc=%2 sslSupported=%3 sslBuild=%4 sslRuntime=%5 systemCa=%6 tokenExpireUtc=%7")
                         .arg(nowLocal.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss t")))
                         .arg(nowUtc.toString(Qt::ISODate))
                         .arg(QSslSocket::supportsSsl() ? QStringLiteral("true") : QStringLiteral("false"))
                         .arg(QSslSocket::sslLibraryBuildVersionString())
                         .arg(QSslSocket::sslLibraryVersionString())
                         .arg(QSslConfiguration::systemCaCertificates().size())
                         .arg(tokenExpireUtc.isValid() ? tokenExpireUtc.toString(Qt::ISODate) : QStringLiteral("invalid"));

    if (!QSslSocket::supportsSsl()) {
        qWarning().noquote() << QStringLiteral("[onenet] 当前 Qt/系统未提供 SSL 支持，HTTPS 请求一定会失败。IMX6ULL 需检查 OpenSSL/QtNetwork SSL 插件是否可用。");
    }
    if (QSslConfiguration::systemCaCertificates().isEmpty()) {
        qWarning().noquote() << QStringLiteral("[onenet] 系统 CA 证书为空，HTTPS 证书校验大概率失败。请检查目标板是否安装根证书包。");
    }
    if (tokenExpireUtc.isValid() && nowUtc > tokenExpireUtc) {
        qWarning().noquote() << QStringLiteral("[onenet] OneNet Token 已过期，当前 UTC 时间已经晚于 tokenExpireUtc。");
    }
}
}

/*
 * 构造 OneNet 轮询客户端。
 * 说明：初始化网络管理器、轮询定时器和基础运行状态，并完成信号槽绑定。
 */
OneNetApiClient::OneNetApiClient(QObject *parent)
    : QObject(parent),
      networkManager(new QNetworkAccessManager(this)),
      pollTimer(new QTimer(this)),
      requestInFlight(false),
      cloudOnline(false),
      consecutiveFailures(0)
{
    pollTimer->setSingleShot(false);
    pollTimer->setInterval(kPollIntervalMs);

    connect(pollTimer, SIGNAL(timeout()), this, SLOT(onPollTimeout()));
    connect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onReplyFinished(QNetworkReply*)));
}

/*
 * 启动 OneNet 轮询。
 * 说明：开启轮询定时器后会立即主动请求一次，避免首页初次进入时长时间空白。
 */
void OneNetApiClient::start()
{
    static bool startupLogged = false;

    if (!startupLogged) {
        startupLogged = true;
        logStartupDiagnostics();
    }

    /* 启动后立即拉一次数据，避免页面初次进入时长时间空白。 */
    if (!pollTimer->isActive()) {
        pollTimer->start();
    }

    requestLatestProperty();
}

/*
 * 停止 OneNet 轮询。
 * 说明：这里只停止后续轮询和飞行态标记，不主动清空已收到的数据缓存。
 */
void OneNetApiClient::stop()
{
    pollTimer->stop();
    requestInFlight = false;
}

/*
 * 生成设置页展示的运行时信息摘要。
 * 说明：返回值面向 UI 文本展示，敏感字段会先做脱敏处理。
 */
QString OneNetApiClient::runtimeInfoText() const
{
    return QStringLiteral("OneNet Host：%1\n接口：%2\n参数方案：代码内硬编码\nProduct ID：%3\nDevice Name：%4\nDevice Key：%5\nAuth Token：%6\n轮询周期：%7 ms")
            .arg(QString::fromLatin1(kOneNetApiHost))
            .arg(QString::fromLatin1(kOneNetLatestPropertyPath))
            .arg(QString::fromLatin1(kOneNetProductId))
            .arg(QString::fromLatin1(kOneNetDeviceName))
            .arg(maskSensitiveText(kOneNetDeviceKey))
            .arg(maskSensitiveText(kOneNetApiToken, 10, 8))
            .arg(kPollIntervalMs);
}

/*
 * 轮询定时器回调。
 * 说明：定时器只负责触发一次新的属性查询，真正的请求组装在 requestLatestProperty() 中完成。
 */
void OneNetApiClient::onPollTimeout()
{
    requestLatestProperty();
}

/*
 * 请求设备最新属性。
 * 运行流程：组装查询 URL、附带鉴权头、发起 GET 请求，并为请求挂接独立超时控制。
 */
void OneNetApiClient::requestLatestProperty()
{
    static int requestSerial = 0;

    if (requestInFlight) {
        return;
    }

    /* 构造查询 URL：按 product_id 和 device_name 获取设备最新属性。 */
    QUrl url(QString::fromLatin1(kOneNetApiHost) + QString::fromLatin1(kOneNetLatestPropertyPath));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("product_id"), QString::fromLatin1(kOneNetProductId));
    query.addQueryItem(QStringLiteral("device_name"), QString::fromLatin1(kOneNetDeviceName));
    url.setQuery(query);

    /* 通过 Authorization 头携带平台鉴权令牌。 */
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArray(kOneNetApiToken));
    request.setRawHeader("Accept", "application/json");

    const int requestId = ++requestSerial;

    QNetworkReply *reply = networkManager->get(request);
    requestInFlight = true;
    reply->setProperty("onenetRequestId", requestId);
    reply->setProperty("onenetStartedAtMs", QDateTime::currentMSecsSinceEpoch());

    connect(reply, &QNetworkReply::sslErrors, this, [requestId](const QList<QSslError> &errors) {
        for (int i = 0; i < errors.size(); ++i) {
            const QString hint = sslErrorHint(errors.at(i));
            qWarning().noquote() << QStringLiteral("[onenet] 请求 #%1 SSL 错误：%2")
                                    .arg(requestId)
                                    .arg(errors.at(i).errorString());
            if (!hint.isEmpty()) {
                qWarning().noquote() << QStringLiteral("[onenet] 请求 #%1 诊断：%2")
                                        .arg(requestId)
                                        .arg(hint);
            }
        }
    });

    /* 为每次请求挂一个独立超时定时器，超时后主动 abort。 */
    QTimer *timeoutTimer = new QTimer(reply);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, [reply, requestId]() {
        if (reply->isRunning()) {
            qWarning().noquote() << QStringLiteral("[onenet] 请求 #%1 超时，%2 ms 内未完成，准备 abort。")
                                    .arg(requestId)
                                    .arg(kRequestTimeoutMs);
            reply->abort();
        }
    });
    timeoutTimer->start(kRequestTimeoutMs);
}

/*
 * 处理 OneNet HTTP 响应。
 * 运行流程：区分网络错误与解析错误，更新在线状态和失败计数，再把结果反馈给上层。
 */
void OneNetApiClient::onReplyFinished(QNetworkReply *reply)
{
    requestInFlight = false;

    const int requestId = reply->property("onenetRequestId").toInt();
    const qint64 startedAtMs = reply->property("onenetStartedAtMs").toLongLong();
    const qint64 durationMs = (startedAtMs > 0)
            ? (QDateTime::currentMSecsSinceEpoch() - startedAtMs)
            : -1;
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString httpReason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    const QByteArray payload = reply->readAll();

    QString errorMessage;
    bool ok = false;

    if (reply->error() != QNetworkReply::NoError) {
        errorMessage = QStringLiteral("API请求失败：%1 [%2]，HTTP=%3 %4")
                .arg(reply->errorString())
                .arg(networkErrorName(reply->error()))
                .arg(httpStatus > 0 ? QString::number(httpStatus) : QStringLiteral("--"))
                .arg(httpReason.isEmpty() ? QStringLiteral("--") : httpReason);

        qWarning().noquote() << QStringLiteral("[onenet] 请求 #%1 失败 duration=%2ms error=%3 status=%4 reason=%5")
                                .arg(requestId)
                                .arg(durationMs)
                                .arg(networkErrorName(reply->error()))
                                .arg(httpStatus > 0 ? QString::number(httpStatus) : QStringLiteral("--"))
                                .arg(httpReason.isEmpty() ? QStringLiteral("--") : httpReason);
    } else {
        ok = parsePropertyResponse(payload, &errorMessage);
        if (!ok && errorMessage.isEmpty()) {
            errorMessage = QStringLiteral("API响应解析失败");
        }
        if (!ok) {
            qWarning().noquote() << QStringLiteral("[onenet] 请求 #%1 解析失败：%2")
                                    .arg(requestId)
                                    .arg(errorMessage);
        }
    }

    /* 成功则清零失败计数并上报在线态；失败则累计失败次数并上报错误。 */
    if (ok) {
        consecutiveFailures = 0;
        if (!cloudOnline) {
            cloudOnline = true;
            emit connectedChanged(true);
        }
    } else {
        consecutiveFailures++;
        if (cloudOnline) {
            cloudOnline = false;
            emit connectedChanged(false);
        }
        emit errorOccurred(errorMessage);
    }

    reply->deleteLater();
}

/*
 * 解析平台返回的设备属性响应。
 * 运行流程：校验返回码、提取属性数组、统一时间和字段格式，再转为主界面可复用的 params JSON。
 */
bool OneNetApiClient::parsePropertyResponse(const QByteArray &payload, QString *errorMessage)
{
    /* 第一步：校验响应最外层是否是成功 JSON。 */
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        *errorMessage = QStringLiteral("返回数据不是JSON对象");
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("code")).toInt(-1) != 0) {
        *errorMessage = QStringLiteral("平台返回错误：%1")
                .arg(root.value(QStringLiteral("msg")).toString(QStringLiteral("unknown")));
        return false;
    }

    /* 第二步：兼容 data/list/items 等不同字段，把属性数组统一提取出来。 */
    QJsonArray properties;
    const QJsonValue dataValue = root.value(QStringLiteral("data"));
    if (dataValue.isArray()) {
        properties = dataValue.toArray();
    } else if (dataValue.isObject()) {
        const QJsonObject dataObject = dataValue.toObject();
        if (dataObject.value(QStringLiteral("list")).isArray()) {
            properties = dataObject.value(QStringLiteral("list")).toArray();
        } else if (dataObject.value(QStringLiteral("data")).isArray()) {
            properties = dataObject.value(QStringLiteral("data")).toArray();
        } else if (dataObject.value(QStringLiteral("items")).isArray()) {
            properties = dataObject.value(QStringLiteral("items")).toArray();
        }
    }

    if (properties.isEmpty()) {
        *errorMessage = QStringLiteral("属性列表为空");
        return false;
    }

    QMap<QString, QJsonValue> mappedValues;
    QDateTime latestReportTimeUtc;

    /* 第三步：把各种时间格式统一转换成 UTC 时间，后续用于在线性判断。 */
    auto parseTimeValue = [](const QJsonValue &value) -> QDateTime {
        if (value.isDouble()) {
            const qint64 raw = static_cast<qint64>(value.toDouble());
            if (raw > 1000000000000LL) {
                return QDateTime::fromMSecsSinceEpoch(raw, Qt::UTC);
            }
            if (raw > 1000000000LL) {
                return QDateTime::fromSecsSinceEpoch(raw, Qt::UTC);
            }
            return QDateTime();
        }

        if (!value.isString()) {
            return QDateTime();
        }

        const QString text = value.toString().trimmed();
        if (text.isEmpty()) {
            return QDateTime();
        }

        bool numericOk = false;
        const double numeric = text.toDouble(&numericOk);
        if (numericOk) {
            const qint64 raw = static_cast<qint64>(numeric);
            if (raw > 1000000000000LL) {
                return QDateTime::fromMSecsSinceEpoch(raw, Qt::UTC);
            }
            if (raw > 1000000000LL) {
                return QDateTime::fromSecsSinceEpoch(raw, Qt::UTC);
            }
        }

        QDateTime dt = QDateTime::fromString(text, Qt::ISODateWithMs);
        if (!dt.isValid()) {
            dt = QDateTime::fromString(text, Qt::ISODate);
        }
        if (!dt.isValid()) {
            dt = QDateTime::fromString(text, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        }
        if (dt.isValid() && dt.timeSpec() == Qt::LocalTime) {
            dt = dt.toUTC();
        }
        return dt;
    };

    /* 第四步：遍历属性列表，收集 identifier -> value 映射，并找出最近一次上报时间。 */
    for (int i = 0; i < properties.size(); ++i) {
        const QJsonObject item = properties.at(i).toObject();
        QString identifier = item.value(QStringLiteral("identifier")).toString();
        if (identifier.isEmpty()) {
            identifier = item.value(QStringLiteral("id")).toString();
        }

        if (identifier.isEmpty()) {
            continue;
        }

        QJsonValue value = item.value(QStringLiteral("value"));
        if (value.isObject()) {
            const QJsonValue nested = value.toObject().value(QStringLiteral("value"));
            if (!nested.isUndefined()) {
                value = nested;
            }
        }
        if (!value.isUndefined() && !value.isNull()) {
            mappedValues.insert(identifier, value);
        }

        static const char* const kTimeKeys[] = {
            "time", "last_time", "at", "update_time", "updated_at", "create_time"
        };
        for (int keyIndex = 0; keyIndex < static_cast<int>(sizeof(kTimeKeys) / sizeof(kTimeKeys[0])); ++keyIndex) {
            const QDateTime candidate = parseTimeValue(item.value(QString::fromLatin1(kTimeKeys[keyIndex])));
            if (candidate.isValid() && (!latestReportTimeUtc.isValid() || candidate > latestReportTimeUtc)) {
                latestReportTimeUtc = candidate;
            }
        }
    }

    /* 第五步：把平台字段名归一化到 STM32/UI 使用的统一标识。 */
    const QJsonValue tempValue = mappedValues.value(QStringLiteral("temp"), QJsonValue(0));
    const QJsonValue humiValue = mappedValues.value(QStringLiteral("humi"), QJsonValue(0));
    const QJsonValue mq2Value = mappedValues.value(QStringLiteral("mq2"), QJsonValue(0));
    const QJsonValue mq2PpmValue = mappedValues.value(QStringLiteral("mq2_ppm"), QJsonValue(0.0));
    QJsonValue sr501Value = mappedValues.value(QStringLiteral("sr501"), QJsonValue());
    if (sr501Value.isUndefined() || sr501Value.isNull()) {
        sr501Value = mappedValues.value(QStringLiteral("pir"), QJsonValue());
    }
    if (sr501Value.isUndefined() || sr501Value.isNull()) {
        sr501Value = mappedValues.value(QStringLiteral("human"), QJsonValue());
    }
    if (sr501Value.isUndefined() || sr501Value.isNull()) {
        sr501Value = mappedValues.value(QStringLiteral("human_detect"), QJsonValue(0));
    }

    /* 第六步：重新打包成统一的 params/value 结构，供主界面直接复用现有解析逻辑。 */
    QJsonObject params;
    auto addValue = [&](const QString &name, const QJsonValue &value) {
        QJsonObject data;
        data.insert(QStringLiteral("value"), value);
        params.insert(name, data);
    };

    addValue(QStringLiteral("temp"), tempValue);
    addValue(QStringLiteral("humi"), humiValue);
    addValue(QStringLiteral("mq2"), mq2Value);
    addValue(QStringLiteral("mq2_ppm"), mq2PpmValue);
    addValue(QStringLiteral("sr501"), sr501Value);
    if (latestReportTimeUtc.isValid()) {
        addValue(QStringLiteral("cloud_report_time"), latestReportTimeUtc.toString(Qt::ISODateWithMs));
    }

    QJsonObject normalized;
    normalized.insert(QStringLiteral("params"), params);

    /* 第七步：若最近上报时间过旧，则直接判定设备离线。 */
    if (!latestReportTimeUtc.isValid()) {
        *errorMessage = QStringLiteral("未找到设备上报时间");
        qWarning().noquote() << QStringLiteral("[onenet] 平台返回中未解析到任何有效上报时间字段。");
        return false;
    }

    const qint64 staleMs = latestReportTimeUtc.msecsTo(QDateTime::currentDateTimeUtc());
    if (staleMs > kDeviceDataStaleMs) {
        *errorMessage = QStringLiteral("设备离线：最近上报已超时 %1 秒").arg(staleMs / 1000);
        qWarning().noquote() << QStringLiteral("[onenet] 设备最新上报时间=%1，距当前 UTC 已过 %2 ms，按离线处理。")
                                .arg(latestReportTimeUtc.toString(Qt::ISODate))
                                .arg(staleMs);
        return false;
    }

    emit sensorJsonReceived(QString::fromUtf8(QJsonDocument(normalized).toJson(QJsonDocument::Compact)));
    return true;
}
