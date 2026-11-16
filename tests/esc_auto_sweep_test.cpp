// Auto ESC search (#6589): pure logic, driven with a synthetic meter.
#include "gui/EscAutoSweep.h"

#include <cmath>
#include <functional>
#include <iostream>

using namespace AetherSDR;

namespace {

bool expect(bool condition, const char* label)
{
    std::cout << (condition ? "[ OK ] " : "[FAIL] ") << label << '\n';
    return condition;
}

// A combined level that peaks at (peakDeg, peakGain): a cosine in phase plus a
// gentle gain term, so the best point is unique and on the slider grid.
double synthetic(const EscAutoSweep::Point& p, int peakDeg, int peakGainPct)
{
    const double rad = (p.phaseDeg - peakDeg) * M_PI / 180.0;
    const double gainErr = (p.gainPct - peakGainPct) / 100.0;
    return -90.0 + 20.0 * std::cos(rad) - 6.0 * gainErr * gainErr;
}

int run(EscAutoSweep& sweep, const std::function<double(const EscAutoSweep::Point&)>& meter,
        const std::function<bool(int)>& validAt = {})
{
    int steps = 0;
    while (sweep.active() && steps < 1000) {
        const bool valid = validAt ? validAt(steps) : true;
        sweep.report(meter(sweep.current()), valid);
        ++steps;
    }
    return steps;
}

} // namespace

int main()
{
    bool ok = true;

    {
        const auto p = EscAutoSweep::normalized({-7, 250});
        ok &= expect(p.phaseDeg == 355 && p.gainPct == 200,
                     "normalized wraps phase onto the 5-degree grid and clamps gain");
        const auto q = EscAutoSweep::normalized({362, -4});
        ok &= expect(q.phaseDeg == 0 && q.gainPct == 0,
                     "normalized wraps past 360 and clamps gain at 0");
    }

    {
        EscAutoSweep sweep;
        sweep.start({0, 100}, EscAutoSweep::Target::Peak, true);
        const int steps = run(sweep, [](const EscAutoSweep::Point& p) {
            return synthetic(p, 135, 85);
        });
        ok &= expect(!sweep.active() && sweep.hasResult(), "peak search finishes with a result");
        ok &= expect(sweep.best().phaseDeg == 135, "peak search finds the best phase");
        ok &= expect(sweep.best().gainPct == 85, "peak search finds the best gain");
        ok &= expect(steps == sweep.expectedTotal(), "peak search measures the expected number of points");
        ok &= expect(sweep.improvementDb() > 30.0, "improvement is reported against the starting point");
    }

    {
        EscAutoSweep sweep;
        sweep.start({90, 100}, EscAutoSweep::Target::Null, false);
        run(sweep, [](const EscAutoSweep::Point& p) { return synthetic(p, 20, 100); });
        ok &= expect(sweep.best().phaseDeg == 200, "null search finds the phase opposite the peak");
        ok &= expect(sweep.best().gainPct == 100, "phase-only search leaves gain alone");
        ok &= expect(sweep.improvementDb() > 0.0, "a deeper null counts as an improvement");
    }

    {
        // Wrap-around: best coarse phase near 0 refines across 360.
        EscAutoSweep sweep;
        sweep.start({180, 100}, EscAutoSweep::Target::Peak, false);
        run(sweep, [](const EscAutoSweep::Point& p) { return synthetic(p, 355, 100); });
        ok &= expect(sweep.best().phaseDeg == 355, "fine phase search wraps across 0/360");
    }

    {
        // A flat meter: nothing beats the baseline, so the controls stay put.
        EscAutoSweep sweep;
        sweep.start({45, 120}, EscAutoSweep::Target::Peak, true);
        run(sweep, [](const EscAutoSweep::Point&) { return -100.0; });
        ok &= expect(sweep.best() == EscAutoSweep::Point{45, 120},
                     "a tie keeps the starting point");
        ok &= expect(sweep.improvementDb() == 0.0, "no improvement is reported on a flat meter");
    }

    {
        // No meter readings at all: no result, the caller restores the start.
        EscAutoSweep sweep;
        sweep.start({45, 120}, EscAutoSweep::Target::Peak, true);
        run(sweep, [](const EscAutoSweep::Point&) { return 0.0; },
            [](int) { return false; });
        ok &= expect(!sweep.active() && !sweep.hasResult(),
                     "a search with no readings ends without a result");
    }

    {
        // Missing readings are skipped, not counted as quiet points.
        EscAutoSweep sweep;
        sweep.start({0, 100}, EscAutoSweep::Target::Null, false);
        run(sweep, [](const EscAutoSweep::Point& p) { return synthetic(p, 0, 100); },
            [](int step) { return step % 3 != 1; });
        ok &= expect(sweep.hasResult() && std::abs(sweep.best().phaseDeg - 180) <= 15,
                     "skipped points do not break the search");
    }

    {
        EscAutoSweep sweep;
        sweep.start({0, 100}, EscAutoSweep::Target::Peak, true);
        sweep.report(-90.0, true);
        sweep.cancel();
        ok &= expect(!sweep.active(), "cancel stops the search");
        ok &= expect(!sweep.report(-80.0, true), "reports after cancel are ignored");
    }

    {
        // Fine gain near the top of the range stays on the slider.
        EscAutoSweep sweep;
        sweep.start({0, 100}, EscAutoSweep::Target::Peak, true);
        bool inRange = true;
        while (sweep.active()) {
            const auto p = sweep.current();
            inRange &= p.gainPct >= 0 && p.gainPct <= 200 && p.phaseDeg >= 0 && p.phaseDeg < 360;
            sweep.report(synthetic(p, 0, 200), true);
        }
        ok &= expect(inRange, "every point stays within the slider ranges");
        ok &= expect(sweep.best().gainPct == 200, "gain search reaches the top of the range");
    }

    return ok ? 0 : 1;
}
