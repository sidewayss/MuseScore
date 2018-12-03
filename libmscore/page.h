//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2002-2011 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#ifndef __PAGE_H__
#define __PAGE_H__

#include "config.h"
#include "element.h"
#include "bsp.h"

namespace Ms {

class System;
class Text;
class Measure;
class XmlWriter;
class Score;
class MeasureBase;

//--------------------------------------------------------------------
//   PageFormat Units (named Unit singular because of QPageSize::Unit)
//--------------------------------------------------------------------
enum class Unit : char {
	MM = 0, // Millimeters, for printing
	PT,     // Points @72DPI for font sizes and spatium storage
	INCH,   // Inches, for printing
	P,      // Picas,  for future typesetting use
	DD,     // Didot,  for future French typesetting use
	C,      // Cicero, for future European typesetting use
	CM,     // Centimeters, for future use
	PX,     // Pixels, for SVG exports and internal use, same as points @360DPI
	SP      // Staff Spaces
};
// For converting from Ms::Units to a two-char string used as a suffix
static const char TRANSLATE_CTX_UNITS[] = "unitSuffix";
extern const char* unitSuffixes[];

//---------------------------------------------------------
//   @@ Page
//   @P pagenumber int (read only)
//---------------------------------------------------------

class Page final : public Element {
      QList<System*> _systems;
      int _no;                      // page number
#ifdef USE_BSP
      BspTree bspTree;
      void doRebuildBspTree();
#endif
      bool bspTreeValid;

      QString replaceTextMacros(const QString&) const;
      void drawHeaderFooter(QPainter*, int area, const QString&) const;

   public:
      Page(Score*);
      ~Page();
      virtual Page* clone() const           { return new Page(*this); }
      virtual ElementType type() const      { return ElementType::PAGE; }
      const QList<System*>& systems() const { return _systems;   }
      QList<System*>& systems()             { return _systems;   }
      System* system(int idx)               { return _systems[idx];   }

      virtual void write(XmlWriter&) const;
      virtual void read(XmlReader&);

      void appendSystem(System* s);

      int no() const                     { return _no;        }
      void setNo(int n)                  { _no = n;           }
      bool isOdd() const;
      qreal tm() const;            // margins in pixel
      qreal bm() const;
      qreal lm() const;
      qreal rm() const;

      virtual void draw(QPainter*) const override;
      virtual void scanElements(void* data, void (*func)(void*, Element*), bool all=true) override;

      QList<Element*> items(const QRectF& r);
      QList<Element*> items(const QPointF& p);
      void rebuildBspTree()   { bspTreeValid = false; }
      QPointF pagePos() const { return QPointF(); }     ///< position in page coordinates
      QList<Element*> elements();               ///< list of visible elements
      QRectF tbbox();                           // tight bounding box, excluding white space
      int endTick() const;
      };



}     // namespace Ms
#endif
