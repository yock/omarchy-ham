#include "omarchy_ham/gps.h"
#include "omarchy_ham/maidenhead.h"
#include "omarchy_ham/pipewire.h"
#include "omarchy_ham/profile.h"
#include "omarchy_ham/profile_store.h"
#include "omarchy_ham/profile_watcher.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QTextStream>

#include <cstdio>
#include <optional>

#ifndef OMARCHY_HAM_VERSION
#define OMARCHY_HAM_VERSION "0.1.0"
#endif

using namespace omarchy::ham;

namespace {

constexpr int kExitOk = 0;
constexpr int kExitError = 1;
constexpr int kExitUsage = 2;

int fail(const QString &message)
{
  std::fprintf(stderr, "%s\n", qPrintable(message));
  return kExitError;
}

int usage(const QString &message)
{
  if (!message.isEmpty())
    std::fprintf(stderr, "%s\n", qPrintable(message));
  std::fprintf(stderr, "Try 'omarchy-ham --help' for usage.\n");
  return kExitUsage;
}

void printErrors(const QList<ValidationError> &errors)
{
  for (const ValidationError &error : errors) {
    const QByteArray path = (error.path.isEmpty() ? QStringLiteral("$") : error.path).toUtf8();
    const QByteArray message = error.message.toUtf8();
    std::fprintf(stderr, "%s: %s\n", path.constData(), message.constData());
  }
}

void printProfile(const Profile &profile)
{
  const QByteArray data = ProfileCodec::serialize(profile);
  std::fwrite(data.constData(), 1, static_cast<size_t>(data.size()), stdout);
  std::fflush(stdout);
}

QString compactValue(const QJsonValue &value)
{
  QJsonArray wrapper;
  wrapper.append(value);
  const QString encoded =
      QString::fromUtf8(QJsonDocument(wrapper).toJson(QJsonDocument::Compact));
  return encoded.mid(1, encoded.size() - 2);
}

void printField(const QJsonValue &value)
{
  if (value.isUndefined())
    return;
  if (value.isString()) {
    std::printf("%s\n", qPrintable(value.toString()));
    return;
  }
  std::printf("%s\n", qPrintable(compactValue(value)));
}

QJsonValue lookup(const QJsonObject &object, const QString &field)
{
  QJsonValue current(object);
  for (const QString &part : field.split(QLatin1Char('.'))) {
    if (!current.isObject())
      return QJsonValue(QJsonValue::Undefined);
    current = current.toObject().value(part);
  }
  return current;
}

const QSet<QString> &knownFields()
{
  static const QSet<QString> fields = {
      QStringLiteral("version"),
      QStringLiteral("$schema"),
      QStringLiteral("operator"),
      QStringLiteral("operator.callsign"),
      QStringLiteral("operator.grid"),
      QStringLiteral("operator.gridSource"),
      QStringLiteral("station"),
      QStringLiteral("station.paths"),
      QStringLiteral("audio"),
      QStringLiteral("audio.rx"),
      QStringLiteral("audio.rx.node"),
      QStringLiteral("audio.tx"),
      QStringLiteral("audio.tx.node"),
      QStringLiteral("tnc"),
      QStringLiteral("tnc.transport"),
      QStringLiteral("tnc.baud"),
      QStringLiteral("cat"),
      QStringLiteral("cat.kind"),
      QStringLiteral("cat.host"),
      QStringLiteral("cat.port"),
  };
  return fields;
}

int getField(const Profile &profile, const QString &field)
{
  if (!knownFields().contains(field))
    return usage(QStringLiteral("unknown field: %1").arg(field));

  if (field == QStringLiteral("version")) {
    std::printf("%d\n", profile.version);
    return kExitOk;
  }

  if (field == QStringLiteral("operator.gridSource")) {
    const GridSource source = profile.operatorSection
                                  ? profile.operatorSection->effectiveGridSource()
                                  : GridSource::Manual;
    std::printf("%s\n", qPrintable(gridSourceToString(source)));
    return kExitOk;
  }

  if (field == QStringLiteral("cat.host")) {
    if (profile.cat)
      std::printf("%s\n", qPrintable(profile.cat->effectiveHost()));
    return kExitOk;
  }

  if (field == QStringLiteral("cat.port")) {
    if (profile.cat)
      std::printf("%d\n", profile.cat->effectivePort());
    return kExitOk;
  }

  const QJsonObject object =
      QJsonDocument::fromJson(ProfileCodec::serialize(profile)).object();
  printField(lookup(object, field));
  return kExitOk;
}

bool setError(QList<ValidationError> *errors, const QString &field, const QString &message)
{
  if (errors)
    errors->append({field, message});
  return false;
}

bool applySet(Profile &profile, const QString &field, const QString &value,
              QList<ValidationError> *errors)
{
  auto ensureOperator = [&profile]() -> Operator & {
    if (!profile.operatorSection)
      profile.operatorSection = Operator{};
    return *profile.operatorSection;
  };
  auto ensureAudio = [&profile]() -> Audio & {
    if (!profile.audio)
      profile.audio = Audio{};
    return *profile.audio;
  };
  auto ensureTnc = [&profile]() -> Tnc & {
    if (!profile.tnc)
      profile.tnc = Tnc{};
    return *profile.tnc;
  };
  auto ensureCat = [&profile]() -> Cat & {
    if (!profile.cat)
      profile.cat = Cat{};
    return *profile.cat;
  };

  if (field == QStringLiteral("operator.callsign")) {
    ensureOperator().callsign = value;
    return true;
  }
  if (field == QStringLiteral("operator.grid")) {
    ensureOperator().grid = value;
    return true;
  }
  if (field == QStringLiteral("operator.gridSource")) {
    const auto source = gridSourceFromString(value);
    if (!source)
      return setError(errors, field, QStringLiteral("must be 'manual' or 'gps'"));
    ensureOperator().gridSource = *source;
    return true;
  }
  if (field == QStringLiteral("station.paths")) {
    QStringList paths;
    for (const QString &raw : value.split(QLatin1Char(','))) {
      const QString path = raw.trimmed();
      if (path.isEmpty())
        return setError(errors, field, QStringLiteral("must not contain an empty entry"));
      if (path != QStringLiteral("host-audio") && path != QStringLiteral("tnc"))
        return setError(errors, field,
                        QStringLiteral("entries must be 'host-audio' or 'tnc'"));
      paths.append(path);
    }
    if (paths.isEmpty())
      return setError(errors, field, QStringLiteral("must contain at least one path"));
    profile.station = Station{paths};
    return true;
  }
  if (field == QStringLiteral("audio.rx.node")) {
    if (value.isEmpty())
      return setError(errors, field, QStringLiteral("must not be empty"));
    Audio &audio = ensureAudio();
    if (!audio.rx)
      audio.rx = AudioRoleBinding{};
    audio.rx->node = value;
    return true;
  }
  if (field == QStringLiteral("audio.tx.node")) {
    if (value.isEmpty())
      return setError(errors, field, QStringLiteral("must not be empty"));
    Audio &audio = ensureAudio();
    if (!audio.tx)
      audio.tx = AudioRoleBinding{};
    audio.tx->node = value;
    return true;
  }
  if (field == QStringLiteral("tnc.transport")) {
    const int colon = value.indexOf(QLatin1Char(':'));
    if (colon <= 0 || colon == value.size() - 1)
      return setError(errors, field,
                      QStringLiteral("must be serial:<path> or bluetooth:<address>"));
    const QString kind = value.left(colon);
    const QString target = value.mid(colon + 1);
    Tnc &tnc = ensureTnc();
    if (kind == QStringLiteral("serial")) {
      tnc.kind = TncKind::Serial;
      tnc.serialPath = target;
      tnc.bluetoothAddress.clear();
    } else if (kind == QStringLiteral("bluetooth")) {
      tnc.kind = TncKind::Bluetooth;
      tnc.bluetoothAddress = target;
      tnc.serialPath.clear();
    } else {
      return setError(errors, field,
                      QStringLiteral("must be serial:<path> or bluetooth:<address>"));
    }
    return true;
  }
  if (field == QStringLiteral("tnc.baud")) {
    if (!profile.tnc)
      return setError(errors, field, QStringLiteral("set tnc.transport first"));
    bool ok = false;
    const int baud = value.toInt(&ok);
    if (!ok || baud < 1)
      return setError(errors, field, QStringLiteral("must be a positive integer"));
    profile.tnc->baud = baud;
    return true;
  }
  if (field == QStringLiteral("cat.kind")) {
    if (value != QStringLiteral("rigctld"))
      return setError(errors, field, QStringLiteral("must be 'rigctld' in version 1"));
    ensureCat().kind = value;
    return true;
  }
  if (field == QStringLiteral("cat.host")) {
    if (value.isEmpty())
      return setError(errors, field, QStringLiteral("must not be empty"));
    ensureCat().host = value;
    return true;
  }
  if (field == QStringLiteral("cat.port")) {
    bool ok = false;
    const int port = value.toInt(&ok);
    if (!ok || port < 1 || port > 65535)
      return setError(errors, field, QStringLiteral("must be between 1 and 65535"));
    ensureCat().port = port;
    return true;
  }
  if (field == QStringLiteral("version"))
    return setError(errors, field, QStringLiteral("version is fixed at 1"));
  if (field == QStringLiteral("$schema"))
    return setError(errors, field,
                    QStringLiteral("the CLI does not manage $schema; edit the profile directly"));

  return setError(errors, field, QStringLiteral("unknown field"));
}

bool applyUnset(Profile &profile, const QString &field, QList<ValidationError> *errors)
{
  auto emptyOperator = [](const Operator &op) {
    return !op.callsign && !op.grid && !op.gridSource;
  };
  auto emptyAudio = [](const Audio &audio) { return !audio.rx && !audio.tx; };

  if (field == QStringLiteral("operator")) {
    profile.operatorSection.reset();
    return true;
  }
  if (field == QStringLiteral("operator.callsign")
      || field == QStringLiteral("operator.grid")
      || field == QStringLiteral("operator.gridSource")) {
    if (profile.operatorSection) {
      if (field == QStringLiteral("operator.callsign"))
        profile.operatorSection->callsign.reset();
      else if (field == QStringLiteral("operator.grid"))
        profile.operatorSection->grid.reset();
      else
        profile.operatorSection->gridSource.reset();
      if (emptyOperator(*profile.operatorSection))
        profile.operatorSection.reset();
    }
    return true;
  }
  if (field == QStringLiteral("station")) {
    profile.station.reset();
    return true;
  }
  if (field == QStringLiteral("station.paths"))
    return setError(errors, field,
                    QStringLiteral("is required when station exists; unset station instead"));
  if (field == QStringLiteral("audio")) {
    profile.audio.reset();
    return true;
  }
  if (field == QStringLiteral("audio.rx") || field == QStringLiteral("audio.rx.node")) {
    if (profile.audio) {
      profile.audio->rx.reset();
      if (emptyAudio(*profile.audio))
        profile.audio.reset();
    }
    return true;
  }
  if (field == QStringLiteral("audio.tx") || field == QStringLiteral("audio.tx.node")) {
    if (profile.audio) {
      profile.audio->tx.reset();
      if (emptyAudio(*profile.audio))
        profile.audio.reset();
    }
    return true;
  }
  if (field == QStringLiteral("tnc")) {
    profile.tnc.reset();
    return true;
  }
  if (field == QStringLiteral("tnc.baud")) {
    if (profile.tnc)
      profile.tnc->baud.reset();
    return true;
  }
  if (field == QStringLiteral("tnc.transport"))
    return setError(errors, field,
                    QStringLiteral("is required when tnc exists; unset tnc instead"));
  if (field == QStringLiteral("cat")) {
    profile.cat.reset();
    return true;
  }
  if (field == QStringLiteral("cat.host") || field == QStringLiteral("cat.port")) {
    if (profile.cat) {
      if (field == QStringLiteral("cat.host"))
        profile.cat->host.reset();
      else
        profile.cat->port.reset();
    }
    return true;
  }
  if (field == QStringLiteral("cat.kind"))
    return setError(errors, field,
                    QStringLiteral("is required when cat exists; unset cat instead"));
  if (field == QStringLiteral("version"))
    return setError(errors, field, QStringLiteral("version is fixed at 1"));
  if (field == QStringLiteral("$schema"))
    return setError(errors, field,
                    QStringLiteral("the CLI does not manage $schema; edit the profile directly"));

  return setError(errors, field, QStringLiteral("unknown field"));
}

std::optional<AudioDirection> parseDirection(const QString &value)
{
  if (value == QStringLiteral("rx"))
    return AudioDirection::Rx;
  if (value == QStringLiteral("tx"))
    return AudioDirection::Tx;
  return std::nullopt;
}

int commandStation(const QStringList &arguments, const QString &path)
{
  if (arguments.isEmpty())
    return usage(QStringLiteral("station requires a subcommand"));

  const QString subcommand = arguments.first();
  ProfileStore store(path);

  if (subcommand == QStringLiteral("path")) {
    if (arguments.size() != 1)
      return usage(QStringLiteral("station path takes no arguments"));
    std::printf("%s\n", qPrintable(store.path()));
    return kExitOk;
  }

  if (subcommand == QStringLiteral("validate")) {
    if (arguments.size() != 1)
      return usage(QStringLiteral("station validate takes no arguments"));
    const LoadResult result = store.load();
    if (!result.found) {
      std::printf("no profile at %s (unconfigured)\n", qPrintable(store.path()));
      return kExitOk;
    }
    if (!result.profile) {
      printErrors(result.errors);
      return kExitError;
    }
    std::printf("valid: %s\n", qPrintable(store.path()));
    return kExitOk;
  }

  if (subcommand == QStringLiteral("migrate")) {
    if (arguments.size() != 1)
      return usage(QStringLiteral("station migrate takes no arguments"));
    const LoadResult result = store.load();
    if (!result.found) {
      std::printf("no profile at %s (nothing to migrate)\n", qPrintable(store.path()));
      return kExitOk;
    }
    if (!result.profile) {
      printErrors(result.errors);
      return kExitError;
    }
    std::printf("version %d is current\n", result.profile->version);
    return kExitOk;
  }

  if (subcommand == QStringLiteral("get")) {
    if (arguments.size() > 2)
      return usage(QStringLiteral("station get [field]"));
    if (arguments.size() == 2 && !knownFields().contains(arguments.at(1)))
      return usage(QStringLiteral("unknown field: %1").arg(arguments.at(1)));
    const LoadResult result = store.load();
    Profile empty;
    const Profile &profile = result.profile ? *result.profile : empty;
    if (result.found && !result.profile) {
      printErrors(result.errors);
      return kExitError;
    }
    if (arguments.size() == 1) {
      printProfile(profile);
      return kExitOk;
    }
    return getField(profile, arguments.at(1));
  }

  if (subcommand == QStringLiteral("set")) {
    if (arguments.size() != 3)
      return usage(QStringLiteral("station set <field> <value>"));
    QList<ValidationError> errors;
    const bool ok = store.update(
        [&](Profile &profile) {
          return applySet(profile, arguments.at(1), arguments.at(2), &errors);
        },
        &errors);
    if (!ok) {
      printErrors(errors);
      return kExitError;
    }
    return kExitOk;
  }

  if (subcommand == QStringLiteral("unset")) {
    if (arguments.size() != 2)
      return usage(QStringLiteral("station unset <field>"));
    QList<ValidationError> errors;
    const bool ok = store.update(
        [&](Profile &profile) { return applyUnset(profile, arguments.at(1), &errors); },
        &errors);
    if (!ok) {
      printErrors(errors);
      return kExitError;
    }
    return kExitOk;
  }

  if (subcommand == QStringLiteral("watch")) {
    if (arguments.size() != 1)
      return usage(QStringLiteral("station watch takes no arguments"));
    ProfileWatcher watcher(path);
    QObject::connect(&watcher, &ProfileWatcher::changed,
                     [](const Profile &profile) { printProfile(profile); });
    QObject::connect(&watcher, &ProfileWatcher::missing, []() {
      std::fputs("{\"event\":\"missing\"}\n", stdout);
      std::fflush(stdout);
    });
    QObject::connect(&watcher, &ProfileWatcher::invalid,
                     [](const QList<ValidationError> &errors) { printErrors(errors); });
    watcher.start();
    return QCoreApplication::exec();
  }

  return usage(QStringLiteral("unknown station subcommand: %1").arg(subcommand));
}

int commandAudio(const QStringList &arguments)
{
  if (arguments.isEmpty())
    return usage(QStringLiteral("audio requires a subcommand"));

  const QString subcommand = arguments.first();
  if (subcommand == QStringLiteral("node-exists")) {
    if (arguments.size() != 3)
      return usage(QStringLiteral("audio node-exists <rx|tx> <node>"));
    const auto direction = parseDirection(arguments.at(1));
    if (!direction)
      return usage(QStringLiteral("direction must be 'rx' or 'tx'"));
    QString error;
    const bool exists = PipeWire::nodeExists(arguments.at(2), *direction, &error);
    if (!error.isEmpty())
      return fail(error);
    std::printf("%s\n", exists ? "present" : "missing");
    return exists ? kExitOk : kExitError;
  }

  if (subcommand == QStringLiteral("list")) {
    if (arguments.size() != 2)
      return usage(QStringLiteral("audio list <rx|tx>"));
    const auto direction = parseDirection(arguments.at(1));
    if (!direction)
      return usage(QStringLiteral("direction must be 'rx' or 'tx'"));
    QString error;
    const QStringList nodes = PipeWire::nodes(*direction, &error);
    if (!error.isEmpty())
      return fail(error);
    for (const QString &node : nodes)
      std::printf("%s\n", qPrintable(node));
    return kExitOk;
  }

  return usage(QStringLiteral("unknown audio subcommand: %1").arg(subcommand));
}

int commandGrid(const QStringList &arguments, const QString &path, bool fromGps,
                bool force, bool dryRun)
{
  if (!arguments.isEmpty())
    return usage(QStringLiteral("grid takes no positional arguments"));
  if (!fromGps)
    return usage(QStringLiteral("grid currently requires --from-gps"));

  ProfileStore store(path);
  const LoadResult result = store.load();
  if (!result.found)
    return fail(QStringLiteral("no profile at %1").arg(store.path()));
  if (!result.profile) {
    printErrors(result.errors);
    return kExitError;
  }

  const GridSource source = result.profile->operatorSection
                                ? result.profile->operatorSection->effectiveGridSource()
                                : GridSource::Manual;
  if (source != GridSource::Gps && !force)
    return fail(QStringLiteral(
        "operator.gridSource is manual; set it to gps or pass --force"));

  QString gpsError;
  const auto fix = Gps::currentFix(&gpsError);
  if (!fix)
    return fail(gpsError);

  const QString grid = maidenhead(fix->latitude, fix->longitude, 6);
  if (grid.isEmpty())
    return fail(QStringLiteral("cannot compute a Maidenhead grid from the GPS fix"));

  if (dryRun) {
    std::printf("%s\n", qPrintable(grid));
    return kExitOk;
  }

  QList<ValidationError> errors;
  const bool ok = store.update(
      [&](Profile &profile) {
        if (!profile.operatorSection)
          profile.operatorSection = Operator{};
        profile.operatorSection->grid = grid;
        return true;
      },
      &errors);
  if (!ok) {
    printErrors(errors);
    return kExitError;
  }

  std::printf("%s\n", qPrintable(grid));
  return kExitOk;
}

} // namespace

