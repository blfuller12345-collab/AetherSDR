#pragma once

#include "EscHeadingMemory.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <optional>

namespace AetherSDR {

// Where the beam points, as reported over MQTT (aethersdr/rotor/heading), and
// the per-heading Auto ESC memory that goes with it. One shared instance so
// every VFO flag sees the same heading and the same saved settings.
// AetherSDR never talks to the rotator itself: the station controller that
// already owns it reports the heading.
class RotorHeadingFeed : public QObject {
    Q_OBJECT
public:
    static RotorHeadingFeed& instance();

    // A message on the heading topic (already gated by the MQTT settings).
    void handlePayload(const QByteArray& payload);

    bool hasHeading() const { return m_detector.latest().has_value(); }
    std::optional<int> latestHeading() const { return m_detector.latest(); }
    // The heading while the beam is at rest; empty while it turns.
    std::optional<int> stableHeading() const;
    // Whether the reported heading is meant for this band ("" = any band).
    bool appliesTo(const QString& bandKey) const;
    QString reportedBand() const { return m_band; }
    QString reportedRotor() const { return m_rotor; }

    EscHeadingMemory& memory() { return m_memory; }
    void saveMemory();

    bool autoSweepOnNewHeading() const { return m_autoSweep; }
    void setAutoSweepOnNewHeading(bool on);

signals:
    // The beam has stopped on a new heading.
    void headingSettled(int headingDeg);

private:
    RotorHeadingFeed();
    void poll();

    HeadingSettleDetector m_detector;
    QTimer m_pollTimer;
    QString m_band;
    QString m_rotor;
    EscHeadingMemory m_memory;
    bool m_autoSweep{false};
};

} // namespace AetherSDR
