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

#include "libmscore/mscore.h"
#include "libmscore/element.h"  // for Element class
#include "libmscore/barline.h"  // for BarLine class
using EType  = Ms::ElementType; // it gets used a lot, Type consts are long too
using BLType = Ms::BarLineType; // for convenience, and consistency w/EType

using CueMap   = QMap<QString, const Ms::Element*>;
using CueMulti = QMultiMap<QString, const Ms::Element*>;
using Type2Cue = QMap<EType, CueMulti>;

using StrPtrList      = QList<QString*>;
using StrPtrVect      = QVector<QString*>;
using StrPtrListList  = QList<StrPtrList*>;
using StrPtrVectList  = QList<StrPtrVect*>;
using StrPtrListVect  = QVector<StrPtrList*>;
using StrPtrVectVect  = QVector<StrPtrVect*>;
using StrPtrListVectList = QList<StrPtrListVect*>;
using StrPtrListVectVect = QVector<StrPtrListVect*>;
using Str2IntMap      = QMap<QString, int>;
using Str2RealMap     = QMap<QString, qreal>;
using BoolVect        = QVector<bool>;
using RealVect        = QVector<qreal>;
using RealList        = QList<qreal>;
using RealListVect    = QVector<RealList>;
using RealPair        = std::pair<qreal, qreal>;
using IntVect         = QVector<int>;
using IntVectList     = QList<IntVect*>;
using IntList         = QList<int>;
using IntListList     = QList<IntList*>;
using IntListVect     = QVector<IntList*>;
using IntListVectList = QList<IntListVect*>;
using IntListVectVect = QVector<IntListVect*>;
using Int2StrMap      = QMap<int, QString>;
using Int2BoolMap     = std::map<int, bool>;
using Int2IntMap      = std::map<int, int>;
using Int2DblMap      = std::map<int, double>;
using Int2RealMap     = QMap<int, qreal>;
using IntSet          = std::set<int>;
using IntPair         = std::pair<int, int>;
using IntPairSet      = std::set<IntPair>;

///////////////////////////////////////////////////////////////////////////////
// SVG and SMAWS constants

// SVG floating point precision - if >8k monitors become the norm, increase it.
#define SVG_PRECISION 2

// SVG chars/strings as constants
// Chars
#define SVG_QUOTE     '"'
#define SVG_COMMA     ','
#define SVG_DASH      '-'
#define SVG_SEMICOLON ';'
#define SVG_HASH      '#'
#define SVG_PERCENT   '%'
#define SVG_DOLLARS   '$'
#define SVG_ASTERISK  '*'
#define SVG_RPAREN    ')'
#define SVG_GT        '>'
#define SVG_LT        '<'
#define SVG_ZERO      '0'
#define SVG_ONE       "1"
#define SVG_SPACE     ' '

// Strings
#define SVG_2SPACES      "  "
#define SVG_3SPACES      "   "
#define SVG_4SPACES      "    "
#define SVG_8SPACES      "        "
#define SVG_ELEMENT_END  "/>"
#define SVG_RPAREN_QUOTE ")\""

// SVG elements
#define SVG_BEGIN       "<svg"
#define SVG_END         "</svg>"

#define SVG_TITLE_BEGIN "<title>"
#define SVG_TITLE_END   "</title>"
#define SVG_DESC_BEGIN  "<desc>"
#define SVG_DESC_END    "</desc>"
#define SVG_DEFS_BEGIN  "<defs>\n"
#define SVG_DEFS_END    "</defs>\n"
#define SVG_GROUP_BEGIN "<g"
#define SVG_GROUP_END   "</g>"
#define SVG_TEXT_BEGIN  "<text"
#define SVG_TEXT_END    "</text>"
#define SVG_USE_END     "</use>"

#define SVG_USE         "<use"
#define SVG_LINE        "<line"
#define SVG_RECT        "<rect"
#define SVG_PATH        "<path"
#define SVG_POLYLINE    "<polyline"
#define SVG_IMAGE       "<image"

// SVG element attributes
#define SVG_VIEW_BOX    " viewBox=\""
#define SVG_XYMIN_SLICE " preserveAspectRatio=\"xMinYMin slice\""
#define SVG_XYMIN_MEET  " preserveAspectRatio=\"xMinYMin meet\""
#define SVG_POINTER     " pointer-events=\""
#define SVG_CURSOR      " cursor=\"default\""  // avoids <text> I-Beam cursor

#define SVG_WIDTH  " width=\""
#define SVG_HEIGHT " height=\""
#define SVG_AUTO   "auto\""

#define SVG_X     " x="  // No quote char due to floating point formatting
#define SVG_Y     " y="  // ditto
#define SVG_X1_NQ " x1=" // ditto
#define SVG_X2_NQ " x2=" // ditto
#define SVG_Y1_NQ " y1=" // ditto
#define SVG_Y2_NQ " y2=" // ditto
#define SVG_X1    " x1=\""
#define SVG_X2    " x2=\""
#define SVG_Y1    " y1=\""
#define SVG_Y2    " y2=\""
#define SVG_RX    " rx=\""
#define SVG_RY    " ry=\""

