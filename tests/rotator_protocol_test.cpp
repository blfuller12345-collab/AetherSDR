// Rotator applet wire protocols (GS-232A, Hamlib rotctld) and the stored
// rotator list. No sockets.
#include "core/RotatorProtocol.h"

#include <QJsonArray>
#include <iostream>

using namespace AetherSDR;
namespace R = AetherSDR::Rotator;

namespace {

bool expect(bool condition, const char* label)
{
    std::cout << (condition ? "[ OK ] " : "[FAIL] ") << label << '\n';
    return condition;
}

}  // namespace

int main()
{
    bool ok = true;

    // ── Commands ──────────────────────────────────────────────────────────
    ok &= expect(R::readCommand(R::Protocol::Gs232A) == "C2\r", "GS-232A reads with C2");
    ok &= expect(R::moveCommand(R::Protocol::Gs232A, 52) == "M052\r",
                 "GS-232A turn is M plus three digits");
    ok &= expect(R::moveCommand(R::Protocol::Gs232A, 365) == "M005\r"
                     && R::moveCommand(R::Protocol::Gs232A, -10) == "M350\r",
                 "GS-232A turn wraps onto 0..359");
    ok &= expect(R::stopCommand(R::Protocol::Gs232A) == "S\r", "GS-232A stop is S");
    ok &= expect(R::readCommand(R::Protocol::Rotctld) == "p\n", "rotctld reads with p");
    ok &= expect(R::moveCommand(R::Protocol::Rotctld, 270) == "P 270 0\n",
                 "rotctld turn is P az 0");
    ok &= expect(R::stopCommand(R::Protocol::Rotctld) == "S\n", "rotctld stop is S");

    // ── GS-232A replies ───────────────────────────────────────────────────
    {
        QByteArray b("+0123+0045\r");
        ok &= expect(R::takeHeading(R::Protocol::Gs232A, b) == 123 && b.isEmpty(),
                     "GS-232A C2 reply gives azimuth");
        QByteArray single("+0007\r\n");
        ok &= expect(R::takeHeading(R::Protocol::Gs232A, single) == 7, "GS-232A C reply");
        QByteArray az("AZ=241\r");
        ok &= expect(R::takeHeading(R::Protocol::Gs232A, az) == 241, "AZ= form accepted");
        QByteArray partial("+01");
        ok &= expect(!R::takeHeading(R::Protocol::Gs232A, partial) && partial == "+01",
                     "a partial reply waits for the rest");
        partial.append("80\r");
        ok &= expect(R::takeHeading(R::Protocol::Gs232A, partial) == 180,
                     "the rest completes it");
        QByteArray noise("?>\r+0090\r");
        ok &= expect(R::takeHeading(R::Protocol::Gs232A, noise) == 90,
                     "a non-heading line before a reply is skipped");
        QByteArray overlap("+0400\r");
        ok &= expect(R::takeHeading(R::Protocol::Gs232A, overlap) == 40,
                     "an overlap reading past 360 folds back");
        QByteArray two("+0010\r+0020\r");
        ok &= expect(R::takeHeading(R::Protocol::Gs232A, two) == 10
                         && R::takeHeading(R::Protocol::Gs232A, two) == 20,
                     "back-to-back replies are taken in order");
    }

    // ── rotctld replies ───────────────────────────────────────────────────
    {
        QByteArray b("123.400000\n10.000000\n");
        ok &= expect(R::takeHeading(R::Protocol::Rotctld, b) == 123 && b.isEmpty(),
                     "rotctld p reply gives the azimuth, elevation consumed");
        QByteArray half("200.0\n");
        ok &= expect(!R::takeHeading(R::Protocol::Rotctld, half),
                     "rotctld waits for the elevation line");
        half.append("0.0\n");
        ok &= expect(R::takeHeading(R::Protocol::Rotctld, half) == 200,
                     "then takes the pair");
        QByteArray rprt("RPRT 0\n45\n0\n");
        ok &= expect(R::takeHeading(R::Protocol::Rotctld, rprt) == 45,
                     "an RPRT acknowledgement is not a heading");
        QByteArray neg("-10.0\n0\n");
        ok &= expect(R::takeHeading(R::Protocol::Rotctld, neg) == 350,
                     "a negative azimuth folds onto 0..359");
    }

    {
        QByteArray junk(5000, 'x');
        ok &= expect(!R::takeHeading(R::Protocol::Gs232A, junk) && junk.isEmpty(),
                     "a runaway buffer is cleared");
    }

    // ── Stored list ───────────────────────────────────────────────────────
    {
        QList<R::Config> list;
        R::Config south;
        south.id = QStringLiteral("rot-1");
        south.name = QStringLiteral("South 2m Yagi");
        south.host = QStringLiteral("192.168.1.159");
        south.port = 4001;
        list.append(south);
        R::Config pst;
        pst.id = QStringLiteral("rot-2");
        pst.name = QStringLiteral("PstRotator");
        pst.host = QStringLiteral("localhost");
        pst.port = 4533;
        pst.protocol = R::Protocol::Rotctld;
        pst.enabled = false;
        list.append(pst);
        R::Config blank;
        blank.id = QStringLiteral("rot-3");
        list.append(blank);

        const QList<R::Config> back = R::fromJson(R::toJson(list));
        ok &= expect(back.size() == 2, "a rotator with no address is not stored");
        ok &= expect(back.value(0) == south, "the first rotator round-trips");
        ok &= expect(back.value(1) == pst, "protocol, port and enabled round-trip");

        QJsonObject odd = R::toJson(list);
        QJsonObject withoutIds;
        QJsonArray arr;
        for (QJsonValue v : odd.value(QStringLiteral("rotators")).toArray()) {
            QJsonObject o = v.toObject();
            o.remove(QStringLiteral("id"));
            o.remove(QStringLiteral("name"));
            o[QStringLiteral("port")] = 0;
            arr.append(o);
        }
        withoutIds[QStringLiteral("rotators")] = arr;
        const QList<R::Config> fixed = R::fromJson(withoutIds);
        ok &= expect(fixed.size() == 2 && !fixed.at(0).id.isEmpty()
                         && fixed.at(0).id != fixed.at(1).id,
                     "missing ids are minted, and unique");
        ok &= expect(fixed.at(0).name == QStringLiteral("192.168.1.159"),
                     "a missing name falls back to the address");
        ok &= expect(fixed.at(0).port == 4001 && fixed.at(1).port == 4533,
                     "a bad port falls back to the protocol's usual port");
        ok &= expect(R::uniqueId(back) == QStringLiteral("rot-3"), "next id is the lowest free");
    }

    return ok ? 0 : 1;
}
