#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QtGlobal>

namespace AetherSDR {

// Looks up spotted callsigns on QRZ in the background, slowly, so the
// panadapter's distance/heading uses the station's real position instead of
// the country center (#6603). A country center is hundreds of miles and tens
// of degrees off for a big country, so the tooltip shows no heading at all
// until a real position is known.
//
// QRZ blocks an IP that sends too many requests, so this is deliberately
// slow and bounded: at most one request every kMinIntervalMs and kMaxPerHour
// in any rolling hour, each callsign tried at most once per session, and
// nothing is retried. HF cluster traffic is far more than that budget, so
// spots at or above kPriorityFreqMhz (where a beam heading matters most and
// spots are few) jump the queue; a full queue drops new HF requests. Already-cached calls cost
// nothing. The lookups go through CallsignLookupService, so they share its
// cache and its "QRZ enabled + credentials" gate.
class SpotQrzPrefetcher : public QObject {
    Q_OBJECT
public:
    static constexpr int kMinIntervalMs = 10'000;
    static constexpr int kMaxPerHour = 30;
    static constexpr double kPriorityFreqMhz = 50.0;
    static constexpr int kMaxQueue = 40;

    // Pure rate gate (unit-tested): one request per kMinIntervalMs and at
    // most kMaxPerHour in any rolling hour.
    struct Gate {
        QVector<qint64> sentMs;
        bool allow(qint64 nowMs) const;
        void record(qint64 nowMs);
    };

    static SpotQrzPrefetcher& instance();

    // A spot arrived (or changed). Queues one quiet lookup when worthwhile.
    void request(const QString& call, double freqMhz);

    // For tests and diagnostics.
    int queuedCount() const { return m_queue.size(); }

private:
    SpotQrzPrefetcher();
    void pump();

    QStringList m_queue;
    QSet<QString> m_tried;
    Gate m_gate;
    QTimer m_timer;
};

} // namespace AetherSDR
