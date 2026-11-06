#pragma once

#include "omarchy_ham/profile.h"
#include "omarchy_ham/profile_store.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

namespace omarchy::ham {

class ProfileWatcher : public QObject {
  Q_OBJECT

public:
  explicit ProfileWatcher(QString path = {}, QObject *parent = nullptr);

  QString path() const;
  void start();
  void stop();

signals:
  void changed(const omarchy::ham::Profile &profile);
  void missing();
  void invalid(const QList<omarchy::ham::ValidationError> &errors);

private slots:
  void onDirectoryChanged(const QString &path);
  void reload();

private:
  ProfileStore m_store;
  QFileSystemWatcher m_watcher;
  QTimer m_debounce;
  bool m_started = false;
};

} // namespace omarchy::ham
