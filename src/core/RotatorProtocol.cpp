#include "core/RotatorProtocol.h"

#include "core/AppSettings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <cmath>
#include <utility>

namespace AetherSDR {

namespace Rotator {

namespace {

constexpr const char* kSettingsKey = "Rotators";
constexpr int kMaxBufferBytes = 4096;

}  // namespace

QString protocolKey(Protocol p)
{
    return p == Protocol::Rotctld ? QStringLiteral("rotctld") : QStringLiteral("gs232a");
}

Protocol protocolFromKey(const QString& key)
{
    return key.trimmed().compare(QLatin1String("rotctld"), Qt::CaseInsensitive) == 0
               ? Protocol::Rotctld
               : Protocol::Gs232A;
}

quint16 defaultPort(Protocol p)
{
    return p == Protocol::Rotctld ? 4533 : 4001;
}

int normalizedAzimuth(int deg)
{
    deg %= 360;
    return deg < 0 ? deg + 360 : deg;
}

QByteArray readCommand(Protocol p)
{
    return p == Protocol::Rotctld ? QByteArrayLiteral("p\n") : QByteArrayLiteral("C2\r");
}

QByteArray moveCommand(Protocol p, int azimuthDeg)
{
    const int az = normalizedAzimuth(azimuthDeg);
    if (p == Protocol::Rotctld) {
        return QStringLiteral("P %1 0\n").arg(az).toLatin1();
    }
    return QStringLiteral("M%1\r").arg(az, 3, 10, QLatin1Char('0')).toLatin1();
}

QByteArray stopCommand(Protocol p)
{
    return p == Protocol::Rotctld ? QByteArrayLiteral("S\n") : QByteArrayLiteral("S\r");
}

namespace {

// Next complete line at or after `from`: its text, and where the following
// line starts. Empty optional when no terminator is there yet.
std::optional<std::pair<QString, int>> lineAt(const QByteArray& buffer, int from)
{
    for (int i = from; i < buffer.size(); ++i) {
        if (buffer.at(i) == '\r' || buffer.at(i) == '\n') {
            return std::make_pair(QString::fromLatin1(buffer.mid(from, i - from)).trimmed(),
                                  i + 1);
        }
    }
    return std::nullopt;
}

std::optional<double> plainNumber(const QString& line)
{
    bool ok = false;
    const double v = line.toDouble(&ok);
    if (ok && std::isfinite(v)) return v;
    return std::nullopt;
}

}  // namespace

std::optional<int> takeHeading(Protocol p, QByteArray& buffer)
{
    if (buffer.size() > kMaxBufferBytes) {
        buffer.clear();
        return std::nullopt;
    }

    if (p == Protocol::Rotctld) {
        // A "p" reply is two lines, azimuth then elevation. Take the pair
        // whole, so an elevation is never mistaken for the next azimuth.
        // Anything else at the front ("RPRT 0" after P or S, blank lines)
        // is dropped.
        while (true) {
            const auto first = lineAt(buffer, 0);
            if (!first) return std::nullopt;
            const auto az = plainNumber(first->first);
            if (!az) {
                buffer.remove(0, first->second);
                continue;
            }
            const auto second = lineAt(buffer, first->second);
            if (!second) return std::nullopt;   // wait for the elevation line
            buffer.remove(0, second->second);
            if (*az < -360.0 || *az > 720.0) continue;
            return normalizedAzimuth(static_cast<int>(std::lround(*az)));
        }
    }

    // GS-232A: "+0aaa" or "+0aaa+0eee", or "AZ=aaa" on some interfaces.
    static const QRegularExpression plus(QStringLiteral(R"(^\+0?(\d{3}))"));
    static const QRegularExpression az(QStringLiteral(R"(AZ\s*=\s*(\d{1,3}))"),
                                       QRegularExpression::CaseInsensitiveOption);
    while (true) {
        const auto line = lineAt(buffer, 0);
        if (!line) return std::nullopt;
        buffer.remove(0, line->second);
        if (line->first.isEmpty()) continue;
        QRegularExpressionMatch m = plus.match(line->first);
        if (!m.hasMatch()) m = az.match(line->first);
        if (!m.hasMatch()) continue;
        const int v = m.captured(1).toInt();
        if (v >= 0 && v <= 450) {   // some controllers report overlap past 360
            return normalizedAzimuth(v);
        }
    }
}

QJsonObject toJson(const QList<Config>& rotators)
{
    QJsonArray arr;
    for (const Config& c : rotators) {
        if (c.host.trimmed().isEmpty()) continue;
        QJsonObject o;
        o[QStringLiteral("id")] = c.id;
        o[QStringLiteral("name")] = c.name.trimmed();
        o[QStringLiteral("host")] = c.host.trimmed();
        o[QStringLiteral("port")] = static_cast<int>(c.port);
        o[QStringLiteral("protocol")] = protocolKey(c.protocol);
        o[QStringLiteral("enabled")] = c.enabled;
        arr.append(o);
    }
    QJsonObject root;
    root[QStringLiteral("rotators")] = arr;
    return root;
}

QString uniqueId(const QList<Config>& rotators)
{
    int n = 1;
    auto taken = [&rotators](const QString& id) {
        for (const Config& c : rotators) {
            if (c.id == id) return true;
        }
        return false;
    };
    while (taken(QStringLiteral("rot-%1").arg(n))) ++n;
    return QStringLiteral("rot-%1").arg(n);
}

QList<Config> fromJson(const QJsonObject& obj)
{
    QList<Config> out;
    for (const QJsonValue& v : obj.value(QStringLiteral("rotators")).toArray()) {
        const QJsonObject o = v.toObject();
        Config c;
        c.host = o.value(QStringLiteral("host")).toString().trimmed();
        if (c.host.isEmpty()) continue;
        c.protocol = protocolFromKey(o.value(QStringLiteral("protocol")).toString());
        const int port = o.value(QStringLiteral("port")).toInt(defaultPort(c.protocol));
        c.port = (port > 0 && port <= 65535) ? static_cast<quint16>(port)
                                             : defaultPort(c.protocol);
        c.name = o.value(QStringLiteral("name")).toString().trimmed();
        if (c.name.isEmpty()) c.name = c.host;
        c.enabled = o.value(QStringLiteral("enabled")).toBool(true);
        c.id = o.value(QStringLiteral("id")).toString().trimmed();
        bool dup = c.id.isEmpty();
        for (const Config& seen : out) dup |= seen.id == c.id;
        if (dup) c.id = uniqueId(out);
        out.append(c);
    }
    return out;
}

QList<Config> loadConfigs()
{
    const QString raw = AppSettings::instance().value(kSettingsKey, QString()).toString();
    if (raw.isEmpty()) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    return doc.isObject() ? fromJson(doc.object()) : QList<Config>{};
}

void saveConfigs(const QList<Config>& rotators)
{
    auto& s = AppSettings::instance();
    s.setValue(kSettingsKey,
               QString::fromUtf8(QJsonDocument(toJson(rotators)).toJson(QJsonDocument::Compact)));
    s.save();
}

}  // namespace Rotator

}  // namespace AetherSDR
