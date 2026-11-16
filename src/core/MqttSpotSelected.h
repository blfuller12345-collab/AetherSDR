#pragma once

// Payload builder for the MQTT `aethersdr/spot/selected` topic: published when
// the operator clicks a spot on the panadapter, so a station controller
// (Node-RED and the like) can act on it, for example turn the right rotator
// or fill in its own "go to". A pure function over a plain struct, like
// MqttRadioState, so tests/mqtt_spot_selected_test.cpp reaches every shape.
//
// Fields are keyed by PRESENCE: `bearing`, `distance_km` and `approx` appear
// only when AetherSDR knows the heading (the same short-path bearing the spot
// tooltip shows); a missing heading is never sent as 0.

#include <QJsonObject>
#include <QString>

namespace AetherSDR {

struct MqttSpotSelectedInputs {
    QString callsign;
    double  freqMhz = 0.0;
    QString mode;
    QString source;        // "DX Cluster", "RBN", "POTA", ...
    QString spotter;
    QString comment;
    qint64  spotTimeMs = 0;   // when the spot was made; 0 = unknown

    bool    haveBearing = false;
    double  bearingDeg = 0.0;
    double  distanceKm = 0.0;
    bool    approx = false;   // position is the DXCC country centre
};

QJsonObject buildMqttSpotSelectedPayload(const MqttSpotSelectedInputs& in);

}  // namespace AetherSDR
