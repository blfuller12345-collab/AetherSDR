#include "core/MqttSettings.h"

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

    {
        const QVector<MqttTopicDef> topics = parseMqttTopicConfig(
            QStringLiteral(" *rotator/pos, station/log, *ant/selected, , plain "));
        ok &= expect(topics.size() == 4, "legacy topic list skips empty entries");
        ok &= expect(topics.value(0).topic == QStringLiteral("rotator/pos")
                         && topics.value(0).displayOnPan,
                     "star-prefixed topic marks display-on-panadapter");
        ok &= expect(topics.value(1).topic == QStringLiteral("station/log")
                         && !topics.value(1).displayOnPan,
                     "plain topic is user subscription only");
        ok &= expect(topics.value(2).topic == QStringLiteral("ant/selected")
                         && topics.value(2).displayOnPan,
                     "later star-prefixed topic keeps display flag");
        ok &= expect(serializeMqttTopicConfig(topics)
                         == QStringLiteral("*rotator/pos, station/log, *ant/selected, plain"),
                     "topic serialization preserves legacy star convention");
    }

    {
        const QStringList internal = internalMqttSubscriptionTopics();
        ok &= expect(internal.contains(QStringLiteral("aethersdr/antenna/name/+")),
                     "internal subscriptions include per-antenna alias wildcard");
        ok &= expect(internal.contains(QStringLiteral("aethersdr/antenna/names")),
                     "internal subscriptions include bulk antenna alias topic");

        const QStringList subscriptions = mqttSubscriptionTopics({
            QStringLiteral("station/log"),
            QStringLiteral("aethersdr/antenna/name/+"),
            QStringLiteral("station/log"),
        });
        ok &= expect(subscriptions.size() == 3,
                     "subscription list deduplicates user and internal topics");
        ok &= expect(subscriptions.value(0) == QStringLiteral("station/log"),
                     "subscription list keeps user topic order");
        ok &= expect(subscriptions.contains(QStringLiteral("aethersdr/antenna/names")),
                     "subscription list appends missing internal topics");
    }

    {
        const QVector<MqttButtonDef> buttons{
            {QStringLiteral("CW"), QStringLiteral("rotator/cmd"), QStringLiteral("CW")},
            {QStringLiteral("Stop"), QStringLiteral("rotator/cmd"), QStringLiteral("STOP")},
        };
        const QVector<MqttButtonDef> roundTrip = mqttButtonsFromJson(mqttButtonsToJson(buttons));
        ok &= expect(roundTrip.size() == 2, "publish button JSON round-trips count");
        ok &= expect(roundTrip.value(0).label == QStringLiteral("CW")
                         && roundTrip.value(0).topic == QStringLiteral("rotator/cmd")
                         && roundTrip.value(0).payload == QStringLiteral("CW"),
                     "publish button JSON preserves first button");
        ok &= expect(roundTrip.value(1).label == QStringLiteral("Stop")
                         && roundTrip.value(1).topic == QStringLiteral("rotator/cmd")
                         && roundTrip.value(1).payload == QStringLiteral("STOP"),
                     "publish button JSON preserves second button");
    }

    {
        // #7136: optional per-subscription label survives a JSON round trip,
        // and is only written when set.
        const QVector<MqttTopicDef> topics{
            {QStringLiteral("lps/hf/status"), true, QStringLiteral("HF Coax")},
            {QStringLiteral("lps/vhf/status"), true, QString()},
            {QStringLiteral("station/log"), false, QStringLiteral("  ")},
        };
        const QString json = mqttTopicsToJson(topics);
        ok &= expect(json.count(QStringLiteral("\"label\"")) == 1,
                     "topic JSON writes a label only when one is set");
        const QVector<MqttTopicDef> roundTrip = mqttTopicsFromJson(json);
        ok &= expect(roundTrip.size() == 3, "topic JSON round-trips count");
        ok &= expect(roundTrip.value(0).label == QStringLiteral("HF Coax")
                         && roundTrip.value(0).displayOnPan,
                     "topic JSON preserves label and display flag");
        ok &= expect(roundTrip.value(1).label.isEmpty()
                         && roundTrip.value(2).label.isEmpty(),
                     "blank or whitespace label reads back empty");

        const QVector<MqttTopicDef> oldConfig = mqttTopicsFromJson(QStringLiteral(
            "[{\"topic\":\"rotator/pos\",\"displayOnPan\":true}]"));
        ok &= expect(oldConfig.size() == 1
                         && oldConfig.value(0).topic == QStringLiteral("rotator/pos")
                         && oldConfig.value(0).displayOnPan
                         && oldConfig.value(0).label.isEmpty(),
                     "topic config saved before labels existed still loads");
    }

    {
        // #7136: overlay name is the label when set, else the last segment,
        // so two topics ending in "status" can carry distinct names.
        ok &= expect(mqttTopicDisplayLabel({QStringLiteral("lps/hf/status"), true,
                                            QStringLiteral("HF Coax")})
                         == QStringLiteral("HF Coax"),
                     "display label uses the subscription label when set");
        ok &= expect(mqttTopicDisplayLabel({QStringLiteral("lps/vhf/status"), true, {}})
                         == QStringLiteral("status"),
                     "display label falls back to the last topic segment");
        ok &= expect(mqttTopicDisplayLabel({QStringLiteral("plain"), true, {}})
                         == QStringLiteral("plain"),
                     "display label of a single-segment topic is the topic");
    }

    return ok ? 0 : 1;
}