int main(int argc, char **argv)
{
  QCoreApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("omarchy-ham"));
  application.setApplicationVersion(QStringLiteral(OMARCHY_HAM_VERSION));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral(
      "Shared station profile for Omarchy ham radio apps.\n"
      "\n"
      "Commands:\n"
      "  station path\n"
      "  station get [field]\n"
      "  station set <field> <value>\n"
      "  station unset <field>\n"
      "  station validate\n"
      "  station migrate\n"
      "  station watch\n"
      "  audio node-exists <rx|tx> <node>\n"
      "  audio list <rx|tx>\n"
      "  grid --from-gps [--force] [--dry-run]\n"
      "\n"
      "Fields:\n"
      "  operator.callsign, operator.grid, operator.gridSource\n"
      "  station.paths (comma-separated)\n"
      "  audio.rx.node, audio.tx.node\n"
      "  tnc.transport (serial:<path> | bluetooth:<address>), tnc.baud\n"
      "  cat.kind, cat.host, cat.port\n"
      "\n"
      "Exit codes: 0 success, 1 error, 2 usage."));
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption profileOption(
      {QStringLiteral("p"), QStringLiteral("profile")},
      QStringLiteral("Read and write this profile instead of the default path."),
      QStringLiteral("path"));
  parser.addOption(profileOption);

  QCommandLineOption fromGpsOption(QStringLiteral("from-gps"),
                                   QStringLiteral("Use the current GPS fix."));
  parser.addOption(fromGpsOption);

  QCommandLineOption forceOption(
      QStringLiteral("force"),
      QStringLiteral("Allow a grid update even when operator.gridSource is manual."));
  parser.addOption(forceOption);

  QCommandLineOption dryRunOption(QStringLiteral("dry-run"),
                                  QStringLiteral("Print the grid without writing it."));
  parser.addOption(dryRunOption);

  parser.addPositionalArgument(QStringLiteral("command"),
                               QStringLiteral("station, audio or grid."));
  parser.addPositionalArgument(QStringLiteral("args"),
                               QStringLiteral("Command arguments."), QStringLiteral("[args...]"));
  parser.process(application);

  QStringList arguments = parser.positionalArguments();
  if (arguments.isEmpty())
    parser.showHelp(kExitUsage);

  const QString path = parser.isSet(profileOption) ? parser.value(profileOption)
                                                   : ProfileStore::defaultPath();
  const QString command = arguments.takeFirst();

  if (command == QStringLiteral("station"))
    return commandStation(arguments, path);
  if (command == QStringLiteral("audio"))
    return commandAudio(arguments);
  if (command == QStringLiteral("grid"))
    return commandGrid(arguments, path, parser.isSet(fromGpsOption),
                       parser.isSet(forceOption), parser.isSet(dryRunOption));

  return usage(QStringLiteral("unknown command: %1").arg(command));
}
