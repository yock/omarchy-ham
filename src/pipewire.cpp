#include "omarchy_ham/pipewire.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QStandardPaths>

namespace omarchy::ham {

QStringList PipeWire::parseDump(const QByteArray &dump, AudioDirection direction,
                                QString *error)
{
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(dump, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
    if (error)
      *error = QStringLiteral("cannot parse pw-dump output");
    return {};
  }

  QStringList names;
  for (const QJsonValue &value : document.array()) {
    if (!value.isObject())
      continue;

    const QJsonObject object = value.toObject();
    if (object.value(QStringLiteral("type")).toString()
        != QStringLiteral("PipeWire:Interface:Node"))
      continue;

    const QJsonObject properties =
        object.value(QStringLiteral("info")).toObject().value(QStringLiteral("props")).toObject();
    const QString name = properties.value(QStringLiteral("node.name")).toString();
    const QString mediaClass = properties.value(QStringLiteral("media.class")).toString();

    if (name.isEmpty() || mediaClass.startsWith(QStringLiteral("Stream/")))
      continue;

    const bool source = mediaClass.contains(QStringLiteral("Source"))
                        || mediaClass.contains(QStringLiteral("Duplex"));
    const bool sink = mediaClass.contains(QStringLiteral("Sink"))
                      || mediaClass.contains(QStringLiteral("Duplex"));

    if ((direction == AudioDirection::Rx && source)
        || (direction == AudioDirection::Tx && sink)) {
      names.append(name);
    }
  }

  names.removeDuplicates();
  return names;
}

QStringList PipeWire::nodes(AudioDirection direction, QString *error)
{
  const QString executable = QStandardPaths::findExecutable(QStringLiteral("pw-dump"));
  if (executable.isEmpty()) {
    if (error)
      *error = QStringLiteral("pw-dump not found");
    return {};
  }

  QProcess process;
  process.start(executable, {});
  if (!process.waitForStarted(2000)) {
    if (error)
      *error = QStringLiteral("cannot start pw-dump");
    return {};
  }

  if (!process.waitForFinished(5000)) {
    process.kill();
    process.waitForFinished(2000);
    if (error)
      *error = QStringLiteral("pw-dump timed out");
    return {};
  }

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (error)
      *error = QStringLiteral("pw-dump failed: %1")
                   .arg(QString::fromUtf8(process.readAllStandardError()).trimmed());
    return {};
  }

  return parseDump(process.readAllStandardOutput(), direction, error);
}

bool PipeWire::nodeExists(const QString &nodeName, AudioDirection direction, QString *error)
{
  if (nodeName.isEmpty()) {
    if (error)
      *error = QStringLiteral("node name is empty");
    return false;
  }

  const QStringList available = nodes(direction, error);
  return available.contains(nodeName);
}

} // namespace omarchy::ham
