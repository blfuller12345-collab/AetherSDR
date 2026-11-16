#pragma once

#include <QLocale>
#include <QString>
#include <QtGlobal>

namespace AetherSDR {

// "2,412 mi @ 078°" / "3 881 km @ 078°" from the operator's position;
// "~" marks a DXCC-country-center estimate. The locale picks the unit.
// Shared by the Callsign Card and the panadapter spot tooltip (#6603) so
// both read the same.
inline QString formatDistanceBearing(double km, double bearingDeg, bool approx)
{
    const bool imperial =
        QLocale().measurementSystem() != QLocale::MetricSystem;
    const double value = imperial ? km * 0.621371 : km;
    const QString unit = imperial ? QStringLiteral("mi") : QStringLiteral("km");
    return QStringLiteral("%1%L2 %3 @ %4°")
        .arg(approx ? QStringLiteral("~") : QString())
        .arg(qRound(value))
        .arg(unit)
        .arg(qRound(bearingDeg) % 360, 3, 10, QLatin1Char('0'));
}

} // namespace AetherSDR
