#include "EscAutoSweep.h"

#include <algorithm>

namespace AetherSDR {

namespace {

int wrapPhase(int deg)
{
    deg %= 360;
    if (deg < 0)
        deg += 360;
    return deg;
}

} // namespace

EscAutoSweep::Point EscAutoSweep::normalized(Point p)
{
    const int snapped = static_cast<int>(
        ((wrapPhase(p.phaseDeg) + kPhaseStepDeg / 2) / kPhaseStepDeg) * kPhaseStepDeg);
    p.phaseDeg = wrapPhase(snapped);
    p.gainPct = std::clamp(p.gainPct, 0, kGainMaxPct);
    return p;
}

void EscAutoSweep::start(Point startPoint, Target target, bool tuneGain)
{
    m_start = normalized(startPoint);
    m_target = target;
    m_tuneGain = tuneGain;
    m_active = true;
    m_measured = 0;
    m_hasBest = false;
    m_hasBaseline = false;
    m_best = m_start;
    m_bestLevel = 0.0;
    m_baselineLevel = 0.0;
    enterStage(Stage::Baseline);
}

void EscAutoSweep::cancel()
{
    m_active = false;
    m_stage = Stage::Done;
    m_queue.clear();
    m_index = 0;
}

EscAutoSweep::Point EscAutoSweep::current() const
{
    if (!m_active || m_index < 0 || m_index >= m_queue.size())
        return m_best;
    return m_queue.at(m_index);
}

int EscAutoSweep::expectedTotal() const
{
    const int coarsePhase = 360 / kCoarsePhaseStepDeg;
    const int finePhase = (2 * kFinePhaseSpanDeg) / kPhaseStepDeg + 1;
    int total = 1 + coarsePhase + finePhase;
    if (m_tuneGain) {
        const int coarseGain = kGainMaxPct / kCoarseGainStepPct + 1;
        const int fineGain = (2 * kFineGainSpanPct) / kFineGainStepPct + 1;
        total += coarseGain + fineGain;
    }
    return total;
}

bool EscAutoSweep::better(double level) const
{
    if (!m_hasBest)
        return true;
    return m_target == Target::Peak ? level > m_bestLevel : level < m_bestLevel;
}

bool EscAutoSweep::report(double levelDbm, bool valid)
{
    if (!m_active)
        return false;

    const Point measured = current();
    ++m_measured;
    if (valid) {
        if (m_stage == Stage::Baseline) {
            m_hasBaseline = true;
            m_baselineLevel = levelDbm;
        }
        if (better(levelDbm)) {
            m_hasBest = true;
            m_best = measured;
            m_bestLevel = levelDbm;
        }
    }

    ++m_index;
    while (m_active && m_index >= m_queue.size()) {
        switch (m_stage) {
        case Stage::Baseline:   enterStage(Stage::CoarsePhase); break;
        case Stage::CoarsePhase: enterStage(Stage::FinePhase); break;
        case Stage::FinePhase:
            enterStage(m_tuneGain ? Stage::CoarseGain : Stage::Done);
            break;
        case Stage::CoarseGain: enterStage(Stage::FineGain); break;
        case Stage::FineGain:   enterStage(Stage::Done); break;
        case Stage::Done:       break;
        }
    }
    return m_active;
}

void EscAutoSweep::enterStage(Stage stage)
{
    m_stage = stage;
    m_queue.clear();
    m_index = 0;

    // Searches after the baseline centre on the best point so far; if no
    // reading has arrived yet, on the starting point.
    const Point centre = m_hasBest ? m_best : m_start;

    switch (stage) {
    case Stage::Baseline:
        m_queue.append(m_start);
        break;
    case Stage::CoarsePhase:
        for (int deg = 0; deg < 360; deg += kCoarsePhaseStepDeg)
            m_queue.append(normalized({deg, m_start.gainPct}));
        break;
    case Stage::FinePhase:
        for (int d = -kFinePhaseSpanDeg; d <= kFinePhaseSpanDeg; d += kPhaseStepDeg)
            m_queue.append(normalized({centre.phaseDeg + d, centre.gainPct}));
        break;
    case Stage::CoarseGain:
        for (int g = 0; g <= kGainMaxPct; g += kCoarseGainStepPct)
            m_queue.append(normalized({centre.phaseDeg, g}));
        break;
    case Stage::FineGain:
        for (int d = -kFineGainSpanPct; d <= kFineGainSpanPct; d += kFineGainStepPct) {
            const int g = centre.gainPct + d;
            if (g < 0 || g > kGainMaxPct)
                continue;   // stay on the slider; edge points are not repeated
            m_queue.append(normalized({centre.phaseDeg, g}));
        }
        break;
    case Stage::Done:
        m_active = false;
        break;
    }
}

double EscAutoSweep::improvementDb() const
{
    if (!m_hasBaseline || !m_hasBest)
        return 0.0;
    return m_target == Target::Peak ? m_bestLevel - m_baselineLevel
                                    : m_baselineLevel - m_bestLevel;
}

} // namespace AetherSDR
