#include "omarchy_ham/maidenhead.h"

#include <QChar>
#include <QString>

namespace omarchy::ham {

QString maidenhead(double latitude, double longitude, int characters)
{
  if (characters != 4 && characters != 6 && characters != 8)
    return {};
  if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0)
    return {};

  double lon = longitude + 180.0;
  double lat = latitude + 90.0;

  if (lon >= 360.0)
    lon = 0.0;
  if (lat >= 180.0)
    lat = 179.999999;

  int fieldLon = static_cast<int>(lon / 20.0);
  int fieldLat = static_cast<int>(lat / 10.0);
  fieldLon = qBound(0, fieldLon, 17);
  fieldLat = qBound(0, fieldLat, 17);

  QString result;
  result += QChar(static_cast<char16_t>('A' + fieldLon));
  result += QChar(static_cast<char16_t>('A' + fieldLat));

  lon -= fieldLon * 20.0;
  lat -= fieldLat * 10.0;

  int squareLon = static_cast<int>(lon / 2.0);
  int squareLat = static_cast<int>(lat / 1.0);
  squareLon = qBound(0, squareLon, 9);
  squareLat = qBound(0, squareLat, 9);

  result += QChar(static_cast<char16_t>('0' + squareLon));
  result += QChar(static_cast<char16_t>('0' + squareLat));

  if (characters >= 6) {
    lon -= squareLon * 2.0;
    lat -= squareLat * 1.0;

    int subsquareLon = static_cast<int>(lon / (2.0 / 24.0));
    int subsquareLat = static_cast<int>(lat / (1.0 / 24.0));
    subsquareLon = qBound(0, subsquareLon, 23);
    subsquareLat = qBound(0, subsquareLat, 23);

    result += QChar(static_cast<char16_t>('a' + subsquareLon));
    result += QChar(static_cast<char16_t>('a' + subsquareLat));

    if (characters >= 8) {
      lon -= subsquareLon * (2.0 / 24.0);
      lat -= subsquareLat * (1.0 / 24.0);

      int extendedLon = static_cast<int>(lon / (2.0 / 240.0));
      int extendedLat = static_cast<int>(lat / (1.0 / 240.0));
      extendedLon = qBound(0, extendedLon, 9);
      extendedLat = qBound(0, extendedLat, 9);

      result += QChar(static_cast<char16_t>('0' + extendedLon));
      result += QChar(static_cast<char16_t>('0' + extendedLat));
    }
  }

  return result;
}

} // namespace omarchy::ham
