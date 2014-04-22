// Scenes/EntryScene.cpp - This file is part of eln

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

// EntryScene.C

#include "EntryScene.H"
#include "SheetScene.H"
#include "Style.H"
#include "BlockData.H"
#include "BlockItem.H"
#include "TitleItem.H"
#include "TextBlockItem.H"
#include "TableBlockItem.H"
#include "TextBlockData.H"
#include "TableBlockData.H"
#include "EntryData.H"
#include "GfxBlockItem.H"
#include "GfxBlockData.H"
#include "ResManager.H"
#include "Mode.H"
#include "FootnoteData.H"
#include "FootnoteItem.H"
#include "Assert.H"
#include "Notebook.H"
#include "LateNoteItem.H"
#include "GfxNoteItem.H"
#include "GfxNoteData.H"
#include "TableItem.H"
#include "TextItemText.H"
#include "SvgFile.H"
#include "Restacker.H"

#include <QGraphicsView>
#include <QGraphicsTextItem>
#include <QGraphicsLineItem>
#include <QDateTime>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextLine>
#include <QTextLayout>
#include <QTextBlock>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QSignalMapper>
#include <QCursor>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QClipboard>
#include <QApplication>
#include <QMimeData>

EntryScene::EntryScene(CachedEntry data, QObject *parent):
  BaseScene(data, parent),
  data_(data) {
  writable = false;
  firstDisallowedPgNo = 0;
  
  unlockedItem = 0;

  vChangeMapper = new QSignalMapper(this);
  futileMovementMapper = new QSignalMapper(this);
  connect(vChangeMapper, SIGNAL(mapped(int)), SLOT(vChanged(int)));
  connect(futileMovementMapper, SIGNAL(mapped(int)), SLOT(futileMovement(int)));
}

void EntryScene::populate() {
  BaseScene::populate();
  makeBlockItems();
  positionBlocks();

  if (data()->isUnlocked())
    addUnlockedWarning();
}

QDate EntryScene::date() const {
  return data_->created().date();
}

void EntryScene::makeBlockItems() {
  foreach (BlockData *bd, data_->blocks()) {
    BlockItem *bi = tryMakeTableBlock(bd);
    if (!bi)
      bi = tryMakeTextBlock(bd);
    if (!bi)
      bi = tryMakeGfxBlock(bd);
    ASSERT(bi);
    connect(bi, SIGNAL(heightChanged()), vChangeMapper, SLOT(map()));
    blockItems.append(bi);
  }
  redateBlocks();
  remap();
}

BlockItem *EntryScene::tryMakeGfxBlock(BlockData *bd) {
  GfxBlockData *gbd = dynamic_cast<GfxBlockData*>(bd);
  if (!gbd)
    return 0;
  GfxBlockItem *gbi = new GfxBlockItem(gbd);
  gbi->setBaseScene(this);
  if (gbd->height()==0)
    gbi->sizeToFit();
  return gbi;
}

BlockItem *EntryScene::tryMakeTableBlock(BlockData *bd) {
  TableBlockData *tbd = dynamic_cast<TableBlockData*>(bd);
  if (!tbd)
    return 0;
  TableBlockItem *tbi = new TableBlockItem(tbd);
  tbi->setBaseScene(this);
  if (tbd->height()==0)
    tbi->sizeToFit();
  connect(tbi, SIGNAL(futileMovement()), futileMovementMapper, SLOT(map()));
  return tbi;
}

BlockItem *EntryScene::tryMakeTextBlock(BlockData *bd) {
  TextBlockData *tbd = dynamic_cast<TextBlockData*>(bd);
  if (!tbd)
    return 0;
  TextBlockItem *tbi = new TextBlockItem(tbd);
  tbi->setBaseScene(this);
  if (tbd->height()==0)
    tbi->sizeToFit();
  connect(tbi, SIGNAL(futileMovement()), futileMovementMapper, SLOT(map()));
  return tbi;
}
  
EntryScene::~EntryScene() {
}

void EntryScene::titleEdited() {
  foreach (SheetScene *s, sheets)
    s->repositionTitle();
}

TitleData *EntryScene::fancyTitle() const {
  return data()->title();
}

void EntryScene::positionBlocks() {
  int isheet = 0;
  foreach (BlockItem *bi, blockItems) {
    isheet = bi->data()->sheet(); 
    sheet(isheet, true)->addItem(bi);
    bi->resetPosition();
    foreach (FootnoteItem *fni, bi->footnotes()) {
      int jsheet = fni->data()->sheet();
      if (jsheet<0)
	jsheet = isheet;
      sheet(isheet, true)->addItem(fni);
      fni->resetPosition();
    }
  }
  setSheetCount(isheet+1);
}

void EntryScene::restackBlocks(int start) {
  if (!writable)
    return;

  Restacker restacker(blockItems, start);
  restacker.restackData();
  restacker.restackItems(*this);

  redateBlocks();
  resetSheetCount();
  qDebug() << "EntryScene::restacked";
  emit restacked();
}

void EntryScene::resetSheetCount() {
  int maxsheet = 0;
  foreach (BlockData *bd, data()->children<BlockData>()) {
    if (bd->sheet()>maxsheet)
      maxsheet = bd->sheet();
    foreach (FootnoteData *fnd, bd->children<FootnoteData>()) 
      if (fnd->sheet()>maxsheet)
	maxsheet = fnd->sheet();
  }
  setSheetCount(maxsheet + 1);
}  

void EntryScene::redateBlocks() {
  QDate cre = data()->created().date();
  QDate mod = cre;
  QMap<BlockItem *, QString> txt;
  for (int i=0; i<blockItems.size(); i++) {
    QDate cre1 = blockItems[i]->data()->created().date();
    QDate mod1 = blockItems[i]->data()->modified().date();
    if (cre1==cre && mod1==mod)
      continue;
    QString txt1;
    if (cre1.year()==cre.year())
      txt1 = cre1.toString(style().string("date-format-yearless"));
    else
      txt1 = cre1.toString(style().string("date-format"));
    if (mod1!=cre1) {
      // date range
      txt1 += QString::fromUtf8("‒"); // figure dash
      if (mod1.year()==cre1.year())
	txt1 += mod1.toString(style().string("date-format-yearless"));
      else
	txt1 += mod1.toString(style().string("date-format"));
    }
    txt[blockItems[i]] = txt1;
    cre = cre1;
    mod = mod1;
  }

  foreach (BlockItem *i, blockDateItems.keys()) {
    if (!txt.contains(i)) {
      delete blockDateItems[i];
      blockDateItems.remove(i);
    }
  }
  foreach (BlockItem *i, txt.keys()) {
    QGraphicsTextItem *dateItem = blockDateItems[i];
    if (!dateItem) {
      dateItem = blockDateItems[i] = new QGraphicsTextItem(i);
      dateItem->setFont(style().font("date-font"));
      dateItem->setDefaultTextColor(style().color("date-color"));
    }
    dateItem->setPlainText(txt[i]);
    QPointF sp = i->scenePos();
    QRectF br = dateItem->sceneBoundingRect();
    double ml = style().real("margin-left");
    dateItem->setPos(QPointF(ml - br.width() - 2 - sp.x(),
			     style().real("text-block-above")));
  }    
}

void EntryScene::notifyChildless(BlockItem *gbi) {
  if (!dynamic_cast<GfxBlockItem*>(gbi))
    return;
  if (!writable)
    return;
  //  qDebug() << "childless" << this << gbi;
  int iblock = -1;
  for (int i=0; i<blockItems.size(); ++i) {
    if (blockItems[i] == gbi) {
      //      qDebug() << "deleting block " << i;
      deleteBlock(i);
      iblock = i;
      break;
    }
  }
  //  qDebug() << "restacking";
  if (iblock>=0)
    restackBlocks(iblock);
}
  
void EntryScene::deleteBlock(int blocki) {
  if (blocki>=blockItems.size()) {
    qDebug() << "EntryScene: deleting nonexisting block " << blocki;
    return;
  }
  BlockItem *bi = blockItems[blocki];
  BlockData *bd = bi->data();

  blockItems.removeAt(blocki);
  remap();

  foreach (FootnoteItem *fni, bi->footnotes()) {
    QGraphicsScene *s = fni->scene();
    if (s)
      s->removeItem(fni);
    fni->deleteLater();
  }

  QGraphicsScene *s = bi->scene();
  if (s)
    s->removeItem(bi);
  bi->deleteLater();

  data_->deleteBlock(bd);
}

