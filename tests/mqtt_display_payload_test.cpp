// Display-on-pan MQTT payloads: plain text, or JSON {"text","color"}.
#include "core/MqttDisplayPayload.h"

#include <iostream>

using namespace AetherSDR;

namespace {
bool expect(bool condition, const char* label)
{
    std::cout << (condition ? "[ OK ] " : "[FAIL] ") << label << '\n';
    return condition;
}
} // namespace

int main()
{
    bool ok = true;

    auto plain = parseMqttDisplayPayload("Coax closed");
    ok &= expect(plain.text == "Coax closed" && plain.color.isEmpty(), "plain text, default color");

    auto green = parseMqttDisplayPayload(R"({"text":"Coax closed","color":"green"})");
    ok &= expect(green.text == "Coax closed" && green.color == "#22c55e", "named color");

    auto hex = parseMqttDisplayPayload(R"({"text":"OPEN","color":"#F00"})");
    ok &= expect(hex.color == "#ff0000", "short hex expands, case folded");

    auto bad = parseMqttDisplayPayload(R"({"text":"x","color":"chartreuse-ish"})");
    ok &= expect(bad.text == "x" && bad.color.isEmpty(), "unknown color falls back to default");

    auto other = parseMqttDisplayPayload(R"({"temp":21})");
    ok &= expect(other.text == R"({"temp":21})" && other.color.isEmpty(),
                 "JSON without text is shown as it came");

    auto longText = parseMqttDisplayPayload(QByteArray(120, 'a'));
    ok &= expect(longText.text.size() == 80, "text is capped at 80 characters");

    ok &= expect(normalizedMqttDisplayColor(" Red ") == "#ef4444"
                     && normalizedMqttDisplayColor("#12abEF") == "#12abef"
                     && normalizedMqttDisplayColor("#12345").isEmpty(),
                 "color normalization");

    std::cout << (ok ? "All checks passed\n" : "Some checks FAILED\n");
    return ok ? 0 : 1;
}
