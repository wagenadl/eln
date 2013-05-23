// App/Navbar.cpp - This file is part of eln

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

// Navbar.C

#include "Navbar.H"
#include "ToolItem.H"

#define NAV_TOC "toc"
#define NAV_PREV "prev"
#define NAV_NEXT "next"
#define NAV_P10 "p10"
#define NAV_N10 "n10"
#define NAV_END "end"

Navbar::Navbar(QGraphicsItem *parent): Toolbar(parent) {
  disableSelect();
  
  ToolItem *t = new ToolItem();
  t->setSvg(":icons/nav-toc.svg");
  addTool(NAV_TOC, t);

  t = new ToolItem();
  t->setSvg(":icons/nav-p10.svg");
  addTool(NAV_P10, t);

  t = new ToolItem();
  t->setSvg(":icons/nav-prev.svg");
  addTool(NAV_PREV, t);

  t = new ToolItem();
  t->setSvg(":icons/nav-next.svg");
  addTool(NAV_NEXT, t);

  t = new ToolItem();
  t->setSvg(":icons/nav-n10.svg");
  addTool(NAV_N10, t);

  t = new ToolItem();
  t->setSvg(":icons/nav-end.svg");
  addTool(NAV_END, t);
}


Navbar::~Navbar() {
}

void Navbar::doLeftClick(QString s) {
  if (s==NAV_TOC)
    emit goTOC();
  else if (s==NAV_P10)
    emit goRelative(-10);
  else if (s==NAV_PREV)
    emit goRelative(-1);
  else if (s==NAV_NEXT)
    emit goRelative(1);
  else if (s==NAV_N10)
    emit goRelative(10);
  else if (s==NAV_END)
    emit goEnd();
}