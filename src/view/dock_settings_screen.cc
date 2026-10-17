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

#include "dock_settings_screen.h"
#include "ui_dock_settings_screen.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFontComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <display/window_system.h>

#include "multi_dock_view.h"

namespace crystaldock {

namespace {

// The per-dock component toggles, in the same order as the model keys they
// write and as DockPanel::setFeatureVisible's switch cases.
const std::vector<std::pair<const char*, const char*>> kFeatureRows = {
    {"Task Manager", "showTaskManager"},
};

}  // namespace

DockSettingsScreen::DockSettingsScreen(QWidget* parent, MultiDockModel* model,
                                       int dockId, DockPanel* panel,
                                       MultiDockView* view)
    : QDialog(parent),
      ui(new Ui::DockSettingsScreen),
      model_(model),
      view_(view),
      dockId_(dockId),
      targetPanel_(panel) {
  ui->setupUi(this);
  setWindowFlag(Qt::Tool);

  panelSelector_ = ui->panelSelector;
  for (const int id : model_->dockIds()) {
    panelSelector_->addItem("Panel " + QString::number(id),
                            QVariant::fromValue<int>(id));
  }
  syncPanelSelector();
  connect(panelSelector_,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &DockSettingsScreen::setTargetPanelFromSelector);

  pageList_ = ui->pageList_;
  pages_ = ui->pages_;
  const QStringList pageNames = {tr("Panel"), tr("Launchers"),
                                 tr("Optional Features"),
                                 tr("Appearance & Style"), tr("Components"),
                                 tr("Help")};
  for (const auto& name : pageNames) {
    pageList_->addItem(name);
  }
  pageList_->setCurrentRow(0);
  connect(pageList_, &QListWidget::currentRowChanged, pages_,
          &QStackedWidget::setCurrentIndex);

  createPanelPage();
  createLaunchersPage();
  createOptionalFeaturesPage();
  createAppearancePage();
  createComponentsPage();
  createHelpPage();
  createLaunchersDialog();

  connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton*)),
          this, SLOT(buttonClicked(QAbstractButton*)));

  loadData();
}

DockSettingsScreen::~DockSettingsScreen() {
  delete ui;
}

void DockSettingsScreen::syncPanelSelector() {
  const int row = panelSelector_->findData(QVariant::fromValue<int>(dockId_));
  panelSelector_->setCurrentIndex(row >= 0 ? row : 0);
  dockId_ = panelSelector_->itemData(panelSelector_->currentIndex())
                .value<int>();
}

void DockSettingsScreen::setTargetPanel(int dockId, DockPanel* panel) {
  dockId_ = dockId;
  targetPanel_ = panel;
  syncPanelSelector();
  // The launchers dialog binds its dock id in the ctor, so it must be rebuilt.
  delete embeddedLaunchersDialog_;
  embeddedLaunchersDialog_ = nullptr;
  createLaunchersDialog();
  loadData();
}

void DockSettingsScreen::setTargetPanelFromSelector() {
  dockId_ = panelSelector_->itemData(panelSelector_->currentIndex())
                .value<int>();
  targetPanel_ = view_->panelFor(dockId_);
  if (embeddedLaunchersDialog_) {
    delete embeddedLaunchersDialog_;
    embeddedLaunchersDialog_ = nullptr;
    createLaunchersDialog();
  }
  loadData();
}

void DockSettingsScreen::accept() {
  saveData();
  QDialog::accept();
}

void DockSettingsScreen::buttonClicked(QAbstractButton* button) {
  const auto role = ui->buttonBox->buttonRole(button);
  if (role == QDialogButtonBox::ApplyRole) {
    saveData();
  } else if (role == QDialogButtonBox::ResetRole) {
    resetData();
  }
}

void DockSettingsScreen::loadData() {
  if (dockId_ <= 0) return;

  positionSelector_->setCurrentIndex(
      static_cast<int>(model_->panelPosition(dockId_)));
  const int screen = model_->screen(dockId_);
  screenSelector_->setCurrentIndex(
      screen < screenSelector_->count() ? screen : 0);
  visibilitySelector_->setCurrentIndex(
      static_cast<int>(model_->visibility(dockId_)));

  const std::vector<bool (MultiDockModel::*)(int) const> featureGetters = {
      &MultiDockModel::showTaskManager,
  };
  for (size_t i = 0; i < featureToggles_.size(); ++i) {
    featureToggles_[i]->setChecked((model_->*featureGetters[i])(dockId_));
  }

  volumeScrollStepSelector_->setCurrentIndex(
      volumeScrollStepSelector_->findData(model_->volumeScrollStep()));
  clockFontFamilySelector_->setCurrentFont(QFont(model_->clockFontFamily()));
  clockFontSizeSelector_->setValue(
      static_cast<int>(model_->clockFontScaleFactor() * 100.0));
  clock24HourClock_->setChecked(model_->use24HourClock());

  updateTrashButton();

  if (embeddedAppearanceDialog_) embeddedAppearanceDialog_->reload();
  if (embeddedLaunchersDialog_) embeddedLaunchersDialog_->reload();
  if (embeddedTaskManagerDialog_) embeddedTaskManagerDialog_->reload();
}


