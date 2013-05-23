// Items/TextItemText.cpp - This file is part of eln

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

// TextItemText.C

#include "TextItemText.H"
#include "TextItem.H"
#include <QDebug>
#include <QGraphicsSceneHoverEvent>
#include <QStyleOptionGraphicsItem>
#include <QStyle>
#include <QGraphicsScene>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QGraphicsSceneDragDropEvent>

TextItemText::TextItemText(TextItem *parent): QGraphicsTextItem(parent) {
  forcebox = false;
}

TextItemText::~TextItemText() {
}

TextItem *TextItemText::parent() {
  return dynamic_cast<TextItem*>(QGraphicsTextItem::parentItem());
}

void TextItemText::mousePressEvent(QGraphicsSceneMouseEvent *e) {
  // Following is an ugly way to clear selection from previously
  // focused text. Can I do better?
  foreach (QGraphicsItem *i, scene()->items()) {
    QGraphicsTextItem *gti = dynamic_cast<QGraphicsTextItem*>(i);
    if (gti && gti!=this) {
      QTextCursor c = gti->textCursor();
      if (c.hasSelection()) {
        c.clearSelection();
        gti->setTextCursor(c);
        break;
      }
    }
  }
  // End of ugly code
  
  if (parent() && !parent()->mousePress(e)) 
    QGraphicsTextItem::mousePressEvent(e);
}

void TextItemText::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) {
  if (parent() && !parent()->mouseDoubleClick(e)) 
    QGraphicsTextItem::mouseDoubleClickEvent(e);
}

void TextItemText::keyPressEvent(QKeyEvent *e) {
  if (parent() && !parent()->keyPress(e)) 
    QGraphicsTextItem::keyPressEvent(e);    
}

void TextItemText::internalKeyPressEvent(QKeyEvent *e) {
  QGraphicsTextItem::keyPressEvent(e);    
}
  
void TextItemText::focusOutEvent(QFocusEvent *e) {
  if (parent() && !parent()->focusOut(e)) 
    QGraphicsTextItem::focusOutEvent(e);
}

void TextItemText::focusInEvent(QFocusEvent *e) {
  if (parent() && !parent()->focusIn(e)) 
    QGraphicsTextItem::focusInEvent(e);
}

void TextItemText::hoverMoveEvent(QGraphicsSceneHoverEvent *e) {
  if (parent())
    parent()->hoverMove(e);
} 

void TextItemText::setBoxVisible(bool v) {
  qDebug() << "TextItemText" << v;
  forcebox = v;
  update();
}

void TextItemText::paint(QPainter * p,
			 const QStyleOptionGraphicsItem *s,
			 QWidget *w) {
  if (forcebox) {
    QStyleOptionGraphicsItem sogi(*s);
    sogi.state |= QStyle::State_HasFocus;
    QGraphicsTextItem::paint(p, &sogi, w);
  } else {
    QGraphicsTextItem::paint(p, s, w);
  }
}

int pointToPos(QGraphicsTextItem const *item, QPointF p) {
  QTextDocument *doc = item->document();
  for (QTextBlock b = doc->begin(); b!=doc->end(); b=b.next()) {
    QTextLayout *lay = b.layout();
    if (lay->boundingRect().contains(p)) {
      p -= lay->position();
      int nLines = lay->lineCount();
      for (int i=0; i<nLines; i++) {
	QTextLine line = lay->lineAt(i); // yes, this returns the i-th line
	if (line.rect().contains(p)) 
	  return line.xToCursor(p.x());
      }
      qDebug() << "TextItem: point in block but not in a line!?";
      return -1;
    }
  }
  return -1;
}

QPointF posToPoint(QGraphicsTextItem const *item, int i) {
  QTextBlock blk = item->document()->findBlock(i);
  if (!blk.isValid())
    return QPointF();
  QTextLayout *lay = blk.layout();
  if (!lay)
    return QPointF();
  if (!lay->isValidCursorPosition(i - blk.position()))
    return QPointF();
  QTextLine line = lay->lineForTextPosition(i - blk.position());
  if (!line.isValid())
    return QPointF();
  QPointF p(line.cursorToX(i-blk.position()), line.y()+line.ascent());
  return p + lay->position();
}

void TextItemText::dropEvent(QGraphicsSceneDragDropEvent *e) {
  //qDebug() << "TextItemText"<<this<<"::drop from "<<e->source();
  //if (e->mimeData()->hasText()) {
  //  int p = pointToPos(this, e->pos());
  //  qDebug() << "  p="<<p;
  //  if (p>=0) {
  //    QTextCursor c(textCursor());
  //    c.setPosition(p);
  //    c.insertText(e->mimeData()->text());
  //    setTextCursor(c);
  //    qDebug() << "  action was "<<e->proposedAction() << e->possibleActions();
  //    e->acceptProposedAction();
  //    setFocus();
  //    return;
  //  }
  //}

  // For whatever reason, this doesn't work well, so I'll just ignore all drops.
  
  //  e->setDropAction(Qt::IgnoreAction);
  //  QGraphicsTextItem::dropEvent(e);
}