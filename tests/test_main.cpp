#include "omarchy_ham/gps.h"
#include "omarchy_ham/maidenhead.h"
#include "omarchy_ham/pipewire.h"
#include "omarchy_ham/profile.h"
#include "omarchy_ham/profile_store.h"
#include "omarchy_ham/profile_watcher.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>

#include <cmath>
#include <cstdio>

using namespace omarchy::ham;

namespace {

int g_failures = 0;

void expect(bool condition, const char *message)
{
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++g_failures;
  }
}

void expectEqual(const QString &actual, const QString &expected, const char *message)
{
  if (actual != expected) {
    std::fprintf(stderr, "FAIL: %s (expected '%s', got '%s')\n", message,
                 qPrintable(expected), qPrintable(actual));
    ++g_failures;
  }
}

void expectNear(double actual, double expected, const char *message)
{
  if (std::fabs(actual - expected) > 1e-6) {
    std::fprintf(stderr, "FAIL: %s (expected %f, got %f)\n", message, expected, actual);
    ++g_failures;
  }
}

void testRoundTrip()
{
  Profile profile;
  profile.schemaUrl = QStringLiteral("https://example.invalid/station.schema.json");
  profile.operatorSection = Operator{};
  profile.operatorSection->callsign = QStringLiteral("n0call");
  profile.operatorSection->grid = QStringLiteral("fn31pr");
  profile.operatorSection->gridSource = GridSource::Gps;
  profile.station = Station{QStringList{QStringLiteral("host-audio"), QStringLiteral("tnc")}};
  Audio audio;
  audio.rx = AudioRoleBinding{QStringLiteral("alsa_input.foo")};
  audio.tx = AudioRoleBinding{QStringLiteral("alsa_output.bar")};
  profile.audio = audio;
  Tnc tnc;
  tnc.kind = TncKind::Serial;
  tnc.serialPath = QStringLiteral("/dev/serial/by-id/modem");
  tnc.baud = 38400;
  profile.tnc = tnc;
  Cat cat;
  cat.kind = QStringLiteral("rigctld");
  cat.host = QStringLiteral("localhost");
  cat.port = 4532;
  profile.cat = cat;

  QList<ValidationError> errors;
  const QByteArray encoded = ProfileCodec::serialize(profile);
  const auto parsed = ProfileCodec::parse(encoded, &errors);

  expect(errors.isEmpty(), "round trip has no validation errors");
  expect(parsed.has_value(), "round trip parses");
  if (!parsed)
    return;

  expectEqual(parsed->operatorSection->callsign.value_or(QString()),
              QStringLiteral("N0CALL"), "callsign is normalised");
  expectEqual(parsed->operatorSection->grid.value_or(QString()),
              QStringLiteral("FN31pr"), "grid is normalised");
  expect(parsed->operatorSection->effectiveGridSource() == GridSource::Gps,
         "grid source round trips");
  expect(parsed->station->paths ==
             QStringList{QStringLiteral("host-audio"), QStringLiteral("tnc")},
         "station paths round trip");
  expect(parsed->audio->rx->node == QStringLiteral("alsa_input.foo"), "rx node round trips");
  expect(parsed->audio->tx->node == QStringLiteral("alsa_output.bar"), "tx node round trips");
  expect(parsed->tnc->kind == TncKind::Serial, "tnc kind round trips");
  expectEqual(parsed->tnc->serialPath, QStringLiteral("/dev/serial/by-id/modem"),
              "tnc path round trips");
  expect(parsed->tnc->baud.value_or(0) == 38400, "tnc baud round trips");
  expectEqual(parsed->cat->effectiveHost(), QStringLiteral("localhost"), "cat host round trips");
  expect(parsed->cat->effectivePort() == 4532, "cat port round trips");
  expect(encoded.contains("\n  \"version\""), "serializer uses two-space indentation");
}

void testDefaults()
{
  QList<ValidationError> errors;
  const auto parsed = ProfileCodec::parse(
      QByteArrayLiteral("{\"version\":1,\"cat\":{\"kind\":\"rigctld\"}}"), &errors);
  expect(parsed.has_value(), "minimal cat profile parses");
  if (!parsed)
    return;
  expectEqual(parsed->cat->effectiveHost(), QStringLiteral("localhost"),
              "cat host defaults to localhost");
  expect(parsed->cat->effectivePort() == 4532, "cat port defaults to 4532");
  const GridSource source = parsed->operatorSection
                                ? parsed->operatorSection->effectiveGridSource()
                                : GridSource::Manual;
  expect(source == GridSource::Manual, "grid source defaults to manual");
}

