// Distance/bearing text shared by the Callsign Card and the spot tooltip (#6603).
#include "gui/DistanceBearingText.h"

#include <QLocale>
#include <iostream>

using namespace AetherSDR;

namespace {

bool expect(bool condition, const char* label)
{
    std::cout << (condition ? "[ OK ] " : "[FAIL] ") << label << '\n';
    return condition;
}

} // namespace

int main()
{
    bool ok = true;

    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    ok &= expect(formatDistanceBearing(2594.3, 52.4, false)
                     == QStringLiteral("1,612 mi @ 052°"),
                 "US locale: miles, thousands separator, three-digit bearing");
    ok &= expect(formatDistanceBearing(2594.3, 52.4, true)
                     == QStringLiteral("~1,612 mi @ 052°"),
                 "country-center estimate gets a leading ~");
    ok &= expect(formatDistanceBearing(10.0, 359.6, false)
                     == QStringLiteral("6 mi @ 000°"),
                 "a bearing that rounds to 360 reads 000");

    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    const QString km = formatDistanceBearing(3881.2, 78.0, false);
    ok &= expect(km.endsWith(QStringLiteral(" km @ 078°")) && km.startsWith(QStringLiteral("3")),
                 "metric locale: kilometres");

    return ok ? 0 : 1;
}
