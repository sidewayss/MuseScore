/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtSvg module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SVGGENERATOR_H
#define SVGGENERATOR_H

#include <QtGui/qpaintdevice.h>

#include <QtCore/qnamespace.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qscopedpointer.h>

#include "libmscore/element.h"   // for Element class
using EType = Ms::Element::Type; // It get used a lot, Type consts are long too
using SVGMap = QMultiMap<QString, const Ms::Element*>; // (SMAWS) A convenience

///////////////////////////////////////////////////////////////////////////////
// SVG and SMAWS constants

// SVG floating point precision
#define SVG_PRECISION 2

// SVG strings as constants
#define SVG_QUOTE    '"'
#define SVG_COMMA    ','
#define SVG_DOT      '.'
#define SVG_DASH     '-'
#define SVG_GT       '>'
#define SVG_ZERO     '0'
#define SVG_ONE      "1"
#define SVG_SPACE    ' '
#define SVG_4SPACES  "    "
#define SVG_8SPACES  "        "
#define SVG_PX       "px"
#define SVG_NONE     "none"
#define SVG_EVENODD  "evenodd"
#define SVG_BUTT     "butt"
#define SVG_SQUARE   "square"
#define SVG_ROUND    "round"
#define SVG_MITER    "miter"
#define SVG_BEVEL    "bevel"
#define SVG_BLACK    "#000000"

#define SVG_BEGIN    "<svg"
#define SVG_END      "</svg>"

#define SVG_WIDTH    " width=\""
#define SVG_HEIGHT   " height=\""
#define SVG_VIEW_BOX " viewBox=\""

#define SVG_X        " x="
#define SVG_Y        " y="

#define SVG_X1       " x1=\""
#define SVG_X2       " x2=\""
#define SVG_Y1       " y1=\""
#define SVG_Y2       " y2=\""

#define SVG_RX       " rx=\"1\"" // for now these are constant values
#define SVG_RY       " ry=\"1\"" // only used once for rehearsal mark rects

#define SVG_POINTS   " points=\""
#define SVG_D        " d=\""
#define SVG_M        'M' // Move
#define SVG_L        'L' // Line
#define SVG_C        'C' // Curve

#define SVG_ELEMENT_END "/>"

#define SVG_DEFS_BEGIN  "<defs>\n"
#define SVG_DEFS_END    "</defs>\n"
#define SVG_TITLE_BEGIN "<title>"
#define SVG_TITLE_END   "</title>"
#define SVG_DESC_BEGIN  "<desc>"
#define SVG_DESC_END    "</desc>"

#define SVG_GROUP_BEGIN "<g"
#define SVG_GROUP_END   "</g>"
#define SVG_TEXT_BEGIN  "<text"
#define SVG_TEXT_END    "</text>"

#define SVG_IMAGE       "<image"
#define SVG_PATH        "<path"
#define SVG_POLYLINE    "<polyline"
#define SVG_LINE        "<line"
#define SVG_RECT        "<rect"
#define SVG_USE         "<use"

#define SVG_CLASS       " class=\""
#define SVG_ID          " id=\""
#define XLINK_HREF      " xlink:href=\"#"

#define SVG_PRESERVE_XYMIN_SLICE " preserveAspectRatio=\"xMinYMin slice\""

#define SVG_POINTER_EVENTS " pointer-events=\"visible\""
#define SVG_CURSOR         " cursor=\"default\""  // to avoid pesky I-Beam cursor

#define SVG_FILL           " fill=\""
#define SVG_FILL_RULE      " fill-rule=\"evenodd\""
#define SVG_FILL_OPACITY   " fill-opacity=\""
#define SVG_OPACITY        " opacity=\""
#define SVG_STROKE_OPACITY " stroke-opacity=\""
#define SVG_STROKE         " stroke=\""
#define SVG_STROKE_WIDTH   " stroke-width=\""
#define SVG_STROKE_LINECAP " stroke-linecap=\""
#define SVG_STROKE_LINEJOIN " stroke-linejoin=\""
#define SVG_STROKE_DASHARRAY " stroke-dasharray=\""
#define SVG_STROKE_DASHOFFSET " stroke-dashoffset=\""
#define SVG_STROKE_MITERLIMIT " stroke-miterlimit=\""

#define SVG_VECTOR_EFFECT  " vector-effect=\"non-scaling-stroke\""

#define SVG_FONT_FAMILY    " font-family=\""
#define SVG_FONT_SIZE      " font-size=\""

#define SVG_TRANSLATE      " transform=\"translate("
#define SVG_MATRIX         " transform=\"matrix("
#define SVG_TRANSFORM_END  ")\""

