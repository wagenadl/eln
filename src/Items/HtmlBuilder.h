// HtmlBuilder.h

#ifndef HTMLBUILDER_H

#define HTMLBUILDER_H

#include <QString>
#include "MarkupData.h"
#include <QVector>
#include <QList>
#include "MarkupStyles.h"

class HtmlBuilder {
public:
  HtmlBuilder(class TextData const *src, int start=0, int end=-1);
  QString toHtml() const { return html; }
private:
  static QString openingTag(MarkupData::Style);
  static QString closingTag(MarkupData::Style);
  static QString tagName(MarkupData::Style);
private:
  QVector<int> nowedges;
  QVector<MarkupStyles> nowstyles;
  QList<MarkupData::Style> stack;
  int start;
  int end;
  QString text;
  QString html;
};

#endif