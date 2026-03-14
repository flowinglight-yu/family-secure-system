#include "securityuiutils.h"

#include <QJsonObject>
#include <QJsonValue>

namespace {

/*
 * 把 JSON 字段统一转换成数值。
 * 说明：兼容布尔、数字和常见字符串表达，供传感器字段解析复用。
 */
double sensorJsonValueToNumber(const QJsonValue &value)
{
    if (value.isBool()) {
        return value.toBool() ? 1.0 : 0.0;
    }

    if (value.isDouble()) {
        return value.toDouble();
    }

    if (value.isString()) {
        const QString text = value.toString().trimmed();
        const QString lowered = text.toLower();
        if (lowered == QStringLiteral("true")
                || lowered == QStringLiteral("on")
                || lowered == QStringLiteral("yes")
                || lowered == QStringLiteral("detected")
                || text == QStringLiteral("有人")
                || text == QStringLiteral("有")) {
            return 1.0;
        }
        if (lowered == QStringLiteral("false")
                || lowered == QStringLiteral("off")
                || lowered == QStringLiteral("no")
                || lowered == QStringLiteral("none")
                || text == QStringLiteral("无人")
                || text == QStringLiteral("无")) {
            return 0.0;
        }

        bool ok = false;
        const double parsed = text.toDouble(&ok);
        return ok ? parsed : 0.0;
    }

    return 0.0;
}

/*
 * 尝试读取单个传感器数值字段。
 * 说明：兼容平铺 JSON 和 params/value 嵌套结构，读取成功时写回输出参数。
 */
bool tryReadSensorNumberField(const QJsonObject &object,
                              const QJsonObject &params,
                              const QString &name,
                              double *value)
{
    if (value == nullptr) {
        return false;
    }

    auto extractNumber = [&](const QJsonValue &field) -> bool {
        if (field.isUndefined() || field.isNull()) {
            return false;
        }

        if (field.isObject()) {
            const QJsonValue nestedValue = field.toObject().value(QStringLiteral("value"));
            if (nestedValue.isUndefined() || nestedValue.isNull()) {
                return false;
            }
            *value = sensorJsonValueToNumber(nestedValue);
            return true;
        }

        *value = sensorJsonValueToNumber(field);
        return true;
    };

    if (!params.isEmpty() && extractNumber(params.value(name))) {
        return true;
    }

    return extractNumber(object.value(name));
}

} // namespace

namespace SecurityUiUtils {

/*
 * 校验锁屏密码格式是否符合约束。
 * 说明：当前项目要求密码必须是 4~6 位纯数字。
 */
bool isValidLockPassword(const QString &password)
{
    if (password.size() < 4 || password.size() > 6) {
        return false;
    }

    for (int i = 0; i < password.size(); ++i) {
        if (!password.at(i).isDigit()) {
            return false;
        }
    }

    return true;
}

/*
 * 生成历史图表的 X 轴标签。
 * 说明：当前只支持 1 小时和 24 小时两个窗口，因此返回固定三段刻度。
 */
QStringList historyAxisLabels(int hours)
{
    if (hours <= 1) {
        return QStringList() << QStringLiteral("0")
                             << QStringLiteral("30")
                             << QStringLiteral("60");
    }

    return QStringList() << QStringLiteral("0")
                         << QStringLiteral("12")
                         << QStringLiteral("24");
}

/*
 * 读取单个传感器数值字段并带默认值回退。
 * 说明：这是 tryReadSensorNumberField() 的包装层，适合直接用于业务取值。
 */
double readSensorNumberField(const QJsonObject &object,
                             const QJsonObject &params,
                             const QString &name,
                             double fallback)
{
    double value = 0.0;
    return tryReadSensorNumberField(object, params, name, &value) ? value : fallback;
}

/*
 * 按候选字段顺序读取第一个可用传感器值。
 * 说明：用于兼容 PIR 等存在多种平台命名的字段。
 */
double readFirstAvailableSensorNumber(const QJsonObject &object,
                                      const QJsonObject &params,
                                      const QStringList &names,
                                      double fallback)
{
    for (int i = 0; i < names.size(); ++i) {
        double value = 0.0;
        if (tryReadSensorNumberField(object, params, names.at(i), &value)) {
            return value;
        }
    }

    return fallback;
}

/* 生成温度来源标签文案。 */
QString tempSourceText(bool tempDanger)
{
    return tempDanger
            ? QStringLiteral("DHT11 传感器 | 温度偏高")
            : QStringLiteral("DHT11 传感器");
}

/* 生成 PIR 状态文案。 */
QString pirStateText(bool detected)
{
    return detected ? QStringLiteral("⚠ 门外有人！") : QStringLiteral("门外无人");
}

/* 生成 WiFi 状态文案。 */
QString wifiStateText(bool online)
{
    return online ? QStringLiteral("WIFI：在线") : QStringLiteral("WIFI：离线");
}

/* 生成云端在线状态文案。 */
QString cloudStateText(bool online)
{
    return online ? QStringLiteral("云端：在线") : QStringLiteral("云端：离线");
}

/* 生成 OneNet 在线状态文案。 */
QString oneNetStateText(bool online)
{
    return online ? QStringLiteral("OneNet：在线") : QStringLiteral("OneNet：离线");
}

/* 生成底栏网络状态文案。 */
QString bottomNetworkStateText(bool online)
{
    return online ? QStringLiteral("📶 网络：在线") : QStringLiteral("📶 网络：离线");
}

/* 返回本地配置文件路径。 */
QString securityConfigPath()
{
    return QStringLiteral("./security_ui.ini");
}

} // namespace SecurityUiUtils