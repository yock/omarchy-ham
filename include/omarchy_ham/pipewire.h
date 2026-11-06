#pragma once

#include "omarchy_ham/profile.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace omarchy::ham {

class PipeWire {
public:
  static bool nodeExists(const QString &nodeName, AudioDirection direction,
                         QString *error = nullptr);
  static QStringList nodes(AudioDirection direction, QString *error = nullptr);
  static QStringList parseDump(const QByteArray &dump, AudioDirection direction,
                               QString *error = nullptr);
};

} // namespace omarchy::ham
