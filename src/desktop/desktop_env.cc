/*
 * This file is part of Crystal Dock.
 * Copyright (C) 2022 Viet Dang (dangvd@gmail.com)
 *
 * Crystal Dock is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Crystal Dock is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Crystal Dock.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "desktop_env.h"

#include <memory>

#include <QtGlobal>
#include <QProcess>
#include <QStringList>

#include <model/application_menu_config.h>
#include <model/multi_dock_model.h>

namespace crystaldock {

DesktopEnv* DesktopEnv::getDesktopEnv() {
  static std::unique_ptr<DesktopEnv> generic(new DesktopEnv);
  return generic.get();
}

QString DesktopEnv::getDesktopEnvName() {
  QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(",", Qt::SkipEmptyParts);
  // The format is usually something like 'KDE' or 'labwc:wlroots'.
  constexpr char kGenericDesktop[] = "generic";
  return desktops.isEmpty() ? kGenericDesktop
                            : desktops.first().mid(0, desktops.first().indexOf(':'));
}

std::vector<QString> DesktopEnv::getDefaultLaunchers() const {
  return { defaultWebBrowser() };
}

QString DesktopEnv::defaultWebBrowser() const {
  QProcess process;
  process.start("xdg-settings", {"get", "default-web-browser"});
  process.waitForFinished(1000 /*msecs*/);
  QString desktopFile = process.readAllStandardOutput().trimmed();
  // lastIndexOf() returns -1 when xdg-settings is missing/failed (empty output)
  // or produced no '.', and QString::first(-1) aborts, so guard the index first.
  const int extensionPos = desktopFile.lastIndexOf('.');
  if (extensionPos <= 0) {
    return "firefox";
  }
  auto appId = desktopFile.first(extensionPos);
  return !appId.isEmpty() ? appId : "firefox";
}

}  // namespace crystaldock
