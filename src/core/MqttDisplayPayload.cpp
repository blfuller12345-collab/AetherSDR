#include "MqttDisplayPayload.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace AetherSDR {

QString normalizedMqttDisplayColor(const QString& color)
{
    const QString c = color.trimmed().toLower();
    if (c.isEmpty())
        return QString();
    struct Named { const char* name; const char* hex; };
    static const Named kNamed[] = {
        {"green", "#22c55e"}, {"red", "#ef4444"},   {"yellow", "#eab308"},
        {"orange", "#f97316"}, {"blue", "#3b82f6"}, {"white", "#ffffff"},
        {"grey", "#9ca3af"},  {"gray", "#9ca3af"},
    };
    for (const Named& n : kNamed) {
        if (c == QLatin1String(n.name))
            return QString::fromLatin1(n.hex);
    }
    static const QRegularExpression kHex6(QStringLiteral("^#[0-9a-f]{6}$"));
    static const QRegularExpression kHex3(QStringLiteral("^#[0-9a-f]{3}$"));
    if (kHex6.match(c).hasMatch())
        return c;
    if (kHex3.match(c).hasMatch()) {
        return QStringLiteral("#%1%1%2%2%3%3").arg(c.at(1)).arg(c.at(2)).arg(c.at(3));
    }
    return QString();
}

MqttDisplayPayload parseMqttDisplayPayload(const QByteArray& payload, int maxChars)
{
    MqttDisplayPayload out;
    const QByteArray trimmed = payload.trimmed();
    if (trimmed.startsWith('{')) {
        const QJsonObject obj = QJsonDocument::fromJson(trimmed).object();
        const QJsonValue text = obj.value(QStringLiteral("text"));
        if (text.isString()) {
            out.text = text.toString().left(maxChars);
            out.color = normalizedMqttDisplayColor(obj.value(QStringLiteral("color")).toString());
            return out;
        }
    }
    out.text = QString::fromUtf8(payload).left(maxChars);
    return out;
}

} // namespace AetherSDR
