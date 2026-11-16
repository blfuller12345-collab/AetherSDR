#include "RotorHeadingFeed.h"

#include "core/AppSettings.h"

#include <QDateTime>
#include <QJsonDocument>

namespace AetherSDR {

namespace {
const QString kMemoryKey = QStringLiteral("EscHeadingMemory");
const QString kAutoSweepKey = QStringLiteral("EscAutoSweepOnNewHeading");

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }
} // namespace

RotorHeadingFeed& RotorHeadingFeed::instance()
{
    static RotorHeadingFeed feed;
    return feed;
}

RotorHeadingFeed::RotorHeadingFeed()
{
    auto& s = AppSettings::instance();
    const QByteArray json = s.value(kMemoryKey, QString()).toString().toUtf8();
    if (!json.isEmpty())
        m_memory = EscHeadingMemory::fromJson(QJsonDocument::fromJson(json).object());
    m_autoSweep = s.value(kAutoSweepKey, QStringLiteral("False")).toString()
                  == QLatin1String("True");

    m_pollTimer.setInterval(1000);
    connect(&m_pollTimer, &QTimer::timeout, this, &RotorHeadingFeed::poll);
}

void RotorHeadingFeed::handlePayload(const QByteArray& payload)
{
    const auto reading = parseRotorHeadingPayload(payload);
    if (!reading)
        return;
    m_band = reading->band;
    m_rotor = reading->rotor;
    m_detector.update(reading->headingDeg, nowMs());
    if (!m_pollTimer.isActive())
        m_pollTimer.start();
}

void RotorHeadingFeed::poll()
{
    if (const auto settled = m_detector.poll(nowMs()))
        emit headingSettled(*settled);
}

std::optional<int> RotorHeadingFeed::stableHeading() const
{
    return m_detector.stableHeading(nowMs());
}

bool RotorHeadingFeed::appliesTo(const QString& bandKey) const
{
    return m_band.isEmpty() || m_band.compare(bandKey, Qt::CaseInsensitive) == 0;
}

void RotorHeadingFeed::saveMemory()
{
    auto& s = AppSettings::instance();
    s.setValue(kMemoryKey, QString::fromUtf8(
        QJsonDocument(m_memory.toJson()).toJson(QJsonDocument::Compact)));
    s.save();
}

void RotorHeadingFeed::setAutoSweepOnNewHeading(bool on)
{
    m_autoSweep = on;
    auto& s = AppSettings::instance();
    s.setValue(kAutoSweepKey, on ? QStringLiteral("True") : QStringLiteral("False"));
    s.save();
}

} // namespace AetherSDR
