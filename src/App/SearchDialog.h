// SearchDialog.H

#ifndef SEARCHDIALOG_H

#define SEARCHDIALOG_H

#include <QObject>
#include <QPointer>
#include "PageView.h"

class SearchDialog: public QObject {
  Q_OBJECT;
public:
  SearchDialog(class PageView *parent);
  virtual ~SearchDialog();
public slots:
  void newSearch();
private slots:
  void gotoPage(int n, Qt::KeyboardModifiers, QString uuid, QString phrase);
private:
  QPointer<PageView> pgView;
  QString lastPhrase;
};

#endif
