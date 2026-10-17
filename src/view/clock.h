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

#ifndef CRYSTALDOCK_CLOCK_H_
#define CRYSTALDOCK_CLOCK_H_

#include "iconless_dock_item.h"

#include <QObject>
#include <QString>

#include "calendar.h"

namespace crystaldock {

// A digital clock.
class Clock : public QObject, public IconlessDockItem {
  Q_OBJECT

 public:
  Clock(DockPanel* parent, MultiDockModel* model, Qt::Orientation orientation,
        int minSize, int maxSize);
  virtual ~Clock() = default;

  void draw(QPainter* painter) const override;
  void mousePressEvent(QMouseEvent* e) override;
  QString getLabel() const override;
  bool beforeTask(const QString& program) override { return false; }

 public slots:
  void updateTime();

private:
  static constexpr float kWhRatio = 2.9;
  static constexpr float kDelta = 0.01;

  Calendar calendar_;
};

}  // namespace crystaldock

#endif  // CRYSTALDOCK_CLOCK_H_
