#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace omarchy::ham {

struct GpsFix {
  double latitude = 0.0;
  double longitude = 0.0;
};

class Gps {
public:
  static std::optional<GpsFix> currentFix(QString *error = nullptr, int timeoutMs = 10000);
  static std::optional<GpsFix> parseGpspipe(const QByteArray &output,
                                            QString *error = nullptr);
};

} // namespace omarchy::ham
