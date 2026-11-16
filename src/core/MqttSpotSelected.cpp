#include "core/MqttSpotSelected.h"

#include <QDateTime>
#include <QTimeZone>

#include <cmath>

namespace AetherSDR {

QJsonObject buildMqttSpotSelectedPayload(const MqttSpotSelectedInputs& in)
{
    QJsonObject obj;
    obj[QStringLiteral("call")] = in.callsign;
    // Hz precision, as a number of MHz: the same unit radio/state uses.
    obj[QStringLiteral("freq")] = std::round(in.freqMhz * 1e6) / 1e6;
    if (!in.mode.isEmpty())    obj[QStringLiteral("mode")] = in.mode;
    if (!in.source.isEmpty())  obj[QStringLiteral("source")] = in.source;
    if (!in.spotter.isEmpty()) obj[QStringLiteral("spotter")] = in.spotter;
    if (!in.comment.isEmpty()) obj[QStringLiteral("comment")] = in.comment;
    if (in.spotTimeMs > 0) {
        obj[QStringLiteral("time")] =
            QDateTime::fromMSecsSinceEpoch(in.spotTimeMs, QTimeZone::utc())
                .toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss'Z'"));
    }
    if (in.haveBearing && std::isfinite(in.bearingDeg) && in.bearingDeg >= 0.0) {
        obj[QStringLiteral("bearing")] =
            static_cast<int>(std::lround(in.bearingDeg)) % 360;
        if (std::isfinite(in.distanceKm) && in.distanceKm >= 0.0) {
            obj[QStringLiteral("distance_km")] = static_cast<int>(std::lround(in.distanceKm));
        }
        obj[QStringLiteral("approx")] = in.approx;
    }
    return obj;
}

}  // namespace AetherSDR
