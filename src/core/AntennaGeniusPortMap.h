#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>

#include <functional>

namespace AetherSDR {

class RadioSettingsScope;

// Which radio antenna connector each Antenna Genius radio port is wired to
// (#4892). The Antenna Genius cannot report this itself, so the operator
// sets it in Radio Setup -> Antennas. An empty token means "not used": that
// port's antenna names are not shown on the radio's antenna buttons.
//
// Stored as one radio-scoped feature document (Principle V):
//   "AntennaGeniusPortMap" v1: {"portA": "ANT1", "portB": "ANT2"}
// A key is written only when set, so the absent document (the default)
// leaves the feature off.
struct AntennaGeniusPortMap {
    QString portA;
    QString portB;

    bool isEmpty() const { return portA.isEmpty() && portB.isEmpty(); }
    bool operator==(const AntennaGeniusPortMap& o) const
    {
        return portA == o.portA && portB == o.portB;
    }
    bool operator!=(const AntennaGeniusPortMap& o) const { return !(*this == o); }

    QJsonObject toJson() const;
    static AntennaGeniusPortMap fromJson(const QJsonObject& obj);
};

inline constexpr int kAntennaGeniusPortMapSchemaVersion = 1;

AntennaGeniusPortMap loadAntennaGeniusPortMap(const RadioSettingsScope& scope);
// Returns false (and logs) when the store refused the write.
bool saveAntennaGeniusPortMap(const RadioSettingsScope& scope,
                              const AntennaGeniusPortMap& map);

// Radio antenna token -> name of the antenna the mapped Antenna Genius port
// has selected (#4892). `portAAntenna` / `portBAntenna` are the AG antenna
// ids each port has selected (0 = none); `antennaName` looks an id up. A
// port that is not mapped, has nothing selected, or whose antenna has no
// name contributes nothing. Port A wins if both ports map to one token.
QMap<QString, QString> antennaGeniusLiveNames(
    const AntennaGeniusPortMap& map, int portAAntenna, int portBAntenna,
    const std::function<QString(int)>& antennaName);

// Emits changed() after every successful save, so a reader that caches
// the map knows to reload it.
class AntennaGeniusPortMapNotifier : public QObject {
    Q_OBJECT
public:
    static AntennaGeniusPortMapNotifier& instance();
signals:
    void changed();
private:
    AntennaGeniusPortMapNotifier() = default;
};

} // namespace AetherSDR