GfxBlockItem *EntryScene::newGfxBlock(int iAbove) {
  int iNew = (iAbove>=0)
    ? iAbove + 1
    : blockItems.size();

  if (iAbove>=0) {
    // perhaps not create a new one after all
    GfxBlockItem *tbi = dynamic_cast<GfxBlockItem *>(blockItems[iAbove]);
    if (tbi && tbi->isWritable()) {
      // Previous block is writable, use it instead
      return tbi;
    }
  }

  GfxBlockData *gbd = new GfxBlockData();
  QList<BlockData *> existingBlocks = data_->blocks();
  if (iNew<existingBlocks.size())
    data_->insertBlockBefore(gbd, existingBlocks[iNew]);
  else
    data_->addBlock(gbd);
  GfxBlockItem *gbi = new GfxBlockItem(gbd);
  gbi->setBaseScene(this);
  gbi->sizeToFit();
  gbi->makeWritable();
  
  blockItems.insert(iNew, gbi);

  connect(gbi, SIGNAL(heightChanged()), vChangeMapper, SLOT(map()));
  remap();

  restackBlocks(iNew);
  gotoSheetOfBlock(iNew);
  return gbi;
}

void EntryScene::splitTextBlock(int iblock, int pos) {
  // block number iblock is going to be split
  TextBlockData *orig =
    dynamic_cast<TextBlockData*>(blockItems[iblock]->data());
  ASSERT(orig);
  TextBlockData *block1 = Data::deepCopy(orig);
  deleteBlock(iblock);
  TextBlockData *block2 = block1->split(pos);
  injectTextBlock(block1, iblock);
  TextBlockItem *tbi_post = injectTextBlock(block2, iblock+1);
  restackBlocks(iblock);
  gotoSheetOfBlock(iblock+1);
  tbi_post->setFocus();
}

void EntryScene::joinTextBlocks(int iblock_pre, int iblock_post) {
  ASSERT(iblock_pre<iblock_post);
  ASSERT(iblock_pre>=0);
  ASSERT(iblock_post<blockItems.size());
  TextBlockItem *tbi_pre
    = dynamic_cast<TextBlockItem*>(blockItems[iblock_pre]);
  TextBlockItem *tbi_post
    = dynamic_cast<TextBlockItem*>(blockItems[iblock_post]);
  ASSERT(tbi_pre);
  ASSERT(tbi_post);
  TextBlockData *block1 = Data::deepCopy(tbi_pre->data());
  TextBlockData *block2 = Data::deepCopy(tbi_post->data());
  int pos = block1->text()->text().size();
  deleteBlock(iblock_post);
  deleteBlock(iblock_pre);
  block1->join(block2);
  TextBlockItem *tbi = injectTextBlock(block1, iblock_pre);
  restackBlocks(iblock_pre);
  gotoSheetOfBlock(iblock_pre);
  tbi->setFocus();
  QTextCursor c(tbi->text()->document());
  c.setPosition(pos);
  tbi->text()->setTextCursor(c);
}  

TableBlockItem *EntryScene::injectTableBlock(TableBlockData *tbd, int iblock) {
  // creates a new table block immediately before iblock (or at end if iblock
  // points past the last text block)
  BlockData *tbd_next =  iblock<blockItems.size()
    ? data_->blocks()[iblock]
    : 0;
  data_->insertBlockBefore(tbd, tbd_next);
  TableBlockItem *tbi = new TableBlockItem(tbd);
  tbi->setBaseScene(this);
  tbi->sizeToFit();
  tbi->makeWritable();

  blockItems.insert(iblock, tbi);
  connect(tbi, SIGNAL(heightChanged()), vChangeMapper, SLOT(map()));
  connect(tbi, SIGNAL(futileMovement()), futileMovementMapper, SLOT(map()));
  remap();
  return tbi;
}

TextBlockItem *EntryScene::injectTextBlock(TextBlockData *tbd, int iblock) {
  // creates a new text block immediately before iblock (or at end if iblock
  // points past the last text block)
  BlockData *tbd_next =  iblock<blockItems.size()
    ? data_->blocks()[iblock]
    : 0;
  data_->insertBlockBefore(tbd, tbd_next);
  TextBlockItem *tbi = new TextBlockItem(tbd);
  tbi->setBaseScene(this);
  tbi->sizeToFit();
  tbi->makeWritable();

  blockItems.insert(iblock, tbi);
  connect(tbi, SIGNAL(heightChanged()), vChangeMapper, SLOT(map()));
  connect(tbi, SIGNAL(futileMovement()), futileMovementMapper, SLOT(map()));
  remap();
  return tbi;
}

