#pragma once

#include <QVector>

namespace AetherSDR {

// Auto ESC (#6589): the search behind the "A" button on the VFO flag's ESC
// panel. Pure logic with no timers or widgets, so it can be tested without a
// radio. VfoWidget applies each point to the slice, waits for the ESC meter to
// settle, averages it, and reports the level back here.
//
// Search order, each step on the slider's own grid (5 degrees, 0.01 gain):
//   1. Baseline: the starting phase and gain, so the result can be compared.
//   2. Coarse phase: 0..345 degrees every 15, gain held at the start value.
//   3. Fine phase: +/-15 degrees around the best coarse phase, every 5.
//   4. (optional) Coarse gain: 0.00..2.00 every 0.25 at the best phase.
//   5. (optional) Fine gain: +/-0.20 around the best coarse gain, every 0.05.
// "Best" is the highest level for Peak, the lowest for Null. A point must beat
// the current best outright, so a tie keeps the earlier point (the baseline
// first), and the controls are not moved for no gain.
class EscAutoSweep {
public:
    enum class Target { Peak, Null };

    struct Point {
        int phaseDeg{0};   // 0..355, multiple of 5
        int gainPct{100};  // 0..200 (gain x 100)
        bool operator==(const Point& o) const
        {
            return phaseDeg == o.phaseDeg && gainPct == o.gainPct;
        }
    };

    static constexpr int kPhaseStepDeg = 5;
    static constexpr int kCoarsePhaseStepDeg = 15;
    static constexpr int kFinePhaseSpanDeg = 15;
    static constexpr int kGainMaxPct = 200;
    static constexpr int kCoarseGainStepPct = 25;
    static constexpr int kFineGainSpanPct = 20;
    static constexpr int kFineGainStepPct = 5;

    // Snap to the slider grid: phase to 0..355 in 5-degree steps, gain to
    // 0..200.
    static Point normalized(Point p);

    void start(Point startPoint, Target target, bool tuneGain);
    void cancel();

    bool active() const { return m_active; }
    Target target() const { return m_target; }
    Point startPoint() const { return m_start; }
    // The point to apply and measure now. Only meaningful while active().
    Point current() const;

    // Report the averaged level for current(). `valid` is false when no
    // meter reading arrived for it; such a point is skipped. Returns true
    // while there is another point to measure, false when the search is done.
    bool report(double levelDbm, bool valid);

    // Progress for the UI: points measured so far, and the total expected.
    int measuredCount() const { return m_measured; }
    int expectedTotal() const;

    // Results, meaningful once the search has finished (active() false).
    bool hasResult() const { return m_hasBest; }
    bool hasBaseline() const { return m_hasBaseline; }
    Point best() const { return m_best; }
    double bestLevel() const { return m_bestLevel; }
    double baselineLevel() const { return m_baselineLevel; }
    // Improvement over the baseline in dB, positive = better for the target
    // (louder for Peak, deeper for Null). 0 without a baseline.
    double improvementDb() const;

private:
    enum class Stage { Baseline, CoarsePhase, FinePhase, CoarseGain, FineGain, Done };

    void enterStage(Stage stage);
    bool better(double level) const;

    bool m_active{false};
    bool m_tuneGain{true};
    Target m_target{Target::Peak};
    Stage m_stage{Stage::Done};
    Point m_start;
    QVector<Point> m_queue;
    int m_index{0};
    int m_measured{0};

    bool m_hasBest{false};
    Point m_best;
    double m_bestLevel{0.0};
    bool m_hasBaseline{false};
    double m_baselineLevel{0.0};
};

} // namespace AetherSDR