#define XLINK_HREF " xlink:href=\"#"
#define SVG_CLASS  " class=\""
#define SVG_ID     " id=\""

#define SVG_FILL         " fill=\""
#define SVG_FILL_URL     " fill=\"url(#"
#define SVG_FILL_RULE    " fill-rule=\"evenodd\"" // rarely used, default is ok
#define SVG_FILL_OPACITY " fill-opacity=\""

#define SVG_STROKE            " stroke=\""
#define SVG_STROKE_URL        " stroke=\"url(#"
#define SVG_STROKE_WIDTH      " stroke-width=\""
#define SVG_STROKE_OPACITY    " stroke-opacity=\""
#define SVG_STROKE_LINECAP    " stroke-linecap=\""
#define SVG_STROKE_LINEJOIN   " stroke-linejoin=\""
#define SVG_STROKE_DASHARRAY  " stroke-dasharray=\""
#define SVG_STROKE_DASHOFFSET " stroke-dashoffset=\""
#define SVG_STROKE_MITERLIMIT " stroke-miterlimit=\""

#define SVG_VECTOR_EFFECT " vector-effect=\"non-scaling-stroke\""

#define SVG_FONT_FAMILY " font-family=\""
#define SVG_FONT_SIZE   " font-size=\""

#define SVG_POINTS " points=\""
#define SVG_D      " d=\""
#define SVG_M      'M' // Move
#define SVG_L      'L' // Line
#define SVG_C      'C' // Curve
#define SVG_H      'H' // Horizontal line
#define SVG_V      'V' // Vertical line
#define SVG_Z      'Z' // Close path

#define SVG_MATRIX    " transform=\"matrix("
#define SVG_TRANSFORM " transform=\""
#define SVG_TRANSLATE "translate("
#define SVG_SCALE     "scale("

// SVG element attribute values
#define SVG_PX      "px"
#define SVG_NONE    "none"
#define SVG_VISIBLE "visible"
#define SVG_EVENODD "evenodd"
#define SVG_BUTT    "butt"
#define SVG_SQUARE  "square"
#define SVG_ROUND   "round"
#define SVG_MITER   "miter"
#define SVG_BEVEL   "bevel"
#define SVG_BLACK   "#000000"

// For extended characters in MScore font (unicode Private Use Area)
#define XML_ENTITY_BEGIN "&#x"
#define XML_ENTITY_END   ';'

// Boilerplate header text
#define XML_STYLE_MUSE "<?xml-stylesheet type=\"text/css\" href=\"/SMAWS/MuseScore.svg.css\"?>\n"
#define XML_STYLE_GRID "<?xml-stylesheet type=\"text/css\" href=\"/SMAWS/SMAWS_Grid.svg.css\"?>\n<?xml-stylesheet type=\"text/css\" href=\"/SMAWS/SMAWS_Grid.psu.css\"?>\n"
#define XML_NAMESPACE  " xmlns=\"http://www.w3.org/2000/svg\""
#define XML_XLINK      "     xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
#define VTT_HEADER     "WEBVTT\n\nNOTE\n    SMAWS  - Sheet Music Animation w/Sound -\n    This file links to one or more SVG files via the\n    cue ids, which are in this format: 0000000_1234567\nNOTE\n\n"
#define VTT_START_ONLY "WEBVTT\n\nNOTE\n    SMAWS  - Sheet Music Animation w/Sound -\n    This file links to one or more SVG files via the\n    cue ids, which are integer MIDI tick values formatted variable-length\nNOTE\n\n"
#define VTT_MIXED      "WEBVTT\n\nNOTE\n    SMAWS  - Sheet Music Animation w/Sound -\n    This file links to one or more SVG files via the\n    cue ids, which are in two formats:\n    1) fixed 7-digit start_end ticks: 0000000_1234567\n    2) variable-length tick values, start time only\nNOTE\n\n"
#define HTML_HEADER    "<!DOCTYPE html>\n<!-- SMAWS HTML Tables -->\n<html>\n<head>\n    <meta charset=\"utf-8\">\n    <meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\n    <link rel=\"stylesheet\" href=\"../SMAWS_22.css\">\n</head>\n\n<body onload=\"onLoadHTMLTables()\">\n\n"

// Boilerplate events
#define SVG_ONCLICK    " onclick=\"musicClick(evt)\""
#define SVG_ONLOAD     " onload=\"onLoadSVGGrid(evt)\""
#define SVG_TOP_ONLOAD " onload=\"top.onLoadSVGGrid(evt)\""

// SMAWS
#define SMAWS         "SMAWS"
#define SMAWS_VERSION "2.3"

