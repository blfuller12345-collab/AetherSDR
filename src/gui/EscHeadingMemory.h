#pragma once

#include "EscAutoSweep.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QtGlobal>

#include <optional>

namespace AetherSDR {

// Auto ESC heading memory: the best ESC phase/gain depends on the direction
// the signal arrives from, so when a station controller reports where the
// beam points (MQTT aethersdr/rotor/heading), Auto ESC results are kept per
// band and per 10-degree heading, and recalled when the beam stops there.
// Pure logic, no timers or widgets, so it can be tested without a radio.

// One heading report. Payload is either a bare number of degrees ("135") or
// JSON {"heading":135, "band":"2m", "rotor":"south"}. "band" limits the
// report to slices on that band (empty = any band); "rotor" is informational.
struct RotorHeadingReading {
    int headingDeg{0};  // 0..359
    QString band;
    QString rotor;
};
std::optional<RotorHeadingReading> parseRotorHeadingPayload(const QByteArray& payload);

// Shortest angular distance between two headings, 0..180.
int headingDistanceDeg(int a, int b);

// Decides when the beam has stopped. Rotator controllers are usually polled
// every few seconds, so "the same value twice" is not proof; the heading must
// stay within kToleranceDeg for kSettleMs. A settled heading is reported once,
// and again only after the beam has moved and stopped somewhere else.
class HeadingSettleDetector {
public:
    static constexpr int kToleranceDeg = 2;
    static constexpr qint64 kSettleMs = 7000;

    void update(int headingDeg, qint64 nowMs);
    // Returns the heading the first time it has been steady for kSettleMs.
    std::optional<int> poll(qint64 nowMs);
    // The heading if the beam is at rest (settled and not moving since).
    std::optional<int> stableHeading(qint64 nowMs) const;
    std::optional<int> latest() const { return m_latest; }
    void reset();

private:
    std::optional<int> m_candidate;
    qint64 m_candidateSinceMs{0};
    std::optional<int> m_settled;
    std::optional<int> m_latest;
};

class EscHeadingMemory {
public:
    static constexpr int kBucketDeg = 10;

    // "160m".."2m", "70cm", "33cm", "23cm"; other frequencies "<N>MHz".
    static QString bandKey(double freqMhz);
    // 0..35: 355..4 is bucket 0, 5..14 is bucket 1, and so on.
    static int bucketFor(int headingDeg);
    static int bucketCenterDeg(int bucket) { return bucket * kBucketDeg; }

    void store(const QString& band, int headingDeg, EscAutoSweep::Point point);
    std::optional<EscAutoSweep::Point> recall(const QString& band, int headingDeg) const;
    int countForBand(const QString& band) const;
    void clearBand(const QString& band);
    int size() const { return m_points.size(); }

    // {"version":1,"points":{"2m|13":{"phase":40,"gain":110},...}}
    QJsonObject toJson() const;
    static EscHeadingMemory fromJson(const QJsonObject& obj);

private:
    static QString key(const QString& band, int bucket);
    QHash<QString, EscAutoSweep::Point> m_points;
};

} // namespace AetherSDR
