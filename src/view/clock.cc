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

#include "clock.h"

#include <QDate>
#include <QTime>
#include <QTimer>

#include "dock_panel.h"
#include <utils/draw_utils.h>
#include <utils/font_utils.h>

namespace crystaldock {

constexpr float Clock::kWhRatio;
constexpr float Clock::kDelta;

Clock::Clock(DockPanel* parent, MultiDockModel* model,
             Qt::Orientation orientation, int minSize, int maxSize)
    : IconlessDockItem(parent, model, "" /* label */, orientation, minSize, maxSize,
                       kWhRatio),
      calendar_(parent) {
  QTimer* timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(updateTime()));
  timer->start(1000);  // update the time every second.
}

void Clock::draw(QPainter *painter) const {
  const QString timeFormat = model_->use24HourClock() ? "hh:mm" : "hh:mm AP";
  const QString time = QTime::currentTime().toString(timeFormat);
  // The reference time used to calculate the font size.
  const QString referenceTime = QTime(8, 8).toString(timeFormat);

  const int margin = parent_->isHorizontal() ? getHeight() * 0.1 : 0;
  const auto x = left_ + margin;
  const auto y = top_;
  const auto w = getWidth() - margin;
  const auto h = getHeight();
  painter->setFont(adjustFontSize(w, h, referenceTime,
                                  model_->clockFontScaleFactor(),
                                  model_->clockFontFamily()));
  painter->setRenderHint(QPainter::TextAntialiasing);
  if (size_ > minSize_) {
    drawBorderedText(x, y , w, h, Qt::AlignCenter, time,
                     2 /* borderWidth */, Qt::black, Qt::white, painter,
                     /*simplified=*/ true);
  } else {
    painter->setPen(Qt::white);
    painter->drawText(x, y, w, h, Qt::AlignCenter, time);
  }
}

void Clock::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton) {
    calendar_.showCalendar();
  }
}

QString Clock::getLabel() const {
  return QLocale::system().toString(QDate::currentDate(), QLocale::LongFormat);
}

void Clock::updateTime() {
  parent_->update();
}


}  // namespace crystaldock
