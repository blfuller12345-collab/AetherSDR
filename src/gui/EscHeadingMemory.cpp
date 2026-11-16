#include "EscHeadingMemory.h"

#include <QJsonDocument>
#include <QJsonValue>

#include <cmath>

namespace AetherSDR {

namespace {

std::optional<int> normalizedHeading(double deg)
{
    if (!std::isfinite(deg) || deg < 0.0 || deg > 360.0)
        return std::nullopt;
    return static_cast<int>(std::lround(deg)) % 360;
}

std::optional<double> numberFrom(const QJsonValue& v)
{
    if (v.isDouble())
        return v.toDouble();
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().trimmed().toDouble(&ok);
        if (ok)
            return d;
    }
    return std::nullopt;
}

} // namespace

std::optional<RotorHeadingReading> parseRotorHeadingPayload(const QByteArray& payload)
{
    const QByteArray trimmed = payload.trimmed();
    if (trimmed.isEmpty())
        return std::nullopt;

    RotorHeadingReading r;
    if (trimmed.startsWith('{')) {
        const QJsonObject obj = QJsonDocument::fromJson(trimmed).object();
        std::optional<double> deg = numberFrom(obj.value(QStringLiteral("heading")));
        if (!deg)
            deg = numberFrom(obj.value(QStringLiteral("azimuth")));
        if (!deg)
            return std::nullopt;
        const auto h = normalizedHeading(*deg);
        if (!h)
            return std::nullopt;
        r.headingDeg = *h;
        r.band = obj.value(QStringLiteral("band")).toString().trimmed();
        r.rotor = obj.value(QStringLiteral("rotor")).toString().trimmed();
        return r;
    }

    bool ok = false;
    const double deg = QString::fromUtf8(trimmed).toDouble(&ok);
    if (!ok)
        return std::nullopt;
    const auto h = normalizedHeading(deg);
    if (!h)
        return std::nullopt;
    r.headingDeg = *h;
    return r;
}

int headingDistanceDeg(int a, int b)
{
    int d = std::abs(((a % 360) + 360) % 360 - ((b % 360) + 360) % 360);
    return d > 180 ? 360 - d : d;
}

// ── HeadingSettleDetector ───────────────────────────────────────────────────

void HeadingSettleDetector::update(int headingDeg, qint64 nowMs)
{
    m_latest = headingDeg;
    if (!m_candidate || headingDistanceDeg(headingDeg, *m_candidate) > kToleranceDeg) {
        m_candidate = headingDeg;
        m_candidateSinceMs = nowMs;
    }
}

std::optional<int> HeadingSettleDetector::poll(qint64 nowMs)
{
    if (!m_candidate || nowMs - m_candidateSinceMs < kSettleMs)
        return std::nullopt;
    if (m_settled && headingDistanceDeg(*m_settled, *m_candidate) <= kToleranceDeg)
        return std::nullopt;
    m_settled = m_candidate;
    return m_settled;
}

std::optional<int> HeadingSettleDetector::stableHeading(qint64 nowMs) const
{
    if (!m_settled || !m_candidate)
        return std::nullopt;
    if (nowMs - m_candidateSinceMs < kSettleMs)
        return std::nullopt;  // moving (or just stopped and not yet confirmed)
    if (headingDistanceDeg(*m_settled, *m_candidate) > kToleranceDeg)
        return std::nullopt;
    return m_settled;
}

void HeadingSettleDetector::reset()
{
    m_candidate.reset();
    m_settled.reset();
    m_latest.reset();
    m_candidateSinceMs = 0;
}

// ── EscHeadingMemory ────────────────────────────────────────────────────────

QString EscHeadingMemory::bandKey(double freqMhz)
{
    struct Band { double lo; double hi; const char* name; };
    static const Band kBands[] = {
        {1.8, 2.0, "160m"},   {3.5, 4.0, "80m"},    {5.25, 5.45, "60m"},
        {7.0, 7.3, "40m"},    {10.1, 10.15, "30m"}, {14.0, 14.35, "20m"},
        {18.068, 18.168, "17m"}, {21.0, 21.45, "15m"}, {24.89, 24.99, "12m"},
        {28.0, 29.7, "10m"},  {50.0, 54.0, "6m"},   {70.0, 71.0, "4m"},
        {144.0, 148.0, "2m"}, {219.0, 225.0, "1.25m"}, {420.0, 450.0, "70cm"},
        {902.0, 928.0, "33cm"}, {1240.0, 1300.0, "23cm"},
    };
    for (const Band& b : kBands) {
        if (freqMhz >= b.lo && freqMhz < b.hi)
            return QString::fromLatin1(b.name);
    }
    if (!std::isfinite(freqMhz) || freqMhz <= 0.0)
        return QString();
    return QStringLiteral("%1MHz").arg(static_cast<long long>(std::floor(freqMhz)));
}

int EscHeadingMemory::bucketFor(int headingDeg)
{
    const int h = ((headingDeg % 360) + 360) % 360;
    return ((h + kBucketDeg / 2) / kBucketDeg) % (360 / kBucketDeg);
}

QString EscHeadingMemory::key(const QString& band, int bucket)
{
    return band + QLatin1Char('|') + QString::number(bucket);
}

void EscHeadingMemory::store(const QString& band, int headingDeg, EscAutoSweep::Point point)
{
    if (band.isEmpty())
        return;
    m_points.insert(key(band, bucketFor(headingDeg)), EscAutoSweep::normalized(point));
}

std::optional<EscAutoSweep::Point> EscHeadingMemory::recall(const QString& band,
                                                            int headingDeg) const
{
    const auto it = m_points.constFind(key(band, bucketFor(headingDeg)));
    if (it == m_points.constEnd())
        return std::nullopt;
    return it.value();
}

int EscHeadingMemory::countForBand(const QString& band) const
{
    const QString prefix = band + QLatin1Char('|');
    int n = 0;
    for (auto it = m_points.constBegin(); it != m_points.constEnd(); ++it) {
        if (it.key().startsWith(prefix))
            ++n;
    }
    return n;
}

void EscHeadingMemory::clearBand(const QString& band)
{
    const QString prefix = band + QLatin1Char('|');
    for (auto it = m_points.begin(); it != m_points.end();) {
        if (it.key().startsWith(prefix))
            it = m_points.erase(it);
        else
            ++it;
    }
}

QJsonObject EscHeadingMemory::toJson() const
{
    QJsonObject points;
    for (auto it = m_points.constBegin(); it != m_points.constEnd(); ++it) {
        QJsonObject p;
        p.insert(QStringLiteral("phase"), it.value().phaseDeg);
        p.insert(QStringLiteral("gain"), it.value().gainPct);
        points.insert(it.key(), p);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("points"), points);
    return root;
}

EscHeadingMemory EscHeadingMemory::fromJson(const QJsonObject& obj)
{
    EscHeadingMemory mem;
    if (obj.value(QStringLiteral("version")).toInt() != 1)
        return mem;
    const QJsonObject points = obj.value(QStringLiteral("points")).toObject();
    for (auto it = points.constBegin(); it != points.constEnd(); ++it) {
        const QString k = it.key();
        const int bar = k.lastIndexOf(QLatin1Char('|'));
        if (bar <= 0)
            continue;
        bool ok = false;
        const int bucket = k.mid(bar + 1).toInt(&ok);
        if (!ok || bucket < 0 || bucket >= 360 / kBucketDeg)
            continue;
        const QJsonObject p = it.value().toObject();
        if (!p.contains(QStringLiteral("phase")) || !p.contains(QStringLiteral("gain")))
            continue;
        EscAutoSweep::Point pt{p.value(QStringLiteral("phase")).toInt(),
                               p.value(QStringLiteral("gain")).toInt()};
        mem.m_points.insert(key(k.left(bar), bucket), EscAutoSweep::normalized(pt));
    }
    return mem;
}

} // namespace AetherSDR
