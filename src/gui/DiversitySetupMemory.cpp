#include "DiversitySetupMemory.h"

#include <algorithm>

namespace AetherSDR {

DiversitySliceSetup DiversitySliceSetup::normalized() const
{
    DiversitySliceSetup s = *this;
    int phase = ((s.escPhaseDeg % 360) + 360) % 360;
    phase = ((phase + 2) / 5) * 5 % 360;
    s.escPhaseDeg = phase;
    s.escGainPct = std::clamp(s.escGainPct, 0, 200);
    s.volume = std::clamp(s.volume, 0, 100);
    return s;
}

QString DiversitySetupMemory::key(const QString& band, Role role)
{
    return band + (role == Role::Parent ? QStringLiteral("|parent") : QStringLiteral("|child"));
}

void DiversitySetupMemory::store(const QString& band, Role role,
                                 const DiversitySliceSetup& setup)
{
    if (band.isEmpty())
        return;
    m_setups.insert(key(band, role), setup.normalized());
}

std::optional<DiversitySliceSetup> DiversitySetupMemory::recall(const QString& band,
                                                                Role role) const
{
    const auto it = m_setups.constFind(key(band, role));
    if (it == m_setups.constEnd())
        return std::nullopt;
    return it.value();
}

QJsonObject DiversitySetupMemory::toJson() const
{
    QJsonObject setups;
    for (auto it = m_setups.constBegin(); it != m_setups.constEnd(); ++it) {
        const DiversitySliceSetup& s = it.value();
        QJsonObject o;
        o.insert(QStringLiteral("esc"), s.escOn);
        o.insert(QStringLiteral("phase"), s.escPhaseDeg);
        o.insert(QStringLiteral("gain"), s.escGainPct);
        o.insert(QStringLiteral("mute"), s.muted);
        o.insert(QStringLiteral("volume"), s.volume);
        o.insert(QStringLiteral("agct"), s.autoAgcT);
        o.insert(QStringLiteral("ant"), s.rxAntenna);
        setups.insert(it.key(), o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("setups"), setups);
    return root;
}

DiversitySetupMemory DiversitySetupMemory::fromJson(const QJsonObject& obj)
{
    DiversitySetupMemory mem;
    if (obj.value(QStringLiteral("version")).toInt() != 1)
        return mem;
    const QJsonObject setups = obj.value(QStringLiteral("setups")).toObject();
    for (auto it = setups.constBegin(); it != setups.constEnd(); ++it) {
        const QString k = it.key();
        const int bar = k.lastIndexOf(QLatin1Char('|'));
        if (bar <= 0)
            continue;
        const QString roleText = k.mid(bar + 1);
        Role role;
        if (roleText == QLatin1String("parent"))
            role = Role::Parent;
        else if (roleText == QLatin1String("child"))
            role = Role::Child;
        else
            continue;
        const QJsonObject o = it.value().toObject();
        if (!o.contains(QStringLiteral("mute")) || !o.contains(QStringLiteral("volume")))
            continue;
        DiversitySliceSetup s;
        s.escOn = o.value(QStringLiteral("esc")).toBool();
        s.escPhaseDeg = o.value(QStringLiteral("phase")).toInt(0);
        s.escGainPct = o.value(QStringLiteral("gain")).toInt(100);
        s.muted = o.value(QStringLiteral("mute")).toBool();
        s.volume = o.value(QStringLiteral("volume")).toInt(50);
        s.autoAgcT = o.value(QStringLiteral("agct")).toBool();
        s.rxAntenna = o.value(QStringLiteral("ant")).toString();
        mem.store(k.left(bar), role, s);
    }
    return mem;
}

} // namespace AetherSDR
