// Auto ESC heading memory: payload parsing, the "beam has stopped" detector,
// per-band / per-10-degree storage and its JSON round trip. Pure logic.
#include "gui/EscHeadingMemory.h"

#include <QJsonDocument>
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

    // ── Payloads ─────────────────────────────────────────────────────────
    {
        const auto plain = parseRotorHeadingPayload(" 135 \n");
        ok &= expect(plain && plain->headingDeg == 135 && plain->band.isEmpty(),
                     "a bare number is a heading for any band");
        const auto json = parseRotorHeadingPayload(
            R"({"heading":44.6,"band":"2m","rotor":"south"})");
        ok &= expect(json && json->headingDeg == 45 && json->band == "2m"
                         && json->rotor == "south",
                     "JSON carries heading (rounded), band and rotor");
        const auto str = parseRotorHeadingPayload(R"({"azimuth":"360"})");
        ok &= expect(str && str->headingDeg == 0, "azimuth as a string, 360 reads 0");
        ok &= expect(!parseRotorHeadingPayload("north"), "text that is not a number is ignored");
        ok &= expect(!parseRotorHeadingPayload("400"), "out-of-range heading is ignored");
        ok &= expect(!parseRotorHeadingPayload(R"({"band":"2m"})"), "JSON with no heading is ignored");
        ok &= expect(!parseRotorHeadingPayload(""), "empty payload is ignored");
    }

    ok &= expect(headingDistanceDeg(355, 5) == 10 && headingDistanceDeg(90, 270) == 180,
                 "heading distance wraps through north");

    // ── Settle detector ──────────────────────────────────────────────────
    {
        HeadingSettleDetector d;
        d.update(30, 0);
        ok &= expect(!d.poll(5000), "not settled before 7 s");
        d.update(31, 5000);  // within tolerance: same stop
        ok &= expect(d.poll(7000) == 30, "settled after 7 s steady");
        ok &= expect(!d.poll(9000), "a settled heading is reported once");
        ok &= expect(d.stableHeading(9000) == 30, "stable while at rest");

        d.update(60, 10000);  // turning
        ok &= expect(!d.stableHeading(11000), "not stable while turning");
        d.update(90, 15000);
        ok &= expect(!d.poll(20000), "still turning 5 s after the last change");
        ok &= expect(d.poll(22000) == 90, "new stop reported after 7 s at 90");

        // Poll interval of the controller (5 s) repeating the same value while
        // the beam is actually moving must not count as a stop.
        HeadingSettleDetector slow;
        slow.update(100, 0);
        slow.update(100, 2000);
        ok &= expect(!slow.poll(4000), "two equal readings 2 s apart are not a stop");
    }

    // ── Memory ───────────────────────────────────────────────────────────
    {
        ok &= expect(EscHeadingMemory::bandKey(144.200) == "2m"
                         && EscHeadingMemory::bandKey(50.125) == "6m"
                         && EscHeadingMemory::bandKey(14.250) == "20m"
                         && EscHeadingMemory::bandKey(432.1) == "70cm"
                         && EscHeadingMemory::bandKey(137.0) == "137MHz",
                     "band keys");
        ok &= expect(EscHeadingMemory::bucketFor(356) == 0 && EscHeadingMemory::bucketFor(4) == 0
                         && EscHeadingMemory::bucketFor(5) == 1 && EscHeadingMemory::bucketFor(134) == 13
                         && EscHeadingMemory::bucketFor(135) == 14,
                     "10-degree buckets centred on multiples of 10");

        EscHeadingMemory m;
        m.store("2m", 132, {40, 110});
        m.store("6m", 132, {200, 90});
        const auto near = m.recall("2m", 128);
        ok &= expect(near && near->phaseDeg == 40 && near->gainPct == 110,
                     "recall from the same 10-degree bucket");
        ok &= expect(!m.recall("2m", 150), "no recall from another bucket");
        ok &= expect(m.recall("6m", 130)->phaseDeg == 200, "bands are kept apart");
        m.store("2m", 130, {43, 999});
        ok &= expect(m.recall("2m", 130)->phaseDeg == 45 && m.recall("2m", 130)->gainPct == 200,
                     "a new result replaces the old one, snapped to the slider grid");
        ok &= expect(m.countForBand("2m") == 1 && m.size() == 2, "counts");

        const QByteArray json = QJsonDocument(m.toJson()).toJson(QJsonDocument::Compact);
        const EscHeadingMemory back =
            EscHeadingMemory::fromJson(QJsonDocument::fromJson(json).object());
        ok &= expect(back.size() == 2 && back.recall("6m", 130)->gainPct == 90,
                     "JSON round trip");
        ok &= expect(EscHeadingMemory::fromJson(
                         QJsonDocument::fromJson(R"({"version":2,"points":{}})").object())
                             .size() == 0,
                     "unknown version loads nothing");

        m.clearBand("2m");
        ok &= expect(!m.recall("2m", 130) && m.recall("6m", 130), "forget one band only");
        m.store("", 10, {0, 100});
        ok &= expect(m.size() == 1, "no band, nothing stored");
    }

    std::cout << (ok ? "All checks passed\n" : "Some checks FAILED\n");
    return ok ? 0 : 1;
}