void EntryScene::remap() {
  for (int i=0; i<blockItems.size(); i++) {
    BlockItem *bi = blockItems[i];
    vChangeMapper->setMapping(bi, i);
    futileMovementMapper->setMapping(bi, i);
  }
}

TableBlockItem *EntryScene::newTableBlock(int iAbove) {
  int iNew = (iAbove>=0)
    ? iAbove + 1
    : blockItems.size();

  TableBlockData *tbd = new TableBlockData();
  TableBlockItem *tbi = injectTableBlock(tbd, iNew);
  
  restackBlocks(iNew);
  gotoSheetOfBlock(iNew);
  tbi->setFocus();
  return tbi;
}

int EntryScene::lastBlockAbove(QPointF scenepos, int sheet) {
  for (int i=0; i<blockItems.size(); i++) {
    if (blockItems[i]->data()->sheet() != sheet)
      continue;
    double y = blockItems[i]->sceneBoundingRect().bottom();
    if (y>scenepos.y())
      return i-1;
  }
  return blockItems.size()-1;
}

TextBlockItem *EntryScene::newTextBlockAt(QPointF scenepos, int sheet,
                                          bool evenIfLastEmpty) {
  return newTextBlock(lastBlockAbove(scenepos, sheet), evenIfLastEmpty);
}

GfxBlockItem *EntryScene::newGfxBlockAt(QPointF scenepos, int sheet) {
  return newGfxBlock(lastBlockAbove(scenepos, sheet));
}

TableBlockItem *EntryScene::newTableBlockAt(QPointF scenepos, int sheet) {
  return newTableBlock(lastBlockAbove(scenepos, sheet));
}


TextBlockItem *EntryScene::newTextBlock(int iAbove, bool evenIfLastEmpty) {
  int iNew = (iAbove>=0)
    ? iAbove + 1
    : blockItems.size();

  bool prevIsEmpty = false;
  if (iNew>0) {
    TextBlockItem *tbi = dynamic_cast<TextBlockItem *>(blockItems[iNew-1]);
    if (tbi && tbi->document()->isEmpty()) {
      prevIsEmpty = true;
      if (iAbove>=0 && tbi->isWritable() && !evenIfLastEmpty) {
        tbi->setFocus();
        return tbi;
      }
    }
  }

  TextBlockData *tbd = new TextBlockData(); // create w/o parent
  if (!style().flag("always-indent") && (iNew==0 || prevIsEmpty))
    tbd->setIndented(false);
  TextBlockItem *tbi = injectTextBlock(tbd, iNew); // this parents the block
  
  restackBlocks(iNew);
  gotoSheetOfBlock(iNew);
  tbi->setFocus();
  return tbi;
}

