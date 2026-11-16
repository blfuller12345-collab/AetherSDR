#pragma once

#include "core/AntennaGeniusPortMap.h"
#include "models/AntennaGeniusModel.h"

#include <QObject>
#include <QSet>
#include <QString>

namespace AetherSDR {

class RadioModel;

// Shows the antenna an Antenna Genius port has selected as the name of the
// radio antenna connector that port is wired to (#4892). With port A wired
// to ANT1, ANT1's buttons read "Hex Beam" on 20 m and "Doublet" on 40 m,
// following the AG's own band switching.
//
// The names are live only (RadioModel::setAntennaLiveName); nothing is
// persisted, and the operator's saved aliases are untouched. With no AG
// connected, or no port mapped, every name is cleared and the radio's
// buttons look exactly as before.
class AntennaGeniusNameBridge : public QObject {
    Q_OBJECT

public:
    AntennaGeniusNameBridge(RadioModel* radio, AntennaGeniusModel* ag,
                            QObject* parent = nullptr);

private:
    void reloadMap();
    void refresh();
    void clearAll();

    RadioModel*         m_radio{nullptr};
    AntennaGeniusModel* m_ag{nullptr};
    AntennaGeniusPortMap m_map;
    QSet<QString>       m_tokensSet;   // tokens this bridge has named
};

} // namespace AetherSDR
