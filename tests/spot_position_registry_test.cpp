// Positions carried by spots (POTA park, cluster grid), #6603.
#include "core/SpotPositionRegistry.h"

#include <cmath>
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
    auto& r = SpotPositionRegistry::instance();
    using S = SpotPositionRegistry::Source;

    r.rememberLatLon("k4abc", 35.6, -83.5, S::Park, 1000);
    auto p = r.lookup("K4ABC", 2000);
    ok &= expect(p && std::abs(p->lat - 35.6) < 1e-9 && p->source == S::Park,
                 "park position stored, callsign case ignored");

    r.rememberGrid("K4ABC", "EM10", S::Grid, 3000);
    ok &= expect(r.lookup("K4ABC", 4000)->source == S::Park,
                 "a fresh park position is not replaced by a grid");

    r.rememberLatLon("K4ABC", 30.0, -90.0, S::Park, 5000);
    ok &= expect(std::abs(r.lookup("K4ABC", 6000)->lat - 30.0) < 1e-9,
                 "a newer park (activator moved) replaces the old one");

    ok &= expect(!r.lookup("K4ABC", 5000 + SpotPositionRegistry::kFreshMs),
                 "positions expire after an hour");
    r.rememberGrid("K4ABC", "EM10", S::Grid, 5000 + SpotPositionRegistry::kFreshMs);
    ok &= expect(r.lookup("K4ABC", 5001 + SpotPositionRegistry::kFreshMs)->source == S::Grid,
                 "an expired park can be replaced by a grid");

    r.rememberGrid("W1AW", "FN31pr", S::Grid, 0);
    auto g = r.lookup("W1AW", 1);
    ok &= expect(g && g->lat > 41.0 && g->lat < 42.0 && g->lon < -72.0 && g->lon > -73.0,
                 "6-character grid parses to its square");

    r.rememberGrid("N0GRID", "XX", S::Grid, 0);
    r.rememberGrid("N0GRID2", "", S::Grid, 0);
    r.rememberLatLon("N0ZERO", 0.0, 0.0, S::Park, 0);
    r.rememberLatLon("N0BAD", 95.0, 10.0, S::Park, 0);
    ok &= expect(!r.lookup("N0GRID", 1) && !r.lookup("N0GRID2", 1) && !r.lookup("N0ZERO", 1)
                     && !r.lookup("N0BAD", 1),
                 "bad grids, 0/0 and out-of-range positions are ignored");

    r.clear();
    ok &= expect(!r.lookup("W1AW", 1), "clear");

    std::cout << (ok ? "All checks passed\n" : "Some checks FAILED\n");
    return ok ? 0 : 1;
}