void EntryScene::futileMovement(int block) {
  qDebug() << "futilemovement" << block;
  ASSERT(block>=0 && block<blockItems.size());
  // futile movement in a text block
  TextBlockItem *tbi = dynamic_cast<TextBlockItem *>(blockItems[block]);
  if (!tbi) {
    qDebug() << "not a text block";
    return;
  }

  FutileMovementInfo fmi = tbi->lastFutileMovement();
  int tgtidx = -1;
  if (fmi.key()==Qt::Key_Enter || fmi.key()==Qt::Key_Return) {
    QTextCursor c = tbi->text()->textCursor();
    if (c.atEnd()) 
      newTextBlock(block, true);
    else
      splitTextBlock(block, c.position());
    return;
  }
  
  if (fmi.key()==Qt::Key_Left || fmi.key()==Qt::Key_Up
      || fmi.key()==Qt::Key_Backspace) {
    // upward movement
    for (int b=block-1; b>=0; b--) {
      BlockItem *bi = blockItems[b];
      if (bi->data()->type()=="textblock" && bi->isWritable()) {
	tgtidx = b;
	break;
      }
    }
  } else if (fmi.key()==Qt::Key_Right || fmi.key()==Qt::Key_Down
	     || fmi.key()==Qt::Key_Delete) {
    // downward movement
    for (int b=block+1; b<blockItems.size(); b++) {
      BlockItem *bi = blockItems[b];
      if (bi->data()->type()=="textblock" && bi->isWritable()) {
	tgtidx = b;
	break;
      }
    }
  }

  if (tgtidx<0) {
    // no target, go to start/end of current
    QTextCursor c = tbi->text()->textCursor();
    if (fmi.key()==Qt::Key_Down) {
      c.movePosition(QTextCursor::End);
      tbi->text()->setTextCursor(c);
    } else if (fmi.key()==Qt::Key_Up) {
      c.movePosition(QTextCursor::Start);
      tbi->text()->setTextCursor(c);
    }
    return;
  }

  if (fmi.key()==Qt::Key_Delete) {
    if (tgtidx==block+1) // do not combine across (e.g.) gfxblocks
      joinTextBlocks(block, tgtidx);
    return;
  } else if (fmi.key()==Qt::Key_Backspace) {
    if (tgtidx==block-1)
      joinTextBlocks(tgtidx, block);
    return;
  }
  
  TextBlockItem *tgt = dynamic_cast<TextBlockItem*>(blockItems[tgtidx]);
  ASSERT(tgt);

  qDebug() << "EntryScene::futilemovement tgtidx="<<tgtidx;
  gotoSheetOfBlock(tgtidx);
  tgt->setFocus();
  qDebug() << "EntryScene::futilemovement setfocus";

  QTextDocument *doc = tgt->document();
  QTextCursor c(doc);
  QPointF p = tgt->text()->mapFromParent(tgt->mapFromScene(fmi.scenePos()));
  switch (fmi.key()) {
  case Qt::Key_Left: 
    c.movePosition(QTextCursor::End);
    break;
  case Qt::Key_Up: {
    QTextBlock blk = doc->lastBlock();
    QTextLayout *lay = blk.layout();
    QTextLine l = lay->lineAt(lay->lineCount()-1);
    c.setPosition(blk.position() + l.xToCursor(p.x() - lay->position().x()));
    break; }
  case Qt::Key_Right:
    c.movePosition(QTextCursor::Start);
    break;
  case Qt::Key_Down: {
    QTextBlock blk = doc->firstBlock();
    QTextLayout *lay = blk.layout();
    QTextLine l = lay->lineAt(0);
    c.setPosition(l.xToCursor(p.x() - blk.layout()->position().x()));
    break; }
  }
  qDebug() << "EntryScene::futilemovement setcursor " << c.position();
  tgt->text()->setTextCursor(c);
  tgt->setFocus();
}

void EntryScene::futileTitleMovement(int key, Qt::KeyboardModifiers) {
  switch (key) {
  case Qt::Key_Enter: case Qt::Key_Return:
  case Qt::Key_Down:
    focusEnd();
    break;
  default:
    break;
  }
}

void EntryScene::focusEnd(int isheet) {
  qDebug() << "EntryScene::focusEnd" << writable;
  if (!writable)
    return;

  TextBlockItem *lastbi = 0;
  for (int i=0; i<blockItems.size(); ++i) {
    TextBlockItem *tbi = dynamic_cast<TextBlockItem *>(blockItems[i]);
    if (tbi && (isheet<0 || tbi->data()->sheet()==isheet))
      lastbi = tbi;
  }
  if (lastbi) {
    lastbi->setFocus();
    QTextCursor tc = lastbi->text()->textCursor();
    tc.movePosition(QTextCursor::End);
    lastbi->text()->setTextCursor(tc);    
  } else {
    if (isheet == nSheets-1)
      newTextBlock(blockItems.size()-1); // create new text block only on last sheet
  }
}


void EntryScene::vChanged(int block) {
  restackBlocks(block);
  gotoSheetOfBlock(block);
}

bool EntryScene::mousePressEvent(QGraphicsSceneMouseEvent *e, SheetScene *s) {
  QPointF sp = e->scenePos();
  int sh = findSheet(s);
  qDebug() << "EntryScene::mousePressEvent: item at " << sp
	   << " is " << itemAt(sp, sh);
  TextItemText *tit = dynamic_cast<TextItemText*>(itemAt(sp, sh));
  if (tit) {
    qDebug() << " bound=" << tit->sceneBoundingRect()
	     << " txt= " << tit->document()->toPlainText();
  }
  int sheet = findSheet(s);
  bool take = false;
  if (inMargin(sp)) {
    if (itemAt(sp, sh)==0) {
      qDebug() << "  in margin";
      if (data_->book()->mode()->mode()==Mode::Annotate) {
        GfxNoteItem *note = createNote(sp, sheet);
        qDebug() << "created note" << note;
        take = true;
      }
    }
  } else if (itemAt(sp, sh)==0) {
    switch (data_->book()->mode()->mode()) {
    case Mode::Mark: case Mode::Freehand:
      if (isWritable()) {
	GfxBlockItem *blk = newGfxBlockAt(sp, sheet);
	e->setPos(blk->mapFromScene(e->scenePos())); // brutal!
	blk->mousePressEvent(e);
	take = true;
      }
      break;
    case Mode::Type:
      if (writable) {
	newTextBlockAt(sp, sheet);
	take = true;
      }
      break;
    case Mode::Table:
      if (writable) {
	newTableBlockAt(sp, sheet);
	take = true;
      }
      break;
    case Mode::Annotate: {
      createNote(sp, sheet);
      take = true;
    } break;
    default:
      break;
    }
  }
  if (take)
    e->accept();
  return take;
}

bool EntryScene::keyPressEvent(QKeyEvent *e, SheetScene *s) {
  if (e->key()==Qt::Key_Tab || e->key()==Qt::Key_Backtab) {
    TextItemText *focus = dynamic_cast<TextItemText*>(s->focusItem());
    if (focus) {
      focus->keyPressEvent(e);
      e->accept();
      return true;
    }
  }
  if (e->modifiers() & Qt::ControlModifier) {
    bool steal = false;
    switch (e->key()) {
    case Qt::Key_V:
      if (writable)
	steal = tryToPaste(s);
      break;
    case Qt::Key_U:
      if (e->modifiers() & Qt::ShiftModifier) {
        unlock();
        steal = true;
      }
    }
    if (steal) {
      e->accept();
      return true;
    }
  }
  return false;
}

void EntryScene::unlock() {
  if (!writable) {
    data()->setUnlocked(true);
    makeWritable();
    addUnlockedWarning();
  }
}

void EntryScene::addUnlockedWarning() {
  if (unlockedItem)
    return;
  foreach (SheetScene *s, sheets) {
    unlockedItem = new QGraphicsTextItem();
    s->addItem(unlockedItem);
    unlockedItem->setPlainText(style().string("unlocked-text"));
    unlockedItem->setFont(style().font("unlocked-font"));
    unlockedItem->setDefaultTextColor(style().color("unlocked-color"));
    QRectF br = unlockedItem->sceneBoundingRect();
    unlockedItem->setPos(style().real("page-width") - br.width() - 4, 4);
  }
}  


int EntryScene::findBlock(Item const *i) const {
  BlockItem const *bi = i->ancestralBlock();
  for (int i=0; i<blockItems.size(); i++) 
    if (blockItems[i] == bi)
      return i;
  return -1;
}

int EntryScene::findBlock(QPointF scenepos, int sheet) const {
  for (int i=0; i<blockItems.size(); i++) {
    BlockData const *bd = blockItems[i]->data();
    if (bd->sheet() != sheet)
      continue;
    double y0 = bd->y0();
    if (scenepos.y()>=y0 && scenepos.y()<=y0+bd->height())
      return i;
  }
  return -1;
}

bool EntryScene::tryToPaste(SheetScene *s) {
  /* We get it first.
     If we don't send the event on to QGraphicsScene, textItems don't get it.
     What we do is dependent both on mimetype and on whether we have focus.
     If we have focus, text is not handled here, and images are placed in
     a new block below the focused block or in the focused block if it is a
     canvas.
     If we don't have focus, anything is placed in the focused block if it
     is a canvas, or in a new block if not.
  */
  if (!isWritable())
    return false;
  
  QPointF scenePos;
  QGraphicsTextItem *fi = dynamic_cast<QGraphicsTextItem*>(s->focusItem());

  if (fi) {
    scenePos = fi->mapToScene(posToPoint(fi, fi->textCursor().position()));
  } else {
    QList<QGraphicsView*> vv = s->views();
    if (vv.isEmpty()) {
      qDebug() << "EntryScene: cannot determine paste position: no view";
      return false;
    }
    if (vv.size()>1) {
      qDebug() << "EntryScene: multiple views: won't determine paste position";
      return false;
    }
    scenePos = vv[0]->mapToScene(vv[0]->mapFromGlobal(QCursor::pos()));
  }
  int sheet = findSheet(s);
  
  QClipboard *cb = QApplication::clipboard();
  QMimeData const *md = cb->mimeData(QClipboard::Clipboard);
  qDebug() << "EntryScene::trytopaste" << md;
  if (md->hasImage())
    return importDroppedImage(scenePos, sheet,
			      qvariant_cast<QImage>(md->imageData()),
			      QUrl());
  if (fi) 
    return false; // we'll import Urls as text into the focused item

  if (md->hasUrls())
    return importDroppedUrls(scenePos, sheet, md->urls());
  else if (md->hasText())
    return importDroppedText(scenePos, sheet, md->text());
  else
    return false;
}