void testInvalidProfiles()
{
  const QList<QByteArray> invalid = {
      QByteArrayLiteral("{}"),
      QByteArrayLiteral("{\"version\":2}"),
      QByteArrayLiteral("{\"version\":1,\"unknown\":true}"),
      QByteArrayLiteral("{\"version\":1,\"operator\":{\"callsign\":\"x\"}}"),
      QByteArrayLiteral("{\"version\":1,\"operator\":{\"callsign\":\"N0CALL\",\"grid\":\"ZZ99zz\"}}"),
      QByteArrayLiteral("{\"version\":1,\"operator\":{\"gridSource\":\"auto\"}}"),
      QByteArrayLiteral("{\"version\":1,\"station\":{\"paths\":[]}}"),
      QByteArrayLiteral("{\"version\":1,\"station\":{\"paths\":[\"host-audio\",\"host-audio\"]}}"),
      QByteArrayLiteral("{\"version\":1,\"station\":{\"paths\":[\"both\"]}}"),
      QByteArrayLiteral("{\"version\":1,\"audio\":{\"rx\":{}}}"),
      QByteArrayLiteral("{\"version\":1,\"audio\":{\"rx\":{\"node\":\"\"}}}"),
      QByteArrayLiteral("{\"version\":1,\"tnc\":{\"transport\":{\"kind\":\"serial\",\"path\":\"tty\"}}}"),
      QByteArrayLiteral("{\"version\":1,\"tnc\":{\"transport\":{\"kind\":\"serial\",\"path\":\"/dev/tty\",\"address\":\"AA:BB:CC:DD:EE:FF\"}}}"),
      QByteArrayLiteral("{\"version\":1,\"tnc\":{\"transport\":{\"kind\":\"bluetooth\",\"address\":\"nope\"}}}"),
      QByteArrayLiteral("{\"version\":1,\"tnc\":{\"transport\":{\"kind\":\"serial\",\"path\":\"/dev/tty\"},\"baud\":0}}"),
      QByteArrayLiteral("{\"version\":1,\"cat\":{\"kind\":\"flrig\"}}"),
      QByteArrayLiteral("{\"version\":1,\"cat\":{\"kind\":\"rigctld\",\"port\":70000}}"),
  };

  for (const QByteArray &json : invalid) {
    QList<ValidationError> errors;
    const auto parsed = ProfileCodec::parse(json, &errors);
    expect(!parsed.has_value(), "invalid profile is rejected");
    expect(!errors.isEmpty(), "invalid profile reports an error");
  }
}

void testGridAndMaidenhead()
{
  expectEqual(normalizeGrid(QStringLiteral("fn31PR")), QStringLiteral("FN31pr"),
              "6-character grid normalisation");
  expectEqual(normalizeGrid(QStringLiteral("fn31")), QStringLiteral("FN31"),
              "4-character grid normalisation");
  expectEqual(normalizeGrid(QStringLiteral("fn31pr12")), QStringLiteral("FN31pr12"),
              "8-character grid normalisation");

  expectEqual(maidenhead(41.714, -72.727, 6), QStringLiteral("FN31pr"),
              "Hartford grid");
  expectEqual(maidenhead(0.0, 0.0, 6), QStringLiteral("JJ00aa"), "equator grid");
  expectEqual(maidenhead(51.5, -0.1, 6), QStringLiteral("IO91wm"), "London grid");
  expect(maidenhead(91.0, 0.0, 6).isEmpty(), "invalid latitude is rejected");
  expect(maidenhead(0.0, 0.0, 5).isEmpty(), "invalid grid length is rejected");
}