void DockSettingsScreen::updateTrashButton() {
}

void DockSettingsScreen::resetData() {
  loadData();
}

void DockSettingsScreen::saveData() {
  if (dockId_ <= 0) return;

  if (targetPanel_) {
    targetPanel_->setPanelPosition(positionSelector_->currentIndex());
    targetPanel_->setVisibility(visibilitySelector_->currentIndex());
    // The combo may be stale after a monitor is unplugged.
    if (screenSelector_->currentIndex() <
        static_cast<int>(WindowSystem::screens().size())) {
      targetPanel_->setPanelScreen(screenSelector_->currentIndex());
    }

    for (size_t i = 0; i < featureToggles_.size(); ++i) {
      targetPanel_->setFeatureVisible(static_cast<int>(i),
                                      featureToggles_[i]->isChecked());
    }
  } else {
    // The panel is gone (e.g. removed through this window); only the model
    // side of the toggles can still be written.
    const std::vector<void (MultiDockModel::*)(int, bool)> modelSetters = {
        &MultiDockModel::setShowTaskManager, &MultiDockModel::setShowTrash,
        &MultiDockModel::setShowWifiManager,
        &MultiDockModel::setShowVolumeControl,
        &MultiDockModel::setShowBatteryIndicator,
        &MultiDockModel::setShowKeyboardLayout,
        &MultiDockModel::setShowVersionChecker, &MultiDockModel::setShowClock,
    };
    for (size_t i = 0; i < featureToggles_.size(); ++i) {
      (model_->*modelSetters[i])(dockId_, featureToggles_[i]->isChecked());
    }
    model_->saveDockConfig(dockId_);
  }

  model_->setClockFontFamily(clockFontFamilySelector_->currentFont().family());
  model_->setClockFontScaleFactor(
      clockFontSizeSelector_->value() / 100.0);
  model_->setUse24HourClock(clock24HourClock_->isChecked());

  updateTrashButton();
}

void DockSettingsScreen::createLaunchersDialog() {
  if (embeddedLaunchersDialog_) return;
  embeddedLaunchersDialog_ = new EditLaunchersDialog(this, model_, dockId());
  embeddedLaunchersDialog_->configureForEmbedding();
  auto* group = new QGroupBox(tr("Launchers"));
  auto* groupLayout = new QVBoxLayout(group);
  groupLayout->addWidget(embeddedLaunchersDialog_);
  ui->launchersPageLayout->addWidget(group);
}

void DockSettingsScreen::createPanelPage() {
  auto* layout = ui->panelPageLayout;

  auto* positionGroup = new QGroupBox(tr("Position"));
  auto* positionLayout = new QVBoxLayout(positionGroup);
  positionLayout->addWidget(new QLabel(tr("Position on screen:")));
  positionSelector_ = new QComboBox();
  positionSelector_->addItems({tr("Top"), tr("Bottom"), tr("Left"),
                               tr("Right")});
  positionLayout->addWidget(positionSelector_);
  positionLayout->addWidget(new QLabel(tr("Screen:")));
  screenSelector_ = new QComboBox();
  for (unsigned int i = 0; i < WindowSystem::screens().size(); ++i) {
    screenSelector_->addItem("Screen " + QString::number(i + 1));
  }
  positionLayout->addWidget(screenSelector_);
  layout->addWidget(positionGroup);

  auto* visibilityGroup = new QGroupBox(tr("Visibility"));
  auto* visibilityLayout = new QVBoxLayout(visibilityGroup);
  visibilitySelector_ = new QComboBox();
  visibilitySelector_->addItems({tr("Always Visible"), tr("Auto Hide"),
                                 tr("Always On Top"),
                                 tr("Intelligent Auto Hide")});
  visibilityLayout->addWidget(visibilitySelector_);
  layout->addWidget(visibilityGroup);

  auto* actionsGroup = new QGroupBox(tr("Panel Actions"));
  auto* actionsLayout = new QVBoxLayout(actionsGroup);
  auto* addPanelButton = new QPushButton(tr("Add Panel…"));
  auto* clonePanelButton = new QPushButton(tr("Clone Panel…"));
  auto* removePanelButton = new QPushButton(tr("Remove Panel"));
  actionsLayout->addWidget(addPanelButton);
  actionsLayout->addWidget(clonePanelButton);
  actionsLayout->addWidget(removePanelButton);
  connect(addPanelButton, &QPushButton::clicked, this, [this]() {
    if (targetPanel_) targetPanel_->addDock();
  });
  connect(clonePanelButton, &QPushButton::clicked, this, [this]() {
    if (targetPanel_) targetPanel_->cloneDock();
  });
  connect(removePanelButton, &QPushButton::clicked, this, [this]() {
    if (!targetPanel_) return;
    targetPanel_->removeDock();
    // removeDock() deletes the panel on success, so never touch it again.
    close();
  });
  layout->addWidget(actionsGroup);

  layout->addStretch();
}