bool EntryScene::dropBelow(QPointF scenePos, int sheet, QMimeData const *md) {
  if (!isWritable())
    return false;
  if (md->hasImage())
    return importDroppedImage(scenePos, sheet,
			      qvariant_cast<QImage>(md->imageData()),
			      QUrl());
  else if (md->hasUrls())
    return importDroppedUrls(scenePos, sheet, md->urls());
  else if (md->hasText())
    return importDroppedText(scenePos, sheet, md->text());
  else
    return false;
}

bool EntryScene::importDroppedSvg(QPointF scenePos, int sheet,
				  QUrl const &source) {
  QImage img(SvgFile::downloadAsImage(source));
  if (img.isNull()) 
    return importDroppedText(scenePos, sheet, source.toString());
  else
    return importDroppedImage(scenePos, sheet, img, source);
}

bool EntryScene::importDroppedImage(QPointF scenePos, int sheet,
				    QImage const &img, QUrl const &source) {
  // Return true if we want it
  /* If dropped on an existing gfxblock, insert it there.
     If dropped on belowItem, insert after last block on page.
     If dropped on text block, insert after that text block.
     Before creating a new graphics block, consider whether there is
     a graphics block right after it.
   */
  QPointF pdest(0,0);

  int i = findBlock(scenePos, sheet);
  GfxBlockItem *gdst = (i>=0) ? dynamic_cast<GfxBlockItem*>(blockItems[i]) : 0;
  if (gdst) 
    pdest = gdst->mapFromScene(scenePos);
  else if (i>=0 && i+1<blockItems.size())
    gdst = dynamic_cast<GfxBlockItem*>(blockItems[i+1]);
  if (!gdst)
    gdst = newGfxBlockAt(scenePos, sheet);

  gdst->newImage(img, source, pdest);
  gotoSheetOfBlock(findBlock(gdst));
  return true;
}

int EntryScene::indexOfBlock(BlockItem *bi) const {
  for (int i=0; i<blockItems.size(); ++i)
    if (blockItems[i]==bi)
      return i;
  return -1;
}

bool EntryScene::importDroppedUrls(QPointF scenePos, int sheet,
				   QList<QUrl> const &urls) {
  bool ok = false;
  foreach (QUrl const &u, urls)
    if (importDroppedUrl(scenePos, sheet, u))
      ok = true;
  return ok;
}

bool EntryScene::importDroppedUrl(QPointF scenePos, int sheet,
				  QUrl const &url) {
  // QGraphicsItem *dst = itemAt(scenePos);
  /* A URL could be any of the following:
     (1) A local image file
     (2) A local file of non-image type
     (3) An internet-located image file
     (4) A web-page
     (5) Anything else
  */
  qDebug() << "importdroppedurl" << url.toString();
  if (url.isLocalFile()) {
    QString path = url.toLocalFile();
    if (path.endsWith(".svg")) 
      return importDroppedSvg(scenePos, sheet, url);
    QImage image = QImage(path);
    if (!image.isNull())
      return importDroppedImage(scenePos, sheet, image, url);
    else 
      return importDroppedFile(scenePos, sheet, path);
  } else {
    // Right now, we import all network urls as text
    return importDroppedText(scenePos, sheet, url.toString());
  }
  return false;
}

bool EntryScene::importDroppedText(QPointF scenePos, int sheet,
				   QString const &txt,
                                   TextItem **itemReturn,
                                   int *startReturn, int *endReturn) {
  TextItem *ti = 0;
  if (inMargin(scenePos)) {
    GfxNoteItem *note = createNote(scenePos, sheet);
    ti = note->textItem();
  } else {
    int blk = findBlock(scenePos, sheet);
    if (blk>=0) {
      if (blockItems[blk]->isWritable()) {
        GfxBlockItem *gbi = dynamic_cast<GfxBlockItem*>(blockItems[blk]);
        TextBlockItem *tbi = dynamic_cast<TextBlockItem*>(blockItems[blk]);
        if (tbi) {
          ti = tbi->text();
        } else if (gbi) {
          GfxNoteItem *note = gbi->createNote(gbi->mapFromScene(scenePos),
                                              false);
          ti = note->textItem();
        }
      } else { // not writable block
        GfxNoteItem *note = createNote(scenePos, sheet);
        ti = note->textItem();
      }
    }
  }

  if (!ti)
    return false;

  if (itemReturn)
    *itemReturn = ti;
  
  QTextCursor c = ti->textCursor();
  int pos = ti->pointToPos(ti->mapFromScene(scenePos));
  if (pos>=0)
    c.setPosition(pos);
  else
    c.clearSelection();
  if (startReturn)
    *startReturn = c.position();
  c.insertText(txt);
  if (endReturn)
    *endReturn = c.position();

  ti->setFocus();
  ti->setTextCursor(c);

  return true;
}