// Custom SVG attributes (and some default settings)
#define SVG_SCROLL     " data-scroll=\""  // "x" or "y", horizontal or vertical
#define SVG_STAVES     " data-staves=\""  // number of staves for the score (OBSOLETE?)
#define SVG_STAFFLINES " data-lines=\""   // number of staff lines for the part
#define SVG_CUE        " data-cue=\""     // the cue id
#define SVG_CUE_NQ     " data-cue="       // the cue id with no quote char
#define SVG_COL_CUE    " data-col-cue="   // the relative cue id within a grid page (start tick only)
#define SVG_START      " data-start=\""   // cue start time in milliseconds
#define SVG_START_NQ   " data-start="     // cue start time in milliseconds with no quote character
#define SVG_INAME      " data-iname=\""   // full instrument name == MuseScore "short" instrument name
#define SVG_BARNUMB    " data-barnumb="   // measure (bar) number for rulers/counters - no quotes!!
#define SVG_BOTTOM     " data-bottom=\""  // vertical scroll: staff bottom y

#define SVG_PREFIX_TAB "tab" // For tablature class names

// SMAWS class attribute values
#define CLASS_CLEF_COURTESY "ClefCourtesy"
#define CLASS_CURSOR        "cursor HiScore" // always hiLited
#define CLASS_GRAY          "bgFill LoScore"
#define CLASS_NOTES         "notes"
#define CLASS_TABS          "tablature"
#define CLASS_GRID          "grid"
#define CLASS_TITLE         "title"
#define CLASS_INSTRUMENT    "instrument"
#define CLASS_BRACKET_LINK  "bracketLink"
#define CLASS_INAME_LINK    "iNameLink"
#define CLASS_INAME_NOTE    "iNameNote"
#define CLASS_INAME_TABS    "iNameTabs"
#define CLASS_LYRICS        "lyrics"

// Miscellaneous SMAWS constants
#define CUE_ID_FIELD_WIDTH 7
#define CUE_ID_ZERO  "0000000_0000000"
#define TEXT_BPM     "bpm"
#define NATURAL_SIGN 57953  // 0xE261, natural signs excluded from frozen panes
#define FROZEN_WIDTH 535    // frozen-staff-lines:x2; fader-rect:width;
#define RULER_HEIGHT 47
#define INAME_OFFSET  4     //!!third of 12px, which is default font size - alignment-baseline!!
#define CLEF_OFFSET  16
#define STAFF_GRID   "grid" // Yes, it's the same as CLASS_GRID, but they serve different roles, STAFF_GRID is used outside of saveSMAWS_Tables() too.
#define PICK_DOWN    "d"
#define PICK_UP      "u"
#define PICK_NO      "n"
#define ID_STAVES    "Staves"
// Imaginary MIDI note values for rests and invisible "cells" in SVG "tables"
#define MIDI_REST  -1
#define MIDI_EMPTY -2

//#define SVG_HI     " data-hi=\"#0000bb\"" // medium-bright blue
//#define SVG_LO     " data-lo=\"#000000\"" // black
//#define SVG_STAFF  " data-staff=\""       // staff index for this element
//#define SVG_TEMPO  " data-tempo="
//#define BPS2BPM 60 // Beats per Second to Beats per Minute conversion factor
//#define SVG_COMMENT_BEGIN  "<!--"
//#define SVG_COMMENT_END    "-->"

// HTML constants for SMAWS Tables
#define HTML_BEGIN       "<html"
#define HTML_END         "</html>"
#define HTML_BODY_BEGIN  "<body"
#define HTML_BODY_END    "</body>"
#define HTML_TABLE_BEGIN "<table"
#define HTML_TABLE_END   "</table>"
#define HTML_COL_BEGIN   "<col"
#define HTML_TR_BEGIN    "<tr"
#define HTML_TR_END      "</tr>"
#define HTML_TH_BEGIN    "<th"
#define HTML_TH_END      "</th>"
#define HTML_TD_BEGIN    "<td"
#define HTML_TD_END      "</td>"
#define HTML_COLSPAN     " colspan=\""

#define UNICODE_DOT "&#x1D16D;"

// This array links TDuration::DurationType to unicode characters down to 128th
static const char32_t durationUnicode[] = {
    0x1D1B7, // V_LONG
    0x1D15C, // V_BREVE
    0x1D15D, // V_WHOLE
    0x1D15E, // V_HALF
    0x2669,  // V_QUARTER 16-bit, more fonts available for these two chars
    0x266A,  // V_EIGHTH  16-bit, ditto
    0x1D161, // V_16TH
    0x1D162, // V_32ND
    0x1D163, // V_64TH
    0x1D164  // V_128TH
};

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
    void setScrollVertical(bool isVertical);
    bool isScrollVertical();
    void setNonStandardStaves(QVector<int>* nonStdStaves);
    void setNStaves(int n);
    void setStaffLines(int n);
    void setStaffIndex(int idx);
    void setCursorTop(qreal top);
    void setCursorHeight(qreal height);
    void setStartMSecs(int start);
    void freezeIt(int idxStaff);
    void frozenClefs(int tick, bool b);
    void streamDefs();
    void streamBody();
    void beginMultiGroup(QStringList* pINames, QStringList *pFullNames, const QString& className, int height, int top);
    void beginMouseGroup();
    void beginGroup(int indent = 0, bool isFrozen = false);
    void endGroup  (int indent = 0, bool isFrozen = false);
    void setYOffset(qreal y);
    void createMultiUse(qreal y);
    void setLeftRight(qreal left, qreal right);
};

#endif // QSVGGENERATOR_H