// For extended characters in MScore font (unicode Private Use Area)
#define XML_ENTITY_BEGIN "&#x"
#define XML_ENTITY_END   ';'

// Boilerplate header text
#define XML_NAMESPACE  " xmlns=\"http://www.w3.org/2000/svg\"\n"
#define XML_STYLESHEET "<?xml-stylesheet type=\"text/css\" href=\"MuseScore.svg.css\"?>\n"
#define XML_XLINK      "     xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
#define VTT_HEADER     "WEBVTT\n\nNOTE\n    SMAWS  - Sheet Music Animation w/Sound -\n    This file links to one or more SVG files via the\n    cue ids, which are in this format: 0000000_1234567\nNOTE\n\n";

// Boilerplate onclick() event
#define SVG_ONCLICK " onclick=\"top.clickMusic(evt)\""

#define SMAWS "SMAWS" // SMAWS

// Custom SVG attributes (and some default settings)
#define SVG_ATTR   " data-attr=\"fill\""  // the only animated attribute so far
#define SVG_SCROLL " data-scroll=\""      // "x" or "y", horizontal or vertical
#define SVG_STAVES " data-staves=\""      // number of staves for the score
#define SVG_CUE    " data-cue=\""         // the cue id
#define SVG_START  " data-start=\""       // cue start time in milliseconds
#define SVG_INAME  " data-iname=\""       // full instrument name == MuseScore "short" instrument name

#define CLASS_CLEF_COURTESY "ClefCourtesy"
#define CLASS_CURSOR        "cursor"
#define CLASS_GRAY          "gray"

#define NATURAL_SIGN 57953 // 0xE261, natural signs excluded from frozen panes
#define CUE_ID_ZERO "0000000_0000000"

//#define SVG_HI     " data-hi=\"#0000bb\"" // medium-bright blue
//#define SVG_LO     " data-lo=\"#000000\"" // black
//#define SVG_STAFF  " data-staff=\""       // staff index for this element
//#define SVG_TEMPO  " data-tempo="
//#define BPS2BPM 60 // Beats per Second to Beats per Minute conversion factor
//#define SVG_COMMENT_BEGIN  "<!--"
//#define SVG_COMMENT_END    "-->"
///////////////////////////////////////////////////////////////////////////////

class SvgGeneratorPrivate;

//---------------------------------------------------------
//   @@ SvgGenerator
//   @P size          QSize
//   @P viewBox       QRectF
//   @P title         QString
//   @P description   QString
//   @P fileName      QString
//   @P outputDevice  QIODevice
//   @P resolution    int
//---------------------------------------------------------

class SvgGenerator : public QPaintDevice
{
    Q_DECLARE_PRIVATE(SvgGenerator)

    Q_PROPERTY(QSize size READ size WRITE setSize)
    Q_PROPERTY(QRectF viewBox READ viewBoxF WRITE setViewBox)
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QString description READ description WRITE setDescription)
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName)
    Q_PROPERTY(QIODevice* outputDevice READ outputDevice WRITE setOutputDevice)
    Q_PROPERTY(int resolution READ resolution WRITE setResolution)
public:
    SvgGenerator();
    ~SvgGenerator();

    QString title() const;
    void setTitle(const QString &title);

    QString description() const;
    void setDescription(const QString &description);

    QSize size() const;
    void setSize(const QSize &size);

    QRect viewBox() const;
    QRectF viewBoxF() const;
    void setViewBox(const QRect &viewBox);
    void setViewBox(const QRectF &viewBox);

    QString fileName() const;
    void setFileName(const QString &fileName);

    QIODevice *outputDevice() const;
    void setOutputDevice(QIODevice *outputDevice);

    void setResolution(int dpi);
    int resolution() const;
protected:
    QPaintEngine *paintEngine() const;
    int metric(QPaintDevice::PaintDeviceMetric metric) const;

private:
    QScopedPointer<SvgGeneratorPrivate> d_ptr;

public:
    void setElement(const Ms::Element* e);
    void setSMAWS();
    void setCueID(const QString& qs);
    void setScrollAxis(bool axis);
    void setNStaves(int n);
    void setStaffIndex(int idx);
    void setCursorTop(qreal top);
    void setCursorHeight(qreal height);
    void setStartMSecs(int start);
    void freezeIt(int idxStaff);
    void streamDefs();
    void streamBody();
    void beginMultiGroup(QStringList* pINames, const QString& fullName, qreal height, qreal top);
    void endMultiGroup();
    void setYOffset(qreal y);
    void createMultiUse(const QString& qs, qreal y);
};

#endif // QSVGGENERATOR_H