bool EntryScene::importDroppedFile(QPointF scenePos, int sheet,
				   QString const &fn) {
  //  qDebug() << "EntryScene: import dropped file: " << scenePos << fn;
  if (!fn.startsWith("/"))
    return false;
  int start, end;
  TextItem *ti;
  if (!importDroppedText(scenePos, sheet, fn, &ti, &start, &end))
    return false;

  ti->addMarkup(MarkupData::Link, start, end);
  return true;
}
    
void EntryScene::makeWritable() {
  writable = true;
  //belowItem->setCursor(Qt::IBeamCursor);
  foreach (BlockItem *bi, blockItems)
    bi->makeWritable();
  foreach (SheetScene *s, sheets)
    s->fancyTitleItem()->makeWritable();
}

int EntryScene::startPage() const {
  return data_->startPage();
}

QString EntryScene::title() const {
  return data_->title()->current()->text();
}

bool EntryScene::isWritable() const {
  return writable;
}

void EntryScene::newFootnote(int block, QString tag) {
  ASSERT(block>=0 && block<blockItems.size());
  foreach (FootnoteItem *fni, blockItems[block]->footnotes()) {
    if (fni->data()->tag()==tag) {
      fni->setFocus();
      return;
    }
  }
  FootnoteData *fnd = new FootnoteData(blockItems[block]->data());
  fnd->setTag(tag);
  FootnoteItem *fni = blockItems[block]->newFootnote(fnd);
  fni->makeWritable();
  fni->sizeToFit();
  restackBlocks(block);
  gotoSheetOfBlock(block);
  fni->setAutoContents();
  fni->setFocus();
}

EntryData *EntryScene::data() const {
  return data_;
}

GfxNoteItem *EntryScene::createNote(QPointF scenePos, int sheet) {
  qDebug() << "EntryScene::createNote Not properly?";
  TitleItem *ti = sheets[sheet]->fancyTitleItem();
  GfxNoteItem *note
    = ti->createNote(ti->mapFromScene(scenePos));
  if (note)
    note->data()->setSheet(scenePos.y()/style().real("page-height"));
  return note;
  
}

GfxNoteItem *EntryScene::newNote(int sheet,
				 QPointF scenePos1, QPointF scenePos2) {
  if (scenePos2.isNull())
    scenePos2 = scenePos1;
  TitleItem *ti = sheets[sheet]->fancyTitleItem();
  GfxNoteItem *note = ti->newNote(ti->mapFromScene(scenePos1),
				  ti->mapFromScene(scenePos2));
  if (note)
    note->data()->setSheet(scenePos1.y()/style().real("page-height"));
  return note;  
}

void EntryScene::clipPgNoAt(int n) {
  firstDisallowedPgNo = n;
}

int EntryScene::clippedPgNo(int n) const {
  if (firstDisallowedPgNo>0 && n>=firstDisallowedPgNo)
    return firstDisallowedPgNo - 1;
  else
    return n;
}

QString EntryScene::pgNoToString(int n) const {
  int n0 = clippedPgNo(n);
  if (n0 == n)
    return QString::number(n);
  else
    return QString("%1%2").arg(n0).arg(QChar('a'+n-n0-1));
}

QList<BlockItem const *> EntryScene::blocks() const {
  QList<BlockItem const *> bb;
  foreach (BlockItem *b, blockItems)
    bb << b;
  return bb;
}

QList<FootnoteItem const *> EntryScene::footnotes() const {
  QList<FootnoteItem const *> ff;
  foreach (BlockItem *bi, blockItems)
    foreach (FootnoteItem *f, bi->footnotes())
      ff << f;
  return ff;
}

void EntryScene::gotoSheetOfBlock(int n) {
  if (n<0)
    emit sheetRequest(0);
  else if (n>=blockItems.size())
    emit sheetRequest(nSheets-1);
  else
    emit sheetRequest(blockItems[n]->data()->sheet());
}
