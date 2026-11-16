#include "core/AntennaGeniusPortMap.h"

#include "core/LogManager.h"
#include "core/RadioSettingsScope.h"

namespace AetherSDR {

namespace {

constexpr const char* kFeatureName = "AntennaGeniusPortMap";

} // namespace

QJsonObject AntennaGeniusPortMap::toJson() const
{
    QJsonObject obj;
    const QString a = portA.trimmed();
    const QString b = portB.trimmed();
    if (!a.isEmpty())
        obj.insert(QStringLiteral("portA"), a);
    if (!b.isEmpty())
        obj.insert(QStringLiteral("portB"), b);
    return obj;
}

AntennaGeniusPortMap AntennaGeniusPortMap::fromJson(const QJsonObject& obj)
{
    AntennaGeniusPortMap map;
    map.portA = obj.value(QStringLiteral("portA")).toString().trimmed();
    map.portB = obj.value(QStringLiteral("portB")).toString().trimmed();
    return map;
}

QMap<QString, QString> antennaGeniusLiveNames(
    const AntennaGeniusPortMap& map, int portAAntenna, int portBAntenna,
    const std::function<QString(int)>& antennaName)
{
    QMap<QString, QString> names;
    auto add = [&](const QString& rawToken, int antennaId) {
        const QString token = rawToken.trimmed();
        if (token.isEmpty() || names.contains(token) || antennaId <= 0
            || !antennaName)
            return;
        const QString name = antennaName(antennaId).trimmed();
        if (!name.isEmpty())
            names.insert(token, name);
    };
    add(map.portA, portAAntenna);
    add(map.portB, portBAntenna);
    return names;
}

AntennaGeniusPortMap loadAntennaGeniusPortMap(const RadioSettingsScope& scope)
{
    if (!scope.isValid() || !scope.hasRadioIdentity())
        return {};
    return AntennaGeniusPortMap::fromJson(
        scope.feature(QString::fromLatin1(kFeatureName)));
}

bool saveAntennaGeniusPortMap(const RadioSettingsScope& scope,
                              const AntennaGeniusPortMap& map)
{
    // Never write the family-wide default row by accident: the wiring
    // belongs to one physical radio.
    if (!scope.isValid() || !scope.hasRadioIdentity()) {
        qCWarning(lcTuner) << "AntennaGeniusPortMap: no radio identity; not saved";
        return false;
    }
    if (!scope.setFeature(QString::fromLatin1(kFeatureName),
                          kAntennaGeniusPortMapSchemaVersion, map.toJson())) {
        qCWarning(lcTuner) << "AntennaGeniusPortMap: settings write did not persist";
        return false;
    }
    emit AntennaGeniusPortMapNotifier::instance().changed();
    return true;
}

AntennaGeniusPortMapNotifier& AntennaGeniusPortMapNotifier::instance()
{
    static AntennaGeniusPortMapNotifier notifier;
    return notifier;
}

} // namespace AetherSDR
