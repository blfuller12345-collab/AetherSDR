#pragma once

// Rotator control over TCP (Rotator applet): the two wire protocols most
// station rotator controllers and rotator programs speak, and the station's
// list of rotators. Pure functions and plain data, no sockets, so every
// command and parse rule is unit-tested (tests/rotator_protocol_test.cpp);
// RotatorClient owns the socket.
//
//   GS-232A  Yaesu GS-232A command set over raw TCP, as served by microHAM
//            ARCO, ERC, many "GS-232 compatible" interfaces and ser2net in
//            front of a GS-232 controller. Commands end in CR.
//              C2    -> "+0aaa+0eee" (azimuth, elevation); some send "AZ=aaa"
//              Mnnn  turn to azimuth nnn (3 digits)
//              S     stop all rotation
//   rotctld  Hamlib's network rotator daemon (also PstRotator's Hamlib
//            emulation). Commands end in LF.
//              p     -> "<az>\n<el>\n"
//              P az el   turn
//              S     stop

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

namespace AetherSDR {

namespace Rotator {

enum class Protocol { Gs232A, Rotctld };

QString protocolKey(Protocol p);            // "gs232a" / "rotctld"
Protocol protocolFromKey(const QString& key);  // unknown -> Gs232A
quint16 defaultPort(Protocol p);            // 4001 / 4533

// Azimuth folded onto 0..359.
int normalizedAzimuth(int deg);

QByteArray readCommand(Protocol p);
QByteArray moveCommand(Protocol p, int azimuthDeg);
QByteArray stopCommand(Protocol p);

// Take one complete heading reply off the front of `buffer` (removing what
// it used). Returns the azimuth, or nothing when no complete reply is there
// yet. Lines that are not a heading (a rotctld "RPRT 0" acknowledgement, an
// echo) are dropped. A buffer that grows past a sane size without a reply
// is cleared, so a misbehaving device cannot grow it forever.
std::optional<int> takeHeading(Protocol p, QByteArray& buffer);

// One configured rotator.
struct Config {
    QString  id;        // stable, generated once ("rot-1", "rot-2", ...)
    QString  name;      // what the operator calls it: "South 2m Yagi"
    QString  host;
    quint16  port{4001};
    Protocol protocol{Protocol::Gs232A};
    bool     enabled{true};

    bool operator==(const Config& o) const
    {
        return id == o.id && name == o.name && host == o.host && port == o.port
               && protocol == o.protocol && enabled == o.enabled;
    }
};

// {"rotators":[{"id","name","host","port","protocol","enabled"}]}. Entries
// with no host are dropped on read; missing ids are minted.
QJsonObject toJson(const QList<Config>& rotators);
QList<Config> fromJson(const QJsonObject& obj);
QString uniqueId(const QList<Config>& rotators);

// Persisted as one AppSettings key, "Rotators" (Principle V). Station-wide,
// not per radio: the rotators belong to the towers, whichever radio is on.
QList<Config> loadConfigs();
void saveConfigs(const QList<Config>& rotators);

}  // namespace Rotator

}  // namespace AetherSDR
