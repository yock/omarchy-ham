#include "omarchy_ham/profile_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace omarchy::ham {
namespace {

void addError(QList<ValidationError> *errors, const QString &path, const QString &message)
{
  if (errors)
    errors->append({path, message});
}

QString systemError()
{
  return QString::fromLocal8Bit(std::strerror(errno));
}

class FileLock {
public:
  explicit FileLock(const QString &path)
  {
    m_fd = ::open(QFile::encodeName(path).constData(), O_CREAT | O_RDWR | O_CLOEXEC, 0644);
    if (m_fd >= 0 && ::flock(m_fd, LOCK_EX) != 0) {
      const int saved = errno;
      ::close(m_fd);
      m_fd = -1;
      errno = saved;
    }
  }

  ~FileLock()
  {
    if (m_fd >= 0) {
      ::flock(m_fd, LOCK_UN);
      ::close(m_fd);
    }
  }

  FileLock(const FileLock &) = delete;
  FileLock &operator=(const FileLock &) = delete;

  bool ok() const { return m_fd >= 0; }

private:
  int m_fd = -1;
};

bool fsyncDirectory(const QString &path, QString *error)
{
  const int fd = ::open(QFile::encodeName(path).constData(),
                        O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0) {
    if (error)
      *error = systemError();
    return false;
  }
  const bool ok = ::fsync(fd) == 0;
  if (!ok && error)
    *error = systemError();
  ::close(fd);
  return ok;
}

} // namespace

ProfileStore::ProfileStore(QString path)
    : m_path(path.isEmpty() ? defaultPath() : std::move(path))
{
}

QString ProfileStore::path() const
{
  return m_path;
}

bool ProfileStore::exists() const
{
  return QFileInfo::exists(m_path);
}

LoadResult ProfileStore::load() const
{
  LoadResult result;
  QFile file(m_path);
  if (!file.exists())
    return result;

  result.found = true;
  if (!file.open(QIODevice::ReadOnly)) {
    addError(&result.errors, QStringLiteral("$"),
             QStringLiteral("cannot read %1: %2").arg(m_path, file.errorString()));
    return result;
  }

  result.profile = ProfileCodec::parse(file.readAll(), &result.errors);
  return result;
}

bool ProfileStore::save(const Profile &profile, QList<ValidationError> *errors) const
{
  const QString directory = QFileInfo(m_path).absolutePath();
  if (!QDir().mkpath(directory)) {
    addError(errors, QStringLiteral("$"),
             QStringLiteral("cannot create directory %1").arg(directory));
    return false;
  }

  FileLock lock(m_path + QStringLiteral(".lock"));
  if (!lock.ok()) {
    addError(errors, QStringLiteral("$"),
             QStringLiteral("cannot lock %1.lock: %2").arg(m_path, systemError()));
    return false;
  }

  return saveLocked(profile, errors);
}

bool ProfileStore::update(const std::function<bool(Profile &)> &mutate,
                          QList<ValidationError> *errors) const
{
  const QString directory = QFileInfo(m_path).absolutePath();
  if (!QDir().mkpath(directory)) {
    addError(errors, QStringLiteral("$"),
             QStringLiteral("cannot create directory %1").arg(directory));
    return false;
  }

  FileLock lock(m_path + QStringLiteral(".lock"));
  if (!lock.ok()) {
    addError(errors, QStringLiteral("$"),
             QStringLiteral("cannot lock %1.lock: %2").arg(m_path, systemError()));
    return false;
  }

  const LoadResult current = load();
  Profile profile;
  if (current.found) {
    if (!current.profile) {
      if (errors)
        errors->append(current.errors);
      return false;
    }
    profile = *current.profile;
  }

  if (!mutate(profile))
    return false;

  return saveLocked(profile, errors);
}

bool ProfileStore::saveLocked(const Profile &profile, QList<ValidationError> *errors) const
{
  const auto parsed = ProfileCodec::parse(ProfileCodec::serialize(profile), errors);
  if (!parsed)
    return false;

  const QByteArray data = ProfileCodec::serialize(*parsed);
  const QString directory = QFileInfo(m_path).absolutePath();

  QTemporaryFile temporary(directory + QStringLiteral("/.station.json.XXXXXX"));
  temporary.setAutoRemove(true);
  if (!temporary.open()) {
    addError(errors, QStringLiteral("$"),
             QStringLiteral("cannot create a temporary profile: %1")
                 .arg(temporary.errorString()));
    return false;
  }

  if (temporary.write(data) != data.size()) {
    addError(errors, QStringLiteral("$"),
             QStringLiteral("cannot write the temporary profile: %1")
                 .arg(temporary.errorString()));
    return false;
  }

  if (!temporary.flush()) {
    addError(errors, QStringLiteral("$"),
             QStringLiteral("cannot flush the temporary profile: %1")
                 .arg(temporary.errorString()));
    return false;
  }

  if (::fsync(temporary.handle()) != 0) {
    addError(errors, QStringLiteral("$"),
             QStringLiteral("cannot fsync the temporary profile: %1").arg(systemError()));
    return false;
  }

  const QString temporaryPath = temporary.fileName();
  temporary.close();

  if (::chmod(QFile::encodeName(temporaryPath).constData(), 0644) != 0) {
    addError(errors, QStringLiteral("$"),
             QStringLiteral("cannot set the profile mode: %1").arg(systemError()));
    return false;
  }

  if (::rename(QFile::encodeName(temporaryPath).constData(),
               QFile::encodeName(m_path).constData())
      != 0) {
    addError(errors, QStringLiteral("$"),
             QStringLiteral("cannot replace %1: %2").arg(m_path, systemError()));
    return false;
  }

  QString directoryError;
  if (!fsyncDirectory(directory, &directoryError)) {
    addError(errors, QStringLiteral("$"),
             QStringLiteral("cannot fsync %1: %2").arg(directory, directoryError));
    return false;
  }

  return true;
}

QString ProfileStore::defaultPath()
{
  QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
  if (base.isEmpty())
    base = QDir::homePath() + QStringLiteral("/.config");
  return base + QStringLiteral("/omarchy/ham/station.json");
}

} // namespace omarchy::ham
