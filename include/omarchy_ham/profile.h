#pragma once

#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <optional>

namespace omarchy::ham {

enum class GridSource { Manual, Gps };
enum class TncKind { Serial, Bluetooth };
enum class AudioDirection { Rx, Tx };

struct ValidationError {
  QString path;
  QString message;
};

struct Operator {
  std::optional<QString> callsign;
  std::optional<QString> grid;
  std::optional<GridSource> gridSource;

  GridSource effectiveGridSource() const
  {
    return gridSource.value_or(GridSource::Manual);
  }
};

struct Station {
  QStringList paths;
};

struct AudioRoleBinding {
  QString node;
};

struct Audio {
  std::optional<AudioRoleBinding> rx;
  std::optional<AudioRoleBinding> tx;
};

struct Tnc {
  TncKind kind = TncKind::Serial;
  QString serialPath;
  QString bluetoothAddress;
  std::optional<int> baud;
};

struct Cat {
  QString kind = QStringLiteral("rigctld");
  std::optional<QString> host;
  std::optional<int> port;

  QString effectiveHost() const { return host.value_or(QStringLiteral("localhost")); }
  int effectivePort() const { return port.value_or(4532); }
};

struct Profile {
  int version = 1;
  std::optional<QString> schemaUrl;
  std::optional<Operator> operatorSection;
  std::optional<Station> station;
  std::optional<Audio> audio;
  std::optional<Tnc> tnc;
  std::optional<Cat> cat;
};

class ProfileCodec {
public:
  static std::optional<Profile> parse(const QByteArray &json,
                                      QList<ValidationError> *errors = nullptr);
  static QByteArray serialize(const Profile &profile);
  static bool validate(const Profile &profile, QList<ValidationError> *errors = nullptr);
};

QString gridSourceToString(GridSource source);
std::optional<GridSource> gridSourceFromString(const QString &value);
QString normalizeGrid(const QString &grid);

} // namespace omarchy::ham

Q_DECLARE_METATYPE(omarchy::ham::ValidationError)
Q_DECLARE_METATYPE(omarchy::ham::Profile)
Q_DECLARE_METATYPE(QList<omarchy::ham::ValidationError>)
