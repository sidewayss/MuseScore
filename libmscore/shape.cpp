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

#include "shape.h"
#include "segment.h"

namespace Ms {

//---------------------------------------------------------
//   translate
//---------------------------------------------------------

void Shape::translate(const QPointF& pt)
      {
      for (QRectF& r : *this)
            r.translate(pt);
      }

//---------------------------------------------------------
//   draw
//    draw outline of shape
//---------------------------------------------------------

void Shape::draw(QPainter* p) const
      {
      p->save();
      p->setPen(QPen(QBrush(Qt::darkYellow), 0.2));
      p->setBrush(QBrush(Qt::NoBrush));
      for (const QRectF& r : *this)
            p->drawRect(r);
      p->restore();
      }

//---------------------------------------------------------
//   create
//---------------------------------------------------------

void Shape::create(int staffIdx, Segment* s)
      {
      clear();
      for (int voice = 0; voice < VOICES; ++voice) {
            Element* e = s->element(staffIdx * VOICES + voice);
            if (e) {
                  e->layout();
                  add(e->shape());
                  }
            }
      }

//-------------------------------------------------------------------
//   minHorizontalDistance
//    a is located right of this shape.
//    Calculates the minimum vertical distance between the two shapes
//    so they dont touch.
//-------------------------------------------------------------------

qreal Shape::minHorizontalDistance(const Shape& a) const
      {
      qreal dist = 0.0;
      for (const QRectF& r2 : a) {
            qreal by1 = r2.top();
            qreal by2 = r2.bottom();
            for (const QRectF& r1 : *this) {
                  qreal ay1 = r1.top();
                  qreal ay2 = r1.bottom();
                  if ((ay1 >= by1 && ay1 < by2) || (ay2 >= by1 && ay2 < by2) || (ay1 <= by1 && ay2 >by2))
                        dist = qMax(dist, r1.right() - r2.left());
                  }
            }
      return dist;
      }

//-------------------------------------------------------------------
//   minVerticalDistance
//    a is located below of this shape.
//    Calculates the minimum distance between the two shapes
//    so they dont touch.
//-------------------------------------------------------------------

qreal Shape::minVerticalDistance(const Shape& a) const
      {
      qreal dist = 0.0;
      for (const QRectF& r1 : a) {
            for (const QRectF& r2 : *this) {
                  if (r1.intersects(r2)) {
                        QRectF r3 = r1.intersected(r2);
                        if (r3.height() > dist)
                              dist = r3.height();
                        }
                  }
            }
      return dist;
      }

//---------------------------------------------------------
//   left
//    compute left border
//---------------------------------------------------------

qreal Shape::left() const
      {
      qreal left = 0.0;
      for (const QRectF& r : *this) {
            if (r.left() < left)
                  left = r.left();
            }
      return left;
      }

//---------------------------------------------------------
//   right
//    compute left border
//---------------------------------------------------------

qreal Shape::right() const
      {
      qreal right = 0.0;
      for (const QRectF& r : *this) {
            if (r.right() > right)
                  right = r.right();
            }
      return right;
      }


#ifdef DEBUG_SHAPES

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void Shape::dump(const char* p) const
      {
      printf("Shape dump: %p %s size %d\n", this, p, size());
      for (const QRectF& r : *this) {
            printf("   %f %f %f %f\n", r.x(), r.y(), r.width(), r.height());
            }

      }

//---------------------------------------------------------
//   testShapes
//---------------------------------------------------------

void testShapes()
      {
      printf("======test shapes======\n");

      //=======================
      //    minDistance()
      //=======================
      Shape a;
      Shape b;
      a.add(QRectF(-10, -10, 20, 20));
      qreal d = a.minHorizontalDistance(b);           // b is empty
      printf("      minDistance (0.0): %f", d);
      if (d != 0.0)
            printf("   =====error");
      printf("\n");

      b.add(QRectF(0, 0, 10, 10));
      d = a.minHorizontalDistance(b);
      printf("      minDistance (10.0): %f", d);
      if (d != 10.0)
            printf("   =====error");
      printf("\n");
      }
#endif // DEBUG_SHAPES


} // namespace Ms

