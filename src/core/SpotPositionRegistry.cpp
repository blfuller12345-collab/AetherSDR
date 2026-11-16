#include "SpotPositionRegistry.h"

#include "MaidenheadLocator.h"

#include <QMutexLocker>

#include <cmath>

namespace AetherSDR {

SpotPositionRegistry& SpotPositionRegistry::instance()
{
    static SpotPositionRegistry registry;
    return registry;
}

QString SpotPositionRegistry::keyFor(const QString& call)
{
    return call.trimmed().toUpper();
}

void SpotPositionRegistry::rememberLatLon(const QString& call, double lat, double lon,
                                          Source source, qint64 nowMs)
{
    const QString key = keyFor(call);
    if (key.isEmpty() || !std::isfinite(lat) || !std::isfinite(lon)
        || lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0
        || (lat == 0.0 && lon == 0.0)) {
        return;
    }
    QMutexLocker lock(&m_mutex);
    const auto it = m_positions.constFind(key);
    if (it != m_positions.constEnd() && nowMs - it->atMs < kFreshMs
        && static_cast<int>(it->source) > static_cast<int>(source)) {
        return;  // keep a fresher, more exact position (a park over a grid)
    }
    m_positions.insert(key, Position{lat, lon, source, nowMs});
}

void SpotPositionRegistry::rememberGrid(const QString& call, const QString& grid,
                                        Source source, qint64 nowMs)
{
    const QString g = grid.trimmed();
    if (g.size() < 4)
        return;
    double lat = 0.0, lon = 0.0;
    if (!MaidenheadLocator::toLatLon(g.toUpper(), lat, lon))
        return;
    rememberLatLon(call, lat, lon, source, nowMs);
}

std::optional<SpotPositionRegistry::Position>
SpotPositionRegistry::lookup(const QString& call, qint64 nowMs) const
{
    QMutexLocker lock(&m_mutex);
    const auto it = m_positions.constFind(keyFor(call));
    if (it == m_positions.constEnd() || nowMs - it->atMs >= kFreshMs)
        return std::nullopt;
    return *it;
}

void SpotPositionRegistry::clear()
{
    QMutexLocker lock(&m_mutex);
    m_positions.clear();
}

} // namespace AetherSDR
