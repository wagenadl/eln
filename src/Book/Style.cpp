// Book/Style.cpp - This file is part of eln

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

// Style.C

#include "Style.H"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QColor>
#include "JSONParser.H"
#include "Assert.H"

Style const &Style::defaultStyle() {
  static Style s;
  return s;
}

Style::Style() {
  QFile f(":/style.json");
  ASSERT(f.open(QFile::ReadOnly));
  QTextStream ts(&f);
  ts.setCodec("UTF-8");
  JSONParser p(ts.readAll());
  try {
    options_ = p.readObject();
    p.assertEnd();
  } catch (JSONParser::Error e) {
    e.report();
    qFatal("style error");
  }
}

Style::Style(QString fn) {
  QFile f(fn);
  if (f.open(QFile::ReadOnly)) {
    QTextStream ts(&f);
    ts.setCodec("UTF-8");
    JSONParser p(ts.readAll());
    try {
      options_ = p.readObject();
      p.assertEnd();
    } catch (JSONParser::Error e) {
      e.report();
      qFatal("style error");
    }
  } else {
    qDebug() << "Style: File not found: " << fn;
  }

  // fill in missing values from default style
  Style const &s0 = defaultStyle();
  foreach (QString k, s0.options_.keys()) 
    if (!options_.contains(k))
      options_[k] = s0[k];
}

QVariant Style::operator[](QString k) const {
  if (options_.contains(k))
    return options_[k];
  qDebug() << "Style: No value for " << k;
  ASSERT(0);
  return QVariant();
}

double Style::real(QString k) const {
  return (*this)[k].toDouble();
}

QString Style::string(QString k) const {
  return (*this)[k].toString();
}

bool Style::flag(QString k) const {
  return (*this)[k].toBool();
}

int Style::integer(QString k) const {
  return (*this)[k].toInt();
}

QFont Style::font(QString k) const {
  QFont f(string(k + "-family"));
  f.setPointSizeF(real(k + "-size"));
  return f;
}

QColor Style::color(QString k) const {
  QColor c(string(k));
  if (!c.isValid()) {
    qDebug() << "Not a valid color: " << k << string(k);
    ASSERT(0); // this is not actually fatal, so don't crash unless debug
  }
  return c;    
}

QVariantMap const &Style::options() const {
  return options_;
}

bool Style::contains(QString k) const {
  return options_.contains(k);
}
