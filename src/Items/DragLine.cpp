// Items/DragLine.cpp - This file is part of eln

/* eln is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   eln is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with eln.  If not, see <http://www.gnu.org/licenses/>.
*/

// DragLine.C

#include "DragLine.H"
#include "Style.H"
#include <QBrush>
#include <QPen>
#include "Item.H"
#include <QEventLoop>
#include <QGraphicsSceneMouseEvent>
#include "Assert.H"

DragLine::DragLine(QPointF p0, QGraphicsItem *parent):
  QGraphicsLineItem(QLineF(p0, p0), parent) {
}

DragLine::~DragLine() {
}

void DragLine::mouseMoveEvent(QGraphicsSceneMouseEvent *e) {
  // setEndPoint(mapToParent(e->pos()));
  setEndPoint(endPoint() + e->pos() - e->lastPos());
  e->accept();
}

void DragLine::mouseReleaseEvent(QGraphicsSceneMouseEvent *e) {
  e->accept();
  emit release();
}

QPointF DragLine::startPoint() const {
  return line().p1();
}

QPointF DragLine::endPoint() const {
  return line().p2();
}

void DragLine::setEndPoint(QPointF p1) {
  QLineF l = line();
  l.setP2(p1);
  setLine(l);
}

bool DragLine::isShort() const {
  return isShort(startPoint(), endPoint());
}

double DragLine::shortThreshold() {
  return 3.5;
}

bool DragLine::isShort(QPointF p0, QPointF p1) {
  double dx = p1.x() - p0.x();
  double dy = p1.y() - p0.y();
  double thr = shortThreshold();
  return dx*dx + dy*dy < thr*thr;
}


QPointF DragLine::drag(BaseScene *scene, QPointF p0, QColor c) {
  ASSERT(scene);
  double pw = scene->style().real("drag-line-width");
  if (!c.isValid())
    c = scene->style().color("drag-line-color");
  
  DragLine *dl = new DragLine(p0, 0);
  scene->addItem(dl);
  dl->setPen(QPen(c, pw));
  QEventLoop el;
  QObject::connect(dl, SIGNAL(release()),
		   &el, SLOT(quit()),
		   Qt::QueuedConnection);
  // Use queued connection so that we don't try to destruct the dragline
  // prematurely.
  dl->grabMouse();
  el.exec();
  dl->ungrabMouse();
  QPointF p1 = dl->endPoint();
  scene->removeItem(dl);
  delete dl;
  return p1;
}
