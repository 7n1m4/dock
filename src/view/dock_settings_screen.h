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

#ifndef CRYSTALDOCK_DOCK_SETTINGS_SCREEN_H_
#define CRYSTALDOCK_DOCK_SETTINGS_SCREEN_H_

#include <memory>

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFontComboBox>
#include <QGroupBox>
#include <QList>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>

#include "appearance_settings_dialog.h"
#include "dock_panel.h"
#include "edit_keyboard_layouts_dialog.h"
#include "edit_launchers_dialog.h"
#include "task_manager_settings_dialog.h"
#include "trash.h"
#include "wallpaper_settings_dialog.h"
#include <model/multi_dock_model.h>

class MultiDockView;

namespace Ui {
  class DockSettingsScreen;
}

namespace crystaldock {

// The dedicated settings window, replacing the right-click (context) menus.
// Reachable only via `crystal-dock --settings`.
class DockSettingsScreen : public QDialog {
  Q_OBJECT

 public:
  // No pointer ownership.
  DockSettingsScreen(QWidget* parent, MultiDockModel* model, int dockId,
                     DockPanel* panel, MultiDockView* view);
  ~DockSettingsScreen() override;

  void reload() { loadData(); }

  // Re-points the window at another panel, e.g. after the user picks a
  // different one in the panel selector.
  void setTargetPanel(int dockId, DockPanel* panel);

  int dockId() const { return dockId_; }
  DockPanel* panel() const { return targetPanel_; }

 public slots:
  void accept() override;
  void buttonClicked(QAbstractButton* button);

 private slots:
  // Re-points the window at whatever the panel selector now shows.
  void setTargetPanelFromSelector();

 private:
  // Points the panel selector at dockId_, refreshing it if the model gained
  // or lost panels since the last sync.
  void syncPanelSelector();

  // Data binding for the currently selected panel.
  void loadData();
  void resetData();
  void saveData();

  // Embeds the screen's own keyboard-layouts dialog, seeded from the target
  // panel's live KeyboardLayout item.
  void embedKeyboardLayoutsDialog();

  // Enables/disables the Empty Trash button per the target panel's trash.
  void updateTrashButton();

  void createLaunchersDialog();

  void createPanelPage();
  void createLaunchersPage();
  void createOptionalFeaturesPage();
  void createAppearancePage();
  void createComponentsPage();
  void createHelpPage();

  Ui::DockSettingsScreen *ui;

  MultiDockModel* model_;  // No ownership.
  MultiDockView* view_;  // No ownership.
  int dockId_;
  DockPanel* targetPanel_;  // No ownership.

  QComboBox* panelSelector_;

  QListWidget* pageList_;
  QStackedWidget* pages_;

  // Embedded (existing) settings dialogs, one per page.
  AppearanceSettingsDialog* embeddedAppearanceDialog_ = nullptr;
  EditLaunchersDialog* embeddedLaunchersDialog_ = nullptr;
  TaskManagerSettingsDialog* embeddedTaskManagerDialog_ = nullptr;
  EditKeyboardLayoutsDialog* embeddedKeyboardLayoutsDialog_ = nullptr;
  WallpaperSettingsDialog* embeddedWallpaperDialog_ = nullptr;

  // Per-panel controls on the Panel page.
  QComboBox* positionSelector_;
  QComboBox* screenSelector_;
  QComboBox* visibilitySelector_;
  QComboBox* styleSelector_;

  // Optional Features page: the 9 per-dock component toggles, in the same
  // order as the model keys they write.
  QList<QCheckBox*> featureToggles_;

  // Components page.
  QComboBox* volumeScrollStepSelector_;
  QComboBox* versionCheckFrequencySelector_;

  // Clock settings on the Components page (written straight to the model).
  QFontComboBox* clockFontFamilySelector_;
  QSpinBox* clockFontSizeSelector_;
  QCheckBox* clock24HourClock_;

  // Controls that report on the target panel's live items.
  QPushButton* emptyTrashButton_;

  // The target panel's Trash, whose trashStateChanged drives the button.
  Trash* watchedTrash_ = nullptr;

  // Holds the (re-created) keyboard-layouts dialog for the selected panel.
  QGroupBox* keyboardLayoutGroup_;
};

}  // namespace crystaldock

#endif  // CRYSTALDOCK_DOCK_SETTINGS_SCREEN_H_
