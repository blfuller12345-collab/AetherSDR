// Antenna Genius port -> radio antenna name mapping (#4892).
#include "core/AntennaGeniusPortMap.h"

#include <QMap>
#include <iostream>

using namespace AetherSDR;

namespace {

bool expect(bool condition, const char* label)
{
    std::cout << (condition ? "[ OK ] " : "[FAIL] ") << label << '\n';
    return condition;
}

QString agName(int id)
{
    static const QMap<int, QString> names{
        {1, QStringLiteral("Doublet")},
        {2, QStringLiteral("Hex Beam")},
        {3, QStringLiteral("6m Yagi")},
        {4, QStringLiteral("  ")},
    };
    return names.value(id);
}

} // namespace

int main()
{
    bool ok = true;

    {
        const AntennaGeniusPortMap map{QStringLiteral("ANT1"), QStringLiteral("ANT2")};
        const QMap<QString, QString> names = antennaGeniusLiveNames(map, 1, 2, agName);
        ok &= expect(names.size() == 2, "both mapped ports produce a name");
        ok &= expect(names.value(QStringLiteral("ANT1")) == QStringLiteral("Doublet"),
                     "port A's selected antenna names ANT1");
        ok &= expect(names.value(QStringLiteral("ANT2")) == QStringLiteral("Hex Beam"),
                     "port B's selected antenna names ANT2");

        const QMap<QString, QString> after = antennaGeniusLiveNames(map, 1, 3, agName);
        ok &= expect(after.value(QStringLiteral("ANT2")) == QStringLiteral("6m Yagi"),
                     "the name follows the antenna the port switches to");
    }

    {
        ok &= expect(antennaGeniusLiveNames({}, 1, 2, agName).isEmpty(),
                     "no mapping means no names (feature off by default)");

        const AntennaGeniusPortMap onlyB{QString(), QStringLiteral("ANT2")};
        const QMap<QString, QString> names = antennaGeniusLiveNames(onlyB, 1, 2, agName);
        ok &= expect(names.size() == 1 && names.contains(QStringLiteral("ANT2")),
                     "an unmapped port contributes nothing");

        const AntennaGeniusPortMap map{QStringLiteral("ANT1"), QStringLiteral("ANT2")};
        ok &= expect(antennaGeniusLiveNames(map, 0, 0, agName).isEmpty(),
                     "a port with no antenna selected contributes nothing");
        ok &= expect(antennaGeniusLiveNames(map, 4, 9, agName).isEmpty(),
                     "blank or unknown AG antenna names contribute nothing");

        const AntennaGeniusPortMap same{QStringLiteral("ANT1"), QStringLiteral("ANT1")};
        ok &= expect(antennaGeniusLiveNames(same, 1, 2, agName).value(QStringLiteral("ANT1"))
                         == QStringLiteral("Doublet"),
                     "port A wins when both ports map to one connector");
        ok &= expect(antennaGeniusLiveNames(map, 1, 2, {}).isEmpty(),
                     "no name lookup means no names");
    }

    {
        const AntennaGeniusPortMap map{QStringLiteral(" ANT1 "), QString()};
        const QJsonObject json = map.toJson();
        ok &= expect(json.size() == 1 && json.value(QStringLiteral("portA")).toString()
                                             == QStringLiteral("ANT1"),
                     "JSON writes only set ports, trimmed");
        const AntennaGeniusPortMap back = AntennaGeniusPortMap::fromJson(json);
        ok &= expect(back.portA == QStringLiteral("ANT1") && back.portB.isEmpty(),
                     "JSON round-trips the port map");
        ok &= expect(AntennaGeniusPortMap::fromJson({}).isEmpty(),
                     "a missing document reads as an empty (off) map");
    }

    return ok ? 0 : 1;
}
