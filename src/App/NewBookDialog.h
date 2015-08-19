// NewBookDialog.h

#ifndef NEWBOOKDIALOG_H

#define NEWBOOKDIALOG_H

#include <QDialog>

class NewBookDialog: public QDialog {
public:
  NewBookDialog(QWidget *parent=0);
  virtual ~NewBookDialog();
public:
  QString location() const;
  bool hasArchive() const;
  bool isRemote() const;
  QString remoteHost() const;
  QString archiveLocation() const;
private slots:
  void browse();
  void abrowse();
  void localToggled(bool);
  void remoteToggled(bool);
  void locationChanged(QString);
private:
  class Ui_newBookDialog *ui;
};

#endif