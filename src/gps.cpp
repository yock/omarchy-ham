#include "omarchy_ham/gps.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QStandardPaths>

namespace omarchy::ham {

std::optional<GpsFix> Gps::parseGpspipe(const QByteArray &output, QString *error)
{
  for (const QByteArray &rawLine : output.split('\n')) {
    const QByteArray line = rawLine.trimmed();
    if (line.isEmpty())
      continue;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
      continue;

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("class")).toString() != QStringLiteral("TPV"))
      continue;

    const int mode = object.value(QStringLiteral("mode")).toInt();
    if (mode < 2)
      continue;

    if (!object.value(QStringLiteral("lat")).isDouble()
        || !object.value(QStringLiteral("lon")).isDouble())
      continue;

    return GpsFix{object.value(QStringLiteral("lat")).toDouble(),
                  object.value(QStringLiteral("lon")).toDouble()};
  }

  if (error)
    *error = QStringLiteral("no GPS fix in gpspipe output");
  return std::nullopt;
}

std::optional<GpsFix> Gps::currentFix(QString *error, int timeoutMs)
{
  const QString executable = QStandardPaths::findExecutable(QStringLiteral("gpspipe"));
  if (executable.isEmpty()) {
    if (error)
      *error = QStringLiteral("gpspipe not found");
    return std::nullopt;
  }

  QProcess process;
  process.start(executable, {QStringLiteral("-w"), QStringLiteral("-n"),
                            QStringLiteral("20")});
  if (!process.waitForStarted(2000)) {
    if (error)
      *error = QStringLiteral("cannot start gpspipe");
    return std::nullopt;
  }

  if (!process.waitForFinished(timeoutMs)) {
    process.kill();
    process.waitForFinished(2000);
    if (error)
      *error = QStringLiteral("gpspipe timed out");
    return std::nullopt;
  }

  return parseGpspipe(process.readAllStandardOutput(), error);
}

} // namespace omarchy::ham
