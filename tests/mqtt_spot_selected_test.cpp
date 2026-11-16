// aethersdr/spot/selected payload: fields keyed by presence.
#include "core/MqttSpotSelected.h"

#include <QJsonDocument>
#include <iostream>

using namespace AetherSDR;

namespace {

bool expect(bool condition, const char* label)
{
    std::cout << (condition ? "[ OK ] " : "[FAIL] ") << label << '\n';
    return condition;
}

}  // namespace

int main()
{
    bool ok = true;

    MqttSpotSelectedInputs full;
    full.callsign = QStringLiteral("W1AW");
    full.freqMhz = 144.2000004;
    full.mode = QStringLiteral("USB");
    full.source = QStringLiteral("DX Cluster");
    full.spotter = QStringLiteral("K5RBC");
    full.comment = QStringLiteral("loud");
    full.spotTimeMs = 1794844800000LL;   // 2026-11-16T16:00:00Z
    full.haveBearing = true;
    full.bearingDeg = 52.4;
    full.distanceKm = 2594.3;
    full.approx = true;
    const QJsonObject o = buildMqttSpotSelectedPayload(full);
    ok &= expect(o.value(QStringLiteral("call")).toString() == QStringLiteral("W1AW"), "call");
    ok &= expect(qAbs(o.value(QStringLiteral("freq")).toDouble() - 144.2) < 1e-9,
                 "freq in MHz, rounded to the hertz");
    ok &= expect(o.value(QStringLiteral("mode")).toString() == QStringLiteral("USB")
                     && o.value(QStringLiteral("source")).toString() == QStringLiteral("DX Cluster")
                     && o.value(QStringLiteral("spotter")).toString() == QStringLiteral("K5RBC"),
                 "mode, source and spotter");
    ok &= expect(o.value(QStringLiteral("time")).toString() == QStringLiteral("2026-11-16T16:00:00Z"),
                 "time in UTC ISO-8601");
    ok &= expect(o.value(QStringLiteral("bearing")).toInt() == 52
                     && o.value(QStringLiteral("distance_km")).toInt() == 2594
                     && o.value(QStringLiteral("distance_mi")).toInt() == 1612
                     && o.value(QStringLiteral("approx")).toBool(),
                 "bearing, distance and approx when known");

    MqttSpotSelectedInputs bare;
    bare.callsign = QStringLiteral("JA1XYZ");
    bare.freqMhz = 14.025;
    const QJsonObject b = buildMqttSpotSelectedPayload(bare);
    ok &= expect(!b.contains(QStringLiteral("bearing")) && !b.contains(QStringLiteral("distance_km"))
                     && !b.contains(QStringLiteral("distance_mi"))
                     && !b.contains(QStringLiteral("approx")),
                 "no heading fields when the heading is unknown (never 0)");
    ok &= expect(!b.contains(QStringLiteral("time")) && !b.contains(QStringLiteral("mode"))
                     && !b.contains(QStringLiteral("spotter")),
                 "empty fields are left out");

    MqttSpotSelectedInputs round = bare;
    round.haveBearing = true;
    round.bearingDeg = 359.6;
    round.distanceKm = 10.0;
    ok &= expect(buildMqttSpotSelectedPayload(round).value(QStringLiteral("bearing")).toInt() == 0,
                 "a bearing that rounds to 360 is 0");

    std::cout << QJsonDocument(o).toJson(QJsonDocument::Compact).toStdString() << '\n';
    return ok ? 0 : 1;
}