void DockSettingsScreen::createLaunchersPage() {
  // The embedded launchers dialog is added by createLaunchersDialog().
  auto* layout = ui->launchersPageLayout;

  auto* actionsGroup = new QGroupBox(tr("Launcher Actions"));
  auto* actionsLayout = new QVBoxLayout(actionsGroup);
  auto* newWindowButton = new QPushButton(tr("New Window"));
  auto* pinButton = new QPushButton(tr("Pin/Unpin"));
  auto* closeButton = new QPushButton(tr("Close Window(s)"));
  actionsLayout->addWidget(newWindowButton);
  actionsLayout->addWidget(pinButton);
  actionsLayout->addWidget(closeButton);

  connect(newWindowButton, &QPushButton::clicked, this, []() {
    // TODO: new-window action -- needs the selected Program.
    qInfo() << "TODO: new-window action";
  });
  connect(pinButton, &QPushButton::clicked, this, []() {
    // TODO: pin/unpin action -- needs the selected Program.
    qInfo() << "TODO: pin action";
  });
  connect(closeButton, &QPushButton::clicked, this, []() {
    // TODO: close-window action -- needs the selected Program.
    qInfo() << "TODO: close-window action";
  });
  layout->addWidget(actionsGroup);


  layout->addStretch();
}

void DockSettingsScreen::createOptionalFeaturesPage() {
  auto* layout = ui->featuresPageLayout;
  auto* group = new QGroupBox(tr("Optional Features"));
  auto* groupLayout = new QVBoxLayout(group);
  for (const auto& row : kFeatureRows) {
    auto* checkbox = new QCheckBox(tr(row.first));
    groupLayout->addWidget(checkbox);
    featureToggles_.push_back(checkbox);
  }
  layout->addWidget(group);
  layout->addStretch();
}

void DockSettingsScreen::createAppearancePage() {
  auto* layout = ui->appearancePageLayout;
  auto* group = new QGroupBox(tr("Appearance & Style"));
  auto* groupLayout = new QVBoxLayout(group);
  embeddedAppearanceDialog_ = new AppearanceSettingsDialog(this, model_);
  embeddedAppearanceDialog_->configureForEmbedding();
  groupLayout->addWidget(embeddedAppearanceDialog_);
  layout->addWidget(group);
  layout->addStretch();
}

void DockSettingsScreen::createComponentsPage() {
  auto* layout = ui->componentsPageLayout;



  auto* clockGroup = new QGroupBox(tr("Clock"));
  auto* clockLayout = new QVBoxLayout(clockGroup);
  clockLayout->addWidget(new QLabel(tr("Font family:")));
  clockFontFamilySelector_ = new QFontComboBox();
  clockLayout->addWidget(clockFontFamilySelector_);
  clockLayout->addWidget(new QLabel(tr("Font size (% of default):")));
  clockFontSizeSelector_ = new QSpinBox();
  clockFontSizeSelector_->setRange(50, 150);
  clockLayout->addWidget(clockFontSizeSelector_);
  clock24HourClock_ = new QCheckBox(tr("Use 24-hour clock"));
  clockLayout->addWidget(clock24HourClock_);
  layout->addWidget(clockGroup);



  auto* taskManagerGroup = new QGroupBox(tr("Task Manager"));
  auto* taskManagerLayout = new QVBoxLayout(taskManagerGroup);
  embeddedTaskManagerDialog_ = new TaskManagerSettingsDialog(this, model_);
  embeddedTaskManagerDialog_->configureForEmbedding();
  taskManagerLayout->addWidget(embeddedTaskManagerDialog_);
  layout->addWidget(taskManagerGroup);


  layout->addStretch();
}

void DockSettingsScreen::createHelpPage() {
  auto* layout = ui->helpPageLayout;
  auto* group = new QGroupBox(tr("Help"));
  auto* groupLayout = new QVBoxLayout(group);
  auto* docButton = new QPushButton(tr("Online Documentation"));
  auto* aboutButton = new QPushButton(tr("About Crystal Dock"));
  auto* exitButton = new QPushButton(tr("Exit Dock"));
  groupLayout->addWidget(docButton);
  groupLayout->addWidget(aboutButton);
  groupLayout->addWidget(exitButton);

  connect(docButton, &QPushButton::clicked, this, [this]() {
    if (targetPanel_) targetPanel_->showOnlineDocumentation();
  });
  connect(aboutButton, &QPushButton::clicked, this, [this]() {
    if (targetPanel_) targetPanel_->about();
  });
  connect(exitButton, &QPushButton::clicked, this,
          [this]() { view_->exit(); });
  layout->addWidget(group);
  layout->addStretch();
}

}  // namespace crystaldock