void testStore()
{
  QTemporaryDir directory;
  expect(directory.isValid(), "temporary directory exists");

  const QString path = directory.filePath(QStringLiteral("nested/station.json"));
  ProfileStore store(path);

  Profile profile;
  profile.operatorSection = Operator{};
  profile.operatorSection->callsign = QStringLiteral("K1ABC");
  profile.station = Station{QStringList{QStringLiteral("host-audio")}};
  profile.audio = Audio{};
  profile.audio->rx = AudioRoleBinding{QStringLiteral("alsa_input.foo")};

  QList<ValidationError> errors;
  expect(store.save(profile, &errors), "profile saves");
  expect(errors.isEmpty(), "save reports no errors");
  expect(QFileInfo::exists(path), "profile file exists");

  const LoadResult loaded = store.load();
  expect(loaded.found, "saved profile is found");
  expect(loaded.profile.has_value(), "saved profile parses");
  if (loaded.profile)
    expectEqual(loaded.profile->operatorSection->callsign.value_or(QString()),
                QStringLiteral("K1ABC"), "saved callsign loads");

  expect(store.update(
             [](Profile &current) {
               current.operatorSection->callsign = QStringLiteral("W1AW");
               return true;
             },
             &errors),
         "profile update succeeds");
  const LoadResult updated = store.load();
  if (updated.profile)
    expectEqual(updated.profile->operatorSection->callsign.value_or(QString()),
                QStringLiteral("W1AW"), "updated callsign loads");

  QFile file(path);
  expect(file.open(QIODevice::ReadOnly), "profile can be read");
  const QByteArray before = file.readAll();
  file.close();

  errors.clear();
  expect(!store.update(
             [](Profile &current) {
               current.operatorSection->callsign = QStringLiteral("x");
               return true;
             },
             &errors),
         "invalid update is refused");
  expect(!errors.isEmpty(), "invalid update reports an error");

  expect(file.open(QIODevice::ReadOnly), "profile can be re-read");
  const QByteArray after = file.readAll();
  file.close();
  expect(before == after, "invalid update does not modify the profile");
}

void testWatcher()
{
  QTemporaryDir directory;
  expect(directory.isValid(), "watcher temporary directory exists");

  ProfileStore store(directory.filePath(QStringLiteral("watch/station.json")));
  ProfileWatcher watcher(store.path());

  bool changed = false;
  QEventLoop loop;
  QObject::connect(&watcher, &ProfileWatcher::changed, [&](const Profile &) {
    changed = true;
    loop.quit();
  });
  QTimer::singleShot(3000, &loop, &QEventLoop::quit);

  watcher.start();

  Profile profile;
  profile.operatorSection = Operator{};
  profile.operatorSection->callsign = QStringLiteral("K1ABC");
  QList<ValidationError> errors;
  expect(store.save(profile, &errors), "watcher profile saves");

  loop.exec();
  watcher.stop();
  expect(changed, "watcher reports a changed profile");
}

void testPipeWireParse()
{
  const QByteArray dump = QByteArrayLiteral(R"([
    {"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_input.foo","media.class":"Audio/Source"}}},
    {"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_output.bar","media.class":"Audio/Sink"}}},
    {"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"duplex","media.class":"Audio/Duplex"}}},
    {"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"stream","media.class":"Stream/Output/Audio"}}}
  ])");

  QString error;
  const QStringList sources = PipeWire::parseDump(dump, AudioDirection::Rx, &error);
  expect(error.isEmpty(), "pw-dump parse has no error");
  expect(sources.contains(QStringLiteral("alsa_input.foo")), "rx source is found");
  expect(sources.contains(QStringLiteral("duplex")), "duplex is a rx source");
  expect(!sources.contains(QStringLiteral("alsa_output.bar")), "sink is not a rx source");
  expect(!sources.contains(QStringLiteral("stream")), "streams are excluded");

  const QStringList sinks = PipeWire::parseDump(dump, AudioDirection::Tx, &error);
  expect(sinks.contains(QStringLiteral("alsa_output.bar")), "tx sink is found");
  expect(sinks.contains(QStringLiteral("duplex")), "duplex is a tx sink");
  expect(!sinks.contains(QStringLiteral("alsa_input.foo")), "source is not a tx sink");
}

void testGpsParse()
{
  const QByteArray output = QByteArrayLiteral(
      "{\"class\":\"TPV\",\"mode\":1}\n"
      "{\"class\":\"TPV\",\"mode\":2,\"lat\":41.714,\"lon\":-72.727}\n");

  QString error;
  const auto fix = Gps::parseGpspipe(output, &error);
  expect(fix.has_value(), "gps TPV fix is found");
  if (fix) {
    expectNear(fix->latitude, 41.714, "gps latitude");
    expectNear(fix->longitude, -72.727, "gps longitude");
  }

  error.clear();
  const auto missing =
      Gps::parseGpspipe(QByteArrayLiteral("{\"class\":\"TPV\",\"mode\":1}\n"), &error);
  expect(!missing.has_value(), "gps without a fix is rejected");
  expect(!error.isEmpty(), "gps without a fix reports an error");
}

} // namespace

int main(int argc, char **argv)
{
  QCoreApplication application(argc, argv);

  testRoundTrip();
  testDefaults();
  testInvalidProfiles();
  testGridAndMaidenhead();
  testStore();
  testWatcher();
  testPipeWireParse();
  testGpsParse();

  if (g_failures == 0) {
    std::printf("all tests passed\n");
    return 0;
  }

  std::fprintf(stderr, "%d test(s) failed\n", g_failures);
  return 1;
}
