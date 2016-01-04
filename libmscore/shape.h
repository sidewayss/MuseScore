//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2016 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#ifndef __SHAPE_H__
#define __SHAPE_H__

namespace Ms {

//---------------------------------------------------------
//   Shape
//---------------------------------------------------------

class Shape : public QList<QRectF> {
   public:
      Shape() {}
      void draw(QPainter*);
      };


} // namespace Ms

#endif

