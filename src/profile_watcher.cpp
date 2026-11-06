#include "omarchy_ham/profile_watcher.h"

#include <QDir>
#include <QFileInfo>

#include <utility>

namespace omarchy::ham {

ProfileWatcher::ProfileWatcher(QString path, QObject *parent)
    : QObject(parent)
    , m_store(std::move(path))
{
  m_debounce.setSingleShot(true);
  m_debounce.setInterval(50);
  connect(&m_debounce, &QTimer::timeout, this, &ProfileWatcher::reload);
  connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
          &ProfileWatcher::onDirectoryChanged);
  qRegisterMetaType<omarchy::ham::Profile>("omarchy::ham::Profile");
  qRegisterMetaType<QList<omarchy::ham::ValidationError>>(
      "QList<omarchy::ham::ValidationError>");
}

QString ProfileWatcher::path() const
{
  return m_store.path();
}

void ProfileWatcher::start()
{
  if (m_started)
    return;
  m_started = true;

  const QString directory = QFileInfo(m_store.path()).absolutePath();
  QDir().mkpath(directory);
  if (!m_watcher.directories().contains(directory))
    m_watcher.addPath(directory);

  reload();
}

void ProfileWatcher::stop()
{
  if (!m_started)
    return;
  m_started = false;
  m_debounce.stop();
  const QString directory = QFileInfo(m_store.path()).absolutePath();
  if (m_watcher.directories().contains(directory))
    m_watcher.removePath(directory);
}

void ProfileWatcher::onDirectoryChanged(const QString &path)
{
  Q_UNUSED(path)
  if (!m_started)
    return;

  const QString directory = QFileInfo(m_store.path()).absolutePath();
  if (!m_watcher.directories().contains(directory))
    m_watcher.addPath(directory);

  m_debounce.start();
}

void ProfileWatcher::reload()
{
  const LoadResult result = m_store.load();
  if (!result.found) {
    emit missing();
    return;
  }
  if (!result.profile) {
    emit invalid(result.errors);
    return;
  }
  emit changed(*result.profile);
}

} // namespace omarchy::ham
