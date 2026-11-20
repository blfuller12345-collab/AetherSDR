#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace AetherSDR {

// How the operator last left each diversity slice on each band, so a restart
// or a trip to another band and back doesn't mean setting it all up again.
// The parent slice keeps ESC on/off, phase and gain; both slices keep mute,
// volume, Auto AGC-T and the receive antenna. Pure logic, no widgets or
// settings file, so it can be tested without a radio.
struct DiversitySliceSetup {
    bool escOn{false};
    int escPhaseDeg{0};    // 0..355, multiple of 5 (the phase slider's steps)
    int escGainPct{100};   // 0..200 (gain x 100)
    bool muted{false};
    int volume{50};        // 0..100
    bool autoAgcT{false};
    QString rxAntenna;     // empty = leave the antenna alone

    bool operator==(const DiversitySliceSetup& o) const = default;
    // Snaps values onto the grid the controls use.
    DiversitySliceSetup normalized() const;
};

class DiversitySetupMemory {
public:
    enum class Role { Parent, Child };

    void store(const QString& band, Role role, const DiversitySliceSetup& setup);
    std::optional<DiversitySliceSetup> recall(const QString& band, Role role) const;
    int size() const { return m_setups.size(); }

    // {"version":1,"setups":{"2m|parent":{"esc":true,"phase":160,"gain":195,
    //   "mute":false,"volume":50,"agct":true,"ant":"ANT1"},...}}
    QJsonObject toJson() const;
    static DiversitySetupMemory fromJson(const QJsonObject& obj);

private:
    static QString key(const QString& band, Role role);
    QHash<QString, DiversitySliceSetup> m_setups;
};

} // namespace AetherSDR
