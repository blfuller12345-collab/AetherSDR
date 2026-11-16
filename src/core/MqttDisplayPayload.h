#pragma once

#include <QByteArray>
#include <QString>

namespace AetherSDR {

// What a display-on-pan MQTT subscription shows. A plain payload is shown as
// it is. A JSON payload with a "text" field may also carry a "color", so the
// publisher can make a line green when all is well and red when it is not:
//   {"text":"Coax closed","color":"green"}
//   {"text":"COAX OPEN - HF","color":"#ff3030"}
// Named colors: green, red, yellow, orange, blue, white, grey/gray. Anything
// else, or no color, uses the overlay's normal color.
struct MqttDisplayPayload {
    QString text;   // at most maxChars characters
    QString color;  // "#rrggbb", or empty for the default
};

MqttDisplayPayload parseMqttDisplayPayload(const QByteArray& payload, int maxChars = 80);

// "#rrggbb" for a named or hex color, empty if it is not one.
QString normalizedMqttDisplayColor(const QString& color);

} // namespace AetherSDR
