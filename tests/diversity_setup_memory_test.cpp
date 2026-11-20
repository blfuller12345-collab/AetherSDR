// Diversity setup memory: how each diversity slice was last left on each
// band (ESC, phase, gain, mute, volume, Auto AGC-T, antenna) and its JSON
// round trip, which is what survives a restart. Pure logic.
#include "gui/DiversitySetupMemory.h"

#include <QJsonDocument>
#include <iostream>

using namespace AetherSDR;

namespace {

bool expect(bool condition, const char* label)
{
    std::cout << (condition ? "[ OK ] " : "[FAIL] ") << label << '\n';
    return condition;
}

using Role = DiversitySetupMemory::Role;

} // namespace

int main()
{
    bool ok = true;

    DiversitySetupMemory m;
    ok &= expect(!m.recall("2m", Role::Parent), "nothing saved, nothing to restore");

    // Brett's 2m pair: A on 18XXX with ESC tuned, D on Loops unmuted.
    const DiversitySliceSetup parent{true, 160, 195, false, 55, true, "ANT1"};
    const DiversitySliceSetup child{false, 0, 100, false, 40, true, "ANT2"};
    m.store("2m", Role::Parent, parent);
    m.store("2m", Role::Child, child);
    m.store("6m", Role::Parent, {false, 90, 100, true, 30, false, "ANT1"});

    ok &= expect(m.recall("2m", Role::Parent) == parent, "parent setup kept");
    ok &= expect(m.recall("2m", Role::Child) == child, "child setup kept apart from parent");
    ok &= expect(m.recall("6m", Role::Parent)->muted && !m.recall("6m", Role::Child),
                 "bands kept apart");

    m.store("2m", Role::Parent, {true, 162, 250, false, 120, true, "ANT1"});
    const auto snapped = m.recall("2m", Role::Parent);
    ok &= expect(snapped->escPhaseDeg == 160 && snapped->escGainPct == 200
                     && snapped->volume == 100,
                 "newest replaces, snapped to the controls' ranges");
    m.store("2m", Role::Parent, {true, 358, 100, false, 50, true, ""});
    ok &= expect(m.recall("2m", Role::Parent)->escPhaseDeg == 0, "phase wraps at 360");
    m.store("", Role::Parent, parent);
    ok &= expect(m.size() == 3, "no band, nothing stored");

    m.store("2m", Role::Parent, parent);
    const QByteArray json = QJsonDocument(m.toJson()).toJson(QJsonDocument::Compact);
    const DiversitySetupMemory back =
        DiversitySetupMemory::fromJson(QJsonDocument::fromJson(json).object());
    ok &= expect(back.size() == 3 && back.recall("2m", Role::Parent) == parent
                     && back.recall("2m", Role::Child) == child,
                 "everything survives a restart (JSON round trip)");

    ok &= expect(DiversitySetupMemory::fromJson(
                     QJsonDocument::fromJson(R"({"version":2,"setups":{}})").object())
                         .size() == 0,
                 "unknown version loads nothing");
    const auto junk = DiversitySetupMemory::fromJson(QJsonDocument::fromJson(
        R"({"version":1,"setups":{"2m|sideways":{"mute":true,"volume":5},
            "|parent":{"mute":true,"volume":5},"2m|child":{"esc":true}}})").object());
    ok &= expect(junk.size() == 0, "bad entries are skipped");

    std::cout << (ok ? "All checks passed\n" : "Some checks FAILED\n");
    return ok ? 0 : 1;
}
