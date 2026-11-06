#pragma once

#include "omarchy_ham/profile.h"

#include <QString>

#include <functional>
#include <optional>

namespace omarchy::ham {

struct LoadResult {
  bool found = false;
  std::optional<Profile> profile;
  QList<ValidationError> errors;

  bool ok() const { return !found || profile.has_value(); }
};

class ProfileStore {
public:
  explicit ProfileStore(QString path = {});

  QString path() const;
  bool exists() const;
  LoadResult load() const;

  bool save(const Profile &profile, QList<ValidationError> *errors = nullptr) const;
  bool update(const std::function<bool(Profile &)> &mutate,
              QList<ValidationError> *errors = nullptr) const;

  static QString defaultPath();

private:
  bool saveLocked(const Profile &profile, QList<ValidationError> *errors) const;

  QString m_path;
};

} // namespace omarchy::ham
