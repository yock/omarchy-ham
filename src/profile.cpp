#include "omarchy_ham/profile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

#include <cmath>
#include <limits>

namespace omarchy::ham {
namespace {

QString joinPath(const QString &base, const QString &key)
{
  return base.isEmpty() ? key : base + QLatin1Char('.') + key;
}

void addError(QList<ValidationError> *errors, const QString &path, const QString &message)
{
  if (errors)
    errors->append({path, message});
}

bool expectObject(const QJsonValue &value, const QString &path, QJsonObject *out,
                  QList<ValidationError> *errors)
{
  if (!value.isObject()) {
    addError(errors, path, QStringLiteral("must be an object"));
    return false;
  }
  *out = value.toObject();
  return true;
}

bool expectString(const QJsonValue &value, const QString &path, QString *out,
                  QList<ValidationError> *errors)
{
  if (!value.isString()) {
    addError(errors, path, QStringLiteral("must be a string"));
    return false;
  }
  *out = value.toString();
  return true;
}

bool expectInt(const QJsonValue &value, const QString &path, int *out,
               QList<ValidationError> *errors)
{
  if (!value.isDouble()) {
    addError(errors, path, QStringLiteral("must be an integer"));
    return false;
  }
  const double number = value.toDouble();
  if (std::floor(number) != number || number < std::numeric_limits<int>::min()
      || number > std::numeric_limits<int>::max()) {
    addError(errors, path, QStringLiteral("must be an integer"));
    return false;
  }
  *out = static_cast<int>(number);
  return true;
}

bool expectArray(const QJsonValue &value, const QString &path, QJsonArray *out,
                 QList<ValidationError> *errors)
{
  if (!value.isArray()) {
    addError(errors, path, QStringLiteral("must be an array"));
    return false;
  }
  *out = value.toArray();
  return true;
}

bool rejectUnknown(const QJsonObject &object, const QStringList &allowed,
                   const QString &path, QList<ValidationError> *errors)
{
  bool ok = true;
  for (auto it = object.begin(); it != object.end(); ++it) {
    if (!allowed.contains(it.key())) {
      addError(errors, joinPath(path, it.key()), QStringLiteral("unknown field"));
      ok = false;
    }
  }
  return ok;
}

const QRegularExpression &callsignPattern()
{
  static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9/]+$"));
  return pattern;
}

const QRegularExpression &gridPattern()
{
  static const QRegularExpression pattern(
      QStringLiteral("^[A-Ra-r]{2}[0-9]{2}([A-Xa-x]{2}([0-9]{2})?)?$"));
  return pattern;
}

const QRegularExpression &bluetoothPattern()
{
  static const QRegularExpression pattern(
      QStringLiteral("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$"));
  return pattern;
}

bool validateOperator(const QJsonValue &value, QList<ValidationError> *errors)
{
  QJsonObject object;
  if (!expectObject(value, QStringLiteral("operator"), &object, errors))
    return false;

  bool ok = rejectUnknown(object, {QStringLiteral("callsign"), QStringLiteral("grid"),
                                   QStringLiteral("gridSource")},
                          QStringLiteral("operator"), errors);

  if (object.contains(QStringLiteral("callsign"))) {
    QString callsign;
    if (!expectString(object.value(QStringLiteral("callsign")),
                      QStringLiteral("operator.callsign"), &callsign, errors)) {
      ok = false;
    } else if (callsign.size() < 3 || callsign.size() > 16) {
      addError(errors, QStringLiteral("operator.callsign"),
               QStringLiteral("must be between 3 and 16 characters"));
      ok = false;
    } else if (!callsignPattern().match(callsign).hasMatch()) {
      addError(errors, QStringLiteral("operator.callsign"),
               QStringLiteral("must contain only letters, digits and '/'"));
      ok = false;
    }
  }

  if (object.contains(QStringLiteral("grid"))) {
    QString grid;
    if (!expectString(object.value(QStringLiteral("grid")),
                      QStringLiteral("operator.grid"), &grid, errors)) {
      ok = false;
    } else if (!gridPattern().match(grid).hasMatch()) {
      addError(errors, QStringLiteral("operator.grid"),
               QStringLiteral("must be a 4, 6 or 8 character Maidenhead locator"));
      ok = false;
    }
  }

  if (object.contains(QStringLiteral("gridSource"))) {
    QString source;
    if (!expectString(object.value(QStringLiteral("gridSource")),
                      QStringLiteral("operator.gridSource"), &source, errors)) {
      ok = false;
    } else if (source != QStringLiteral("manual") && source != QStringLiteral("gps")) {
      addError(errors, QStringLiteral("operator.gridSource"),
               QStringLiteral("must be 'manual' or 'gps'"));
      ok = false;
    }
  }

  return ok;
}

bool validateStation(const QJsonValue &value, QList<ValidationError> *errors)
{
  QJsonObject object;
  if (!expectObject(value, QStringLiteral("station"), &object, errors))
    return false;

  bool ok = rejectUnknown(object, {QStringLiteral("paths")},
                          QStringLiteral("station"), errors);

  if (!object.contains(QStringLiteral("paths"))) {
    addError(errors, QStringLiteral("station.paths"),
             QStringLiteral("is required when station is present"));
    return false;
  }

  QJsonArray paths;
  if (!expectArray(object.value(QStringLiteral("paths")), QStringLiteral("station.paths"),
                   &paths, errors)) {
    return false;
  }

  if (paths.isEmpty()) {
    addError(errors, QStringLiteral("station.paths"),
             QStringLiteral("must contain at least one path"));
    ok = false;
  }

  QSet<QString> seen;
  for (int i = 0; i < paths.size(); ++i) {
    const QString path = QStringLiteral("station.paths[%1]").arg(i);
    if (!paths.at(i).isString()) {
      addError(errors, path, QStringLiteral("must be a string"));
      ok = false;
      continue;
    }
    const QString entry = paths.at(i).toString();
    if (entry != QStringLiteral("host-audio") && entry != QStringLiteral("tnc")) {
      addError(errors, path, QStringLiteral("must be 'host-audio' or 'tnc'"));
      ok = false;
    }
    if (seen.contains(entry)) {
      addError(errors, path, QStringLiteral("duplicate path"));
      ok = false;
    }
    seen.insert(entry);
  }

  return ok;
}

bool validateAudioRole(const QJsonValue &value, const QString &path,
                       QList<ValidationError> *errors)
{
  QJsonObject object;
  if (!expectObject(value, path, &object, errors))
    return false;

  bool ok = rejectUnknown(object, {QStringLiteral("node")}, path, errors);

  if (!object.contains(QStringLiteral("node"))) {
    addError(errors, joinPath(path, QStringLiteral("node")),
             QStringLiteral("is required when a role is present"));
    return false;
  }

  QString node;
  if (!expectString(object.value(QStringLiteral("node")),
                    joinPath(path, QStringLiteral("node")), &node, errors)) {
    ok = false;
  } else if (node.isEmpty()) {
    addError(errors, joinPath(path, QStringLiteral("node")),
             QStringLiteral("must not be empty"));
    ok = false;
  }

  return ok;
}

bool validateAudio(const QJsonValue &value, QList<ValidationError> *errors)
{
  QJsonObject object;
  if (!expectObject(value, QStringLiteral("audio"), &object, errors))
    return false;

  bool ok = rejectUnknown(object, {QStringLiteral("rx"), QStringLiteral("tx")},
                          QStringLiteral("audio"), errors);

  if (object.contains(QStringLiteral("rx")))
    ok = validateAudioRole(object.value(QStringLiteral("rx")), QStringLiteral("audio.rx"),
                           errors)
         && ok;
  if (object.contains(QStringLiteral("tx")))
    ok = validateAudioRole(object.value(QStringLiteral("tx")), QStringLiteral("audio.tx"),
                           errors)
         && ok;

  return ok;
}

bool validateTnc(const QJsonValue &value, QList<ValidationError> *errors)
{
  QJsonObject object;
  if (!expectObject(value, QStringLiteral("tnc"), &object, errors))
    return false;

  bool ok = rejectUnknown(object, {QStringLiteral("transport"), QStringLiteral("baud")},
                          QStringLiteral("tnc"), errors);

  if (!object.contains(QStringLiteral("transport"))) {
    addError(errors, QStringLiteral("tnc.transport"),
             QStringLiteral("is required when tnc is present"));
    return false;
  }

  QJsonObject transport;
  if (!expectObject(object.value(QStringLiteral("transport")),
                    QStringLiteral("tnc.transport"), &transport, errors)) {
    return false;
  }

  ok = rejectUnknown(transport,
                     {QStringLiteral("kind"), QStringLiteral("path"),
                      QStringLiteral("address")},
                     QStringLiteral("tnc.transport"), errors)
       && ok;

  if (!transport.contains(QStringLiteral("kind"))) {
    addError(errors, QStringLiteral("tnc.transport.kind"), QStringLiteral("is required"));
    ok = false;
  } else {
    QString kind;
    if (!expectString(transport.value(QStringLiteral("kind")),
                      QStringLiteral("tnc.transport.kind"), &kind, errors)) {
      ok = false;
    } else if (kind == QStringLiteral("serial")) {
      if (!transport.contains(QStringLiteral("path"))) {
        addError(errors, QStringLiteral("tnc.transport.path"),
                 QStringLiteral("is required for a serial transport"));
        ok = false;
      } else {
        QString path;
        if (!expectString(transport.value(QStringLiteral("path")),
                          QStringLiteral("tnc.transport.path"), &path, errors)) {
          ok = false;
        } else if (!path.startsWith(QStringLiteral("/dev/"))) {
          addError(errors, QStringLiteral("tnc.transport.path"),
                   QStringLiteral("must start with /dev/"));
          ok = false;
        }
      }
      if (transport.contains(QStringLiteral("address"))) {
        addError(errors, QStringLiteral("tnc.transport.address"),
                 QStringLiteral("is not valid for a serial transport"));
        ok = false;
      }
    } else if (kind == QStringLiteral("bluetooth")) {
      if (!transport.contains(QStringLiteral("address"))) {
        addError(errors, QStringLiteral("tnc.transport.address"),
                 QStringLiteral("is required for a bluetooth transport"));
        ok = false;
      } else {
        QString address;
        if (!expectString(transport.value(QStringLiteral("address")),
                          QStringLiteral("tnc.transport.address"), &address, errors)) {
          ok = false;
        } else if (!bluetoothPattern().match(address).hasMatch()) {
          addError(errors, QStringLiteral("tnc.transport.address"),
                   QStringLiteral("must be a colon-separated Bluetooth address"));
          ok = false;
        }
      }
      if (transport.contains(QStringLiteral("path"))) {
        addError(errors, QStringLiteral("tnc.transport.path"),
                 QStringLiteral("is not valid for a bluetooth transport"));
        ok = false;
      }
    } else {
      addError(errors, QStringLiteral("tnc.transport.kind"),
               QStringLiteral("must be 'serial' or 'bluetooth'"));
      ok = false;
    }
  }

  if (object.contains(QStringLiteral("baud"))) {
    int baud = 0;
    if (!expectInt(object.value(QStringLiteral("baud")), QStringLiteral("tnc.baud"),
                   &baud, errors)) {
      ok = false;
    } else if (baud < 1) {
      addError(errors, QStringLiteral("tnc.baud"),
               QStringLiteral("must be greater than zero"));
      ok = false;
    }
  }

  return ok;
}

bool validateCat(const QJsonValue &value, QList<ValidationError> *errors)
{
  QJsonObject object;
  if (!expectObject(value, QStringLiteral("cat"), &object, errors))
    return false;

  bool ok = rejectUnknown(object,
                          {QStringLiteral("kind"), QStringLiteral("host"),
                           QStringLiteral("port")},
                          QStringLiteral("cat"), errors);

  if (!object.contains(QStringLiteral("kind"))) {
    addError(errors, QStringLiteral("cat.kind"), QStringLiteral("is required"));
    ok = false;
  } else {
    QString kind;
    if (!expectString(object.value(QStringLiteral("kind")),
                      QStringLiteral("cat.kind"), &kind, errors)) {
      ok = false;
    } else if (kind != QStringLiteral("rigctld")) {
      addError(errors, QStringLiteral("cat.kind"),
               QStringLiteral("must be 'rigctld' in version 1"));
      ok = false;
    }
  }

  if (object.contains(QStringLiteral("host"))) {
    QString host;
    if (!expectString(object.value(QStringLiteral("host")),
                      QStringLiteral("cat.host"), &host, errors)) {
      ok = false;
    } else if (host.isEmpty()) {
      addError(errors, QStringLiteral("cat.host"),
               QStringLiteral("must not be empty"));
      ok = false;
    }
  }

  if (object.contains(QStringLiteral("port"))) {
    int port = 0;
    if (!expectInt(object.value(QStringLiteral("port")), QStringLiteral("cat.port"),
                   &port, errors)) {
      ok = false;
    } else if (port < 1 || port > 65535) {
      addError(errors, QStringLiteral("cat.port"),
               QStringLiteral("must be between 1 and 65535"));
      ok = false;
    }
  }

  return ok;
}

bool validateRoot(const QJsonObject &object, QList<ValidationError> *errors)
{
  bool ok = rejectUnknown(object,
                          {QStringLiteral("$schema"), QStringLiteral("version"),
                           QStringLiteral("operator"), QStringLiteral("station"),
                           QStringLiteral("audio"), QStringLiteral("tnc"),
                           QStringLiteral("cat")},
                          QString(), errors);

  if (!object.contains(QStringLiteral("version"))) {
    addError(errors, QStringLiteral("version"), QStringLiteral("is required"));
    return false;
  }

  int version = 0;
  if (!expectInt(object.value(QStringLiteral("version")), QStringLiteral("version"),
                 &version, errors)) {
    return false;
  }
  if (version != 1) {
    addError(errors, QStringLiteral("version"),
             QStringLiteral("unsupported profile version %1; this build supports version 1")
                 .arg(version));
    return false;
  }

  if (object.contains(QStringLiteral("$schema"))) {
    QString schema;
    if (!expectString(object.value(QStringLiteral("$schema")), QStringLiteral("$schema"),
                      &schema, errors)) {
      ok = false;
    } else if (schema.isEmpty()) {
      addError(errors, QStringLiteral("$schema"), QStringLiteral("must not be empty"));
      ok = false;
    }
  }

  if (object.contains(QStringLiteral("operator")))
    ok = validateOperator(object.value(QStringLiteral("operator")), errors) && ok;
  if (object.contains(QStringLiteral("station")))
    ok = validateStation(object.value(QStringLiteral("station")), errors) && ok;
  if (object.contains(QStringLiteral("audio")))
    ok = validateAudio(object.value(QStringLiteral("audio")), errors) && ok;
  if (object.contains(QStringLiteral("tnc")))
    ok = validateTnc(object.value(QStringLiteral("tnc")), errors) && ok;
  if (object.contains(QStringLiteral("cat")))
    ok = validateCat(object.value(QStringLiteral("cat")), errors) && ok;

  return ok;
}

QString scalarJson(const QJsonValue &value)
{
  QJsonArray wrapper;
  wrapper.append(value);
  const QString encoded =
      QString::fromUtf8(QJsonDocument(wrapper).toJson(QJsonDocument::Compact));
  return encoded.mid(1, encoded.size() - 2);
}

void writeJson(QString &out, const QJsonValue &value, int depth)
{
  const QString pad(depth * 2, QLatin1Char(' '));
  const QString childPad((depth + 1) * 2, QLatin1Char(' '));

  if (value.isObject()) {
    const QJsonObject object = value.toObject();
    if (object.isEmpty()) {
      out += QStringLiteral("{}");
      return;
    }
    out += QStringLiteral("{\n");
    bool first = true;
    for (auto it = object.begin(); it != object.end(); ++it) {
      if (!first)
        out += QStringLiteral(",\n");
      first = false;
      out += childPad;
      out += scalarJson(QJsonValue(it.key()));
      out += QStringLiteral(": ");
      writeJson(out, it.value(), depth + 1);
    }
    out += QLatin1Char('\n');
    out += pad;
    out += QLatin1Char('}');
    return;
  }

  if (value.isArray()) {
    const QJsonArray array = value.toArray();
    if (array.isEmpty()) {
      out += QStringLiteral("[]");
      return;
    }
    out += QStringLiteral("[\n");
    for (int i = 0; i < array.size(); ++i) {
      out += childPad;
      writeJson(out, array.at(i), depth + 1);
      if (i + 1 < array.size())
        out += QLatin1Char(',');
      out += QLatin1Char('\n');
    }
    out += pad;
    out += QLatin1Char(']');
    return;
  }

  out += scalarJson(value);
}

QByteArray prettyJson(const QJsonObject &object)
{
  QString out;
  writeJson(out, QJsonValue(object), 0);
  out += QLatin1Char('\n');
  return out.toUtf8();
}

QJsonObject toJson(const Profile &profile)
{
  QJsonObject root;
  root.insert(QStringLiteral("version"), profile.version);

  if (profile.schemaUrl)
    root.insert(QStringLiteral("$schema"), *profile.schemaUrl);

  if (profile.operatorSection) {
    const Operator &op = *profile.operatorSection;
    QJsonObject object;
    if (op.callsign)
      object.insert(QStringLiteral("callsign"), *op.callsign);
    if (op.grid)
      object.insert(QStringLiteral("grid"), *op.grid);
    if (op.gridSource)
      object.insert(QStringLiteral("gridSource"), gridSourceToString(*op.gridSource));
    root.insert(QStringLiteral("operator"), object);
  }

  if (profile.station) {
    QJsonObject object;
    object.insert(QStringLiteral("paths"), QJsonArray::fromStringList(profile.station->paths));
    root.insert(QStringLiteral("station"), object);
  }

  if (profile.audio) {
    QJsonObject object;
    if (profile.audio->rx) {
      QJsonObject role;
      role.insert(QStringLiteral("node"), profile.audio->rx->node);
      object.insert(QStringLiteral("rx"), role);
    }
    if (profile.audio->tx) {
      QJsonObject role;
      role.insert(QStringLiteral("node"), profile.audio->tx->node);
      object.insert(QStringLiteral("tx"), role);
    }
    root.insert(QStringLiteral("audio"), object);
  }

  if (profile.tnc) {
    QJsonObject object;
    QJsonObject transport;
    if (profile.tnc->kind == TncKind::Serial) {
      transport.insert(QStringLiteral("kind"), QStringLiteral("serial"));
      transport.insert(QStringLiteral("path"), profile.tnc->serialPath);
    } else {
      transport.insert(QStringLiteral("kind"), QStringLiteral("bluetooth"));
      transport.insert(QStringLiteral("address"), profile.tnc->bluetoothAddress);
    }
    object.insert(QStringLiteral("transport"), transport);
    if (profile.tnc->baud)
      object.insert(QStringLiteral("baud"), *profile.tnc->baud);
    root.insert(QStringLiteral("tnc"), object);
  }

  if (profile.cat) {
    QJsonObject object;
    object.insert(QStringLiteral("kind"), profile.cat->kind);
    if (profile.cat->host)
      object.insert(QStringLiteral("host"), *profile.cat->host);
    if (profile.cat->port)
      object.insert(QStringLiteral("port"), *profile.cat->port);
    root.insert(QStringLiteral("cat"), object);
  }

  return root;
}

} // namespace

