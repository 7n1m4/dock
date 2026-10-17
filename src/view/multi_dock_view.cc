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

#include "multi_dock_view.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QMessageBox>

#include "display/window_system.h"

#include "add_panel_dialog.h"

namespace crystaldock {

MultiDockView::MultiDockView(MultiDockModel* model, bool openSettings)
    : model_(model),
      desktopEnv_(DesktopEnv::getDesktopEnv()),
      openSettings_(openSettings) {
  connect(model_, SIGNAL(dockAdded(int)), this, SLOT(onDockAdded(int)));
  connect(model_, SIGNAL(wallpaperChanged(int)), this,
          SLOT(setWallpaper(int)));
  connect(WindowSystem::self(), SIGNAL(currentDesktopChanged(std::string_view)),
          this, SLOT(setWallpaper()));
  loadData();
}

/* static */ bool MultiDockView::checkPlatformSupported(const QApplication& app) {
  if (QGuiApplication::platformName().toLower() != "wayland") {
    QMessageBox::critical(nullptr, "Unsupported Platform",
                          "Crystal Dock 2.x only supports Wayland.\n"
                          "For X11, please use Crystal Dock 1.x");
    return false;
  }

  QNativeInterface::QWaylandApplication* waylandApp =
      app.nativeInterface<QNativeInterface::QWaylandApplication>();
  if (!waylandApp) {
    return false;
  }

  return WindowSystem::init(waylandApp->display());
}

void MultiDockView::show() {
  for (const auto& dock : docks_) {
    dock.second->show();
  }
  setWallpaper();
  if (openSettings_ && !model_->dockIds().empty()) {
    showSettingsScreen(model_->dockIds().front());
  }
}

DockPanel* MultiDockView::panelFor(int dockId) const {
  if (auto it = docks_.find(dockId); it != docks_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void MultiDockView::showSettingsScreen(int dockId) {
  if (!settingsScreen_) {
    settingsScreen_ = std::make_unique<DockSettingsScreen>(
        nullptr, model_, dockId, panelFor(dockId), this);
  } else {
    settingsScreen_->setTargetPanel(dockId, panelFor(dockId));
  }
  settingsScreen_->reload();
  settingsScreen_->show();
}

void MultiDockView::exit() {
  for (const auto& dock : docks_) {
    dock.second->close();
  }
}

void MultiDockView::onDockAdded(int dockId) {
  // A dock id can be reused, e.g. after a panel is cloned onto another screen
  // and the original is deleted later. Delete any existing panel for this id
  // first so that its unique_ptr does not overwrite ours while its own
  // deleteLater() is still pending.
  if (auto it = docks_.find(dockId); it != docks_.end() && it->second) {
    QObject* panel = it->second.release();
    panel->deleteLater();
  }
  docks_[dockId] = std::make_unique<DockPanel>(this, model_, dockId);
  docks_[dockId]->show();
}

bool MultiDockView::setWallpaper() {
  if (!model_->hasPager()) {
    return false;
  }

  for (unsigned int screen = 0; screen < WindowSystem::screens().size(); ++screen) {
    if (!setWallpaper(screen)) {
      return false;
    }
  }

  return true;
}

bool MultiDockView::setWallpaper(int screen) {
  if (!model_->hasPager()) {
    return false;
  }

  QString wallpaper = model_->wallpaper(WindowSystem::currentDesktop(), screen);
  if (wallpaper.isEmpty()) {
    return false;  // nothing to do here.
  }

  if (!QFile::exists(wallpaper)) {
    QMessageBox warning(QMessageBox::Warning, "Error",
                        QString("Failed to load wallpaper from: ") + wallpaper,
                        QMessageBox::Ok, nullptr, Qt::Tool);
    warning.exec();
    return false;
  }

  return desktopEnv_->setWallpaper(screen, wallpaper);
}

void MultiDockView::loadData() {
  // Same reasoning as onDockAdded(): destroy the panels through Qt, not through
  // the unique_ptrs, so any pending deleteLater() survives.
  for (auto& [id, panel] : docks_) {
    if (panel) {
      QObject* panelObject = panel.release();
      panelObject->deleteLater();
    }
  }
  docks_.clear();

  // Dock IDs start from 1, but the model may skip IDs, e.g. when a panel was
  // removed. Look them up instead of assuming they are contiguous.
  for (int dockId : model_->dockIds()) {
    docks_[dockId] = std::make_unique<DockPanel>(this, model_, dockId);
  }

  if (docks_.empty()) {
    AddPanelDialog dialog(nullptr, model_, 0 /* dockId not needed */);
    dialog.setMode(AddPanelDialog::Mode::Welcome);
    dialog.exec();
  }
}

}  // namespace crystaldock
