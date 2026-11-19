#include "SpotQrzPrefetcher.h"

#include "CallsignLookupService.h"
#include "CallsignUtils.h"

#include <QDateTime>

namespace AetherSDR {

namespace {
constexpr qint64 kHourMs = 3600LL * 1000;
}

bool SpotQrzPrefetcher::Gate::allow(qint64 nowMs) const
{
    if (!sentMs.isEmpty() && nowMs - sentMs.last() < kMinIntervalMs)
        return false;
    int inLastHour = 0;
    for (qint64 t : sentMs) {
        if (nowMs - t < kHourMs)
            ++inLastHour;
    }
    return inLastHour < kMaxPerHour;
}

void SpotQrzPrefetcher::Gate::record(qint64 nowMs)
{
    sentMs.append(nowMs);
    while (!sentMs.isEmpty() && nowMs - sentMs.first() >= kHourMs)
        sentMs.removeFirst();
}

SpotQrzPrefetcher& SpotQrzPrefetcher::instance()
{
    static SpotQrzPrefetcher s;
    return s;
}

SpotQrzPrefetcher::SpotQrzPrefetcher()
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &SpotQrzPrefetcher::pump);
}

void SpotQrzPrefetcher::request(const QString& call, double freqMhz)
{
    const QString norm = Callsigns::normalized(call);
    if (norm.isEmpty() || !Callsigns::isLikelyCallsign(norm) || m_tried.contains(norm))
        return;
    auto& svc = CallsignLookupService::instance();
    if (!svc.enabled() || !svc.hasCredentials())
        return;
    if (svc.hasCachedEntry(norm)) {
        m_tried.insert(norm);   // cached: nothing to fetch this session
        return;
    }
    const bool priority = freqMhz >= kPriorityFreqMhz;
    if (m_queue.size() >= kMaxQueue) {
        if (!priority)
            return;  // HF burst beyond the budget: no heading rather than a wrong one
        m_tried.remove(m_queue.takeLast());  // make room; that call can ask again
    }
    m_tried.insert(norm);
    if (priority)
        m_queue.prepend(norm);
    else
        m_queue.append(norm);
    if (!m_timer.isActive())
        m_timer.start();
}

void SpotQrzPrefetcher::pump()
{
    if (m_queue.isEmpty()) {
        m_timer.stop();
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_gate.allow(now))
        return;
    auto& svc = CallsignLookupService::instance();
    while (!m_queue.isEmpty()) {
        const QString call = m_queue.takeFirst();
        if (svc.hasCachedEntry(call))
            continue;  // looked up meanwhile (Callsign Card) — free
        if (!svc.enabled() || !svc.hasCredentials()) {
            m_queue.clear();
            break;
        }
        m_gate.record(now);
        svc.lookup(call);
        break;
    }
    if (m_queue.isEmpty())
        m_timer.stop();
}

} // namespace AetherSDR
