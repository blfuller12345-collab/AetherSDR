#pragma once

#include <QHash>
#include <QMutex>
#include <QString>
#include <QtGlobal>

#include <optional>

namespace AetherSDR {

// Where a spotted station actually is, when the spot itself says so: a POTA
// spot carries the park's latitude/longitude, and some clusters send the DX
// grid. The radio's spot list drops these, so they are kept here, by
// callsign, for the panadapter's distance/heading (#6603). A portable or
// POTA operator is at the park, not at the address on file, so a position
// from a spot wins over the callsign database while it is fresh.
// Thread-safe: spot clients run on the spot worker thread.
class SpotPositionRegistry {
public:
    enum class Source { Grid = 0, Park = 1 };  // higher wins

    struct Position {
        double lat{0.0};
        double lon{0.0};
        Source source{Source::Grid};
        qint64 atMs{0};
    };

    static constexpr qint64 kFreshMs = 60LL * 60 * 1000;  // one hour

    static SpotPositionRegistry& instance();

    // A park (exact) position. Replaces anything older or less exact.
    void rememberLatLon(const QString& call, double lat, double lon, Source source,
                        qint64 nowMs);
    // A Maidenhead grid (4 or 6 characters); ignored if it doesn't parse.
    void rememberGrid(const QString& call, const QString& grid, Source source, qint64 nowMs);
    std::optional<Position> lookup(const QString& call, qint64 nowMs) const;
    void clear();

private:
    static QString keyFor(const QString& call);
    mutable QMutex m_mutex;
    QHash<QString, Position> m_positions;
};

} // namespace AetherSDR
