#include "AntennaGeniusNameBridge.h"

#include "models/RadioModel.h"

namespace AetherSDR {

namespace {

// The antenna the port is receiving on, else the one it transmits on.
int selectedAntenna(const AgPortStatus& port)
{
    return port.rxAntenna > 0 ? port.rxAntenna : port.txAntenna;
}

} // namespace

AntennaGeniusNameBridge::AntennaGeniusNameBridge(RadioModel* radio,
                                                 AntennaGeniusModel* ag,
                                                 QObject* parent)
    : QObject(parent)
    , m_radio(radio)
    , m_ag(ag)
{
    if (!m_radio || !m_ag)
        return;

    connect(m_ag, &AntennaGeniusModel::connected, this, &AntennaGeniusNameBridge::refresh);
    connect(m_ag, &AntennaGeniusModel::disconnected, this, &AntennaGeniusNameBridge::clearAll);
    connect(m_ag, &AntennaGeniusModel::antennasChanged, this, &AntennaGeniusNameBridge::refresh);
    connect(m_ag, &AntennaGeniusModel::portStatusChanged, this,
            [this](int) { refresh(); });

    // The wiring is stored per radio: reload it when the radio changes.
    connect(m_radio, &RadioModel::connectionStateChanged, this, [this](bool) {
        reloadMap();
        refresh();
    });
    connect(m_radio, &RadioModel::infoChanged, this, [this] {
        reloadMap();
        refresh();
    });
    connect(&AntennaGeniusPortMapNotifier::instance(),
            &AntennaGeniusPortMapNotifier::changed, this, [this] {
        reloadMap();
        refresh();
    });

    reloadMap();
    refresh();
}

void AntennaGeniusNameBridge::reloadMap()
{
    m_map = m_radio ? loadAntennaGeniusPortMap(m_radio->settingsScope())
                    : AntennaGeniusPortMap{};
}

void AntennaGeniusNameBridge::refresh()
{
    if (!m_radio || !m_ag)
        return;
    if (!m_ag->isConnected() || !m_radio->isConnected() || m_map.isEmpty()) {
        clearAll();
        return;
    }

    const QMap<QString, QString> names = antennaGeniusLiveNames(
        m_map, selectedAntenna(m_ag->portA()), selectedAntenna(m_ag->portB()),
        [this](int id) { return m_ag->antennaName(id); });

    // Clear tokens named before that no longer have a name (port unmapped,
    // or no antenna selected); setAntennaLiveName ignores unchanged names.
    for (const QString& token : QSet<QString>(m_tokensSet)) {
        if (!names.contains(token)) {
            m_radio->setAntennaLiveName(token, QString());
            m_tokensSet.remove(token);
        }
    }
    for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
        m_radio->setAntennaLiveName(it.key(), it.value());
        m_tokensSet.insert(it.key());
    }
}

void AntennaGeniusNameBridge::clearAll()
{
    if (!m_radio)
        return;
    for (const QString& token : std::as_const(m_tokensSet))
        m_radio->setAntennaLiveName(token, QString());
    m_tokensSet.clear();
}

} // namespace AetherSDR