QString gridSourceToString(GridSource source)
{
  return source == GridSource::Gps ? QStringLiteral("gps") : QStringLiteral("manual");
}

std::optional<GridSource> gridSourceFromString(const QString &value)
{
  if (value == QStringLiteral("manual"))
    return GridSource::Manual;
  if (value == QStringLiteral("gps"))
    return GridSource::Gps;
  return std::nullopt;
}

QString normalizeGrid(const QString &grid)
{
  const QString trimmed = grid.trimmed();
  if (trimmed.size() == 4)
    return trimmed.left(2).toUpper() + trimmed.mid(2, 2);
  if (trimmed.size() == 6)
    return trimmed.left(2).toUpper() + trimmed.mid(2, 2) + trimmed.mid(4, 2).toLower();
  if (trimmed.size() == 8)
    return trimmed.left(2).toUpper() + trimmed.mid(2, 2) + trimmed.mid(4, 2).toLower()
           + trimmed.mid(6, 2);
  return trimmed;
}

std::optional<Profile> ProfileCodec::parse(const QByteArray &json,
                                           QList<ValidationError> *errors)
{
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    addError(errors, QStringLiteral("$"),
             QStringLiteral("invalid JSON: %1").arg(parseError.errorString()));
    return std::nullopt;
  }
  if (!document.isObject()) {
    addError(errors, QStringLiteral("$"), QStringLiteral("profile must be a JSON object"));
    return std::nullopt;
  }

  const QJsonObject root = document.object();
  QList<ValidationError> localErrors;
  if (!validateRoot(root, &localErrors) || !localErrors.isEmpty()) {
    if (errors)
      errors->append(localErrors);
    return std::nullopt;
  }

  Profile profile;
  profile.version = 1;

  if (root.contains(QStringLiteral("$schema")))
    profile.schemaUrl = root.value(QStringLiteral("$schema")).toString();

  if (root.contains(QStringLiteral("operator"))) {
    const QJsonObject object = root.value(QStringLiteral("operator")).toObject();
    Operator op;
    if (object.contains(QStringLiteral("callsign")))
      op.callsign = object.value(QStringLiteral("callsign")).toString().toUpper();
    if (object.contains(QStringLiteral("grid")))
      op.grid = normalizeGrid(object.value(QStringLiteral("grid")).toString());
    if (object.contains(QStringLiteral("gridSource")))
      op.gridSource = gridSourceFromString(object.value(QStringLiteral("gridSource")).toString());
    profile.operatorSection = op;
  }

  if (root.contains(QStringLiteral("station"))) {
    Station station;
    const QJsonArray paths =
        root.value(QStringLiteral("station")).toObject().value(QStringLiteral("paths")).toArray();
    for (const QJsonValue &path : paths)
      station.paths.append(path.toString());
    profile.station = station;
  }

  if (root.contains(QStringLiteral("audio"))) {
    const QJsonObject object = root.value(QStringLiteral("audio")).toObject();
    Audio audio;
    if (object.contains(QStringLiteral("rx"))) {
      AudioRoleBinding binding;
      binding.node = object.value(QStringLiteral("rx")).toObject().value(QStringLiteral("node")).toString();
      audio.rx = binding;
    }
    if (object.contains(QStringLiteral("tx"))) {
      AudioRoleBinding binding;
      binding.node = object.value(QStringLiteral("tx")).toObject().value(QStringLiteral("node")).toString();
      audio.tx = binding;
    }
    profile.audio = audio;
  }

  if (root.contains(QStringLiteral("tnc"))) {
    const QJsonObject object = root.value(QStringLiteral("tnc")).toObject();
    const QJsonObject transport = object.value(QStringLiteral("transport")).toObject();
    Tnc tnc;
    if (transport.value(QStringLiteral("kind")).toString() == QStringLiteral("serial")) {
      tnc.kind = TncKind::Serial;
      tnc.serialPath = transport.value(QStringLiteral("path")).toString();
    } else {
      tnc.kind = TncKind::Bluetooth;
      tnc.bluetoothAddress = transport.value(QStringLiteral("address")).toString();
    }
    if (object.contains(QStringLiteral("baud")))
      tnc.baud = object.value(QStringLiteral("baud")).toInt();
    profile.tnc = tnc;
  }

  if (root.contains(QStringLiteral("cat"))) {
    const QJsonObject object = root.value(QStringLiteral("cat")).toObject();
    Cat cat;
    cat.kind = object.value(QStringLiteral("kind")).toString();
    if (object.contains(QStringLiteral("host")))
      cat.host = object.value(QStringLiteral("host")).toString();
    if (object.contains(QStringLiteral("port")))
      cat.port = object.value(QStringLiteral("port")).toInt();
    profile.cat = cat;
  }

  return profile;
}

QByteArray ProfileCodec::serialize(const Profile &profile)
{
  return prettyJson(toJson(profile));
}

bool ProfileCodec::validate(const Profile &profile, QList<ValidationError> *errors)
{
  QList<ValidationError> localErrors;
  const auto parsed = parse(serialize(profile), &localErrors);
  if (errors)
    errors->append(localErrors);
  return parsed.has_value();
}

} // namespace omarchy::ham
