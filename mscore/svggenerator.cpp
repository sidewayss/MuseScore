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

#include "svggenerator.h"
#include "paintengine_p.h"

// SMAWS includes
#include "libmscore/score.h"     // for Score::nstaves()
#include "libmscore/tempotext.h" // for TempoText class
#include "libmscore/clef.h"      // for ClefType/ClefInfo
#include "libmscore/keysig.h"
#include "libmscore/key.h"
#include "libmscore/barline.h"   // for BarLine class
#include "libmscore/mscore.h"    // for BarLineType enum
#include "libmscore/staff.h"     // for Tablature staves
#include "libmscore/text.h"      // for Measure numbers
using BLType = Ms::BarLineType;  // for convenience, and consistency w/EType

static void translate_color(const QColor &color, QString *color_string,
                            QString *opacity_string)
{
    Q_ASSERT(color_string);
    Q_ASSERT(opacity_string);

    *color_string =
        QString::fromLatin1("#%1%2%3")
        .arg(color.red(),   2, 16, QLatin1Char('0'))
        .arg(color.green(), 2, 16, QLatin1Char('0'))
        .arg(color.blue(),  2, 16, QLatin1Char('0'));
    *opacity_string = QString::number(color.alphaF());
}

static void translate_dashPattern(QVector<qreal> pattern, const qreal& width, QString *pattern_string)
{
    Q_ASSERT(pattern_string);

    // Note that SVG operates in absolute lengths, whereas Qt uses a length/width ratio.
    foreach (qreal entry, pattern)
        *pattern_string += QString::fromLatin1("%1,").arg(entry * width);

    pattern_string->chop(1);
}

class SvgPaintEnginePrivate : public QPaintEnginePrivate
{
public:
    SvgPaintEnginePrivate()
    {
        size    = QSize();
        viewBox = QRectF();

        outputDevice = 0;
        resolution   = Ms::DPI;

        attributes.title = QLatin1String("");
        attributes.desc  = QLatin1String("");
    }
    QSize  size;
    QRectF viewBox;
    int    xDigits;
    int    yDigits;

    QIODevice* outputDevice;
    int        resolution;

    struct _attributes {
        QString title;
        QString desc;
    } attributes;

    QString      header;
    QString      defs;
    QString      body;
    QTextStream* stream;
};

static inline QPaintEngine::PaintEngineFeatures svgEngineFeatures()
{
    return QPaintEngine::PaintEngineFeatures(
           QPaintEngine::AllFeatures
        & ~QPaintEngine::PatternBrush
        & ~QPaintEngine::PerspectiveTransform
        & ~QPaintEngine::ConicalGradientFill
        & ~QPaintEngine::PorterDuff);
}

class SvgPaintEngine : public QPaintEngine
{
    friend class SvgGenerator; // for setElement()

    Q_DECLARE_PRIVATE(SvgPaintEngine)

private:
// It gets used more than once
   QTextCodec* _codec;

// Qt translates everything. These help avoid SVG transform="translate()".
    qreal _dx;
    qreal _dy;

// For UpdateState() and the drawXXX() functions.
    QString classState; // all elements
    QString styleState; // only: Non-<text>-elements-not-styled-by-CSS

// Set by updateState(), used various places
    QString  _classValue;

// In updateState() fill/stroke (brush/pen) attributes are switched for text
    QString _color;
    QString _colorOpacity;

// For centering text in frames. The frame is drawn first, before the text.
    QRectF _textFrame;

////////////////////
// for Frozen Pane:
//
    using FDef  = QMap<QString, StrPtrList*>;       // Key    = idxStaff-EType
    using FDefs = QMap<QString, FDef*>; //by cue_id // Values = <text> elements

    Str2IntMap   frozenWidths;
    FDefs        frozenDefs;
    QFile        frozenFile;
    qreal        _xLeft;      // StaffLines left x-coord, for element alignment

    QString getFrozenElement(const QString& textContent,
                             const QString& classValue,
                             const qreal x,
                             const qreal y);
//
////////////////////

// Gets the SVG class attribute for an element
    QString getClass();

// Most streams share these basic settings
    void initStream(QTextStream* stream);

// For fancy text formatting inside the SVG file
    void    streamXY(qreal x, qreal y);
    QString fixedFormat(const QString& attr,
                        const qreal    n,
                        const int      maxDigits,
                        const bool     inQuotes);

protected:
    const Ms::Element* _e;  // The Ms::Element being generated now
          EType        _et; // That element's ::Type - it's used everywhere

////////////////////
// SMAWS only:
    QString _cue_id;           // The current VTT cue ID
    bool    _isSMAWS;          // In order to use SMAWS code only when necessary
    bool    _isScrollVertical; // Only 2 axes: x = false, y = true.
    bool    _isMulti;          // Multi-Select Staves file formats/formatting
    bool    _isFullMatrix;     // A full matrix requires different handling of the y-offset for Multi-Select Staves
    qreal   _cursorTop;        // For calculating the height of (vertical bar)
    qreal   _cursorHeight;     // Sheet music playback position cursor.
    qreal   _yOffset;          // Y axis offset used by Multi-Select Staves
///!!!OBSOLETE!!!    int     _startMSecs;       // The elements start time in milliseconds -Yes, it kind of duplicates _cue_id, but it serves a different purpose for now. an oddly important yet minor kludge.
    int     _maxNote;          // The max duration for notes in this score (in ticks)

////////////////////
// for Frozen Pane:
//
    bool _isFrozen; // Is _e part of a frozen pane?
    int  _nStaves;  // Number of staves in the current score
    int  _idxStaff; // The current staff index
    int  _idxGrid;  // The grid staff index

    QVector<int>* _nonStdStaves; // Vector of staff indices for the tablature and percussion staves in this score

    // These vary by Staff:
    StrPtrVect   frozenLines; // vector by staff, frozen pane staff/system lines
    RealListVect frozenKeyY;  // vector by staff, list = y-coords left-to-right
    RealListVect frozenTimeY; // vector by staff, list = y-coords top-to-bot
    RealVect   yLineKeySig;   // vector by staff, clef's start "staff line" for first accidental (range = 0-9 top-to-bottom for 5-line staff)
    RealVect yOffsetKeySig;   // vector by staff, non-zero if clef changes

    FDef*        _prevDef;  // The previous def,    used by freezeDef()
    QString      _prevCue;  // The previous cue_id, used by freezeDef()
    QStringList* _iNames;   // Multi-Select Staves: list of instrument names
    QStringList  _multiUse; // ditto: list of <use> element text starters

    // These only vary by cue_id, not by Staff
    Str2RealMap xOffsetTimeSig; // TimeSig x-coord varies by KeySig, and thus by cue_id.

    // Streams the beginning of a frozen pane <def>
    void beginDef(const int idx, const QString& cue_id);

    // Completes open FDef in frozenDefs. Called by SvgGenerator::freezeIt().
    void freezeDef(int idxStaff);

    // Completes keysig and timesig defs. Called by freezeDef()
    void freezeSig(FDef* def, int idx, RealListVect& frozenY, EType eType, qreal x);

    // Encapsulates a frequently used line of code (with variables)
    QString getDefKey(int idx, EType eType);

    // Switch the target string for stream().
    // Called by SvgGenerator functions of the same name.
    void streamDefs() {d_func()->stream->setString(&(d_func()->defs));
                       initStream(d_func()->stream);}
    void streamBody() {d_func()->stream->setString(&(d_func()->body));
                       initStream(d_func()->stream);}

////////////////////
// for Multi-Select Staves:
//
    // Begin and End Multi-Select Staves group element.
    // Called by SvgGenerator functions of the same name.
    void beginMultiGroup(const QString& iName, const QString& fullName, const QString& className, qreal height, qreal top, const QString& cues)
    {
        *d_func()->stream << SVG_GROUP_BEGIN
                          << SVG_TRANSFORM << SVG_TRANSLATE << SVG_ZERO
                          << SVG_SPACE     << top           << SVG_RPAREN_QUOTE
                          << SVG_HEIGHT    << height        << SVG_QUOTE
                          << SVG_ID        << iName         << SVG_QUOTE
                          << SVG_CLASS     << className     << SVG_QUOTE
                          << SVG_INAME     << fullName      << SVG_QUOTE
                          << cues          << SVG_GT        << endl;
        if (fullName == STAFF_GRID)
            _idxGrid = _iNames->size() - 1;
    }

    void endMultiGroup() {*d_func()->stream << SVG_GROUP_END << endl;}

    // Streams the <use> elements for Multi-Select Staves frozen pane file only
    void createMultiUse(qreal y) {
        _multiUse.append(QString("%1%2")
                          .arg(SVG_USE)
                          .arg(fixedFormat(SVG_Y, y, d_func()->yDigits, true)));
    }
//
////////////////////

public:
    SvgPaintEngine()
        : QPaintEngine(*new SvgPaintEnginePrivate,
                       svgEngineFeatures())
    {
        _codec = QTextCodec::codecForName("UTF-8");

        _e  = NULL;
        _et = Ms::Element::Type::INVALID;

        _prevDef       = 0;   // FDef*
        _iNames        = 0;   // QStringList*
        _nonStdStaves  = 0;   // QVector<int>*
        _nStaves       = 0;   // int
        _idxGrid       = -1;  // int
        _maxNote       = 0.0; // int
        _xLeft         = 0.0; // qreal
        _cursorTop     = 0.0; // qreal
        _cursorHeight  = 0.0; // qreal
        _yOffset       = 0.0; // qreal

        _isScrollVertical = false;
        _isMulti          = false;
        _isSMAWS          = false;
    }

    bool begin(QPaintDevice *device);
    bool end();

    void updateState(const QPaintEngineState &state);

    const QString qpenToSvg(const QPen &spen);
    const QString qbrushToSvg(const QBrush &sbrush);

    void drawTextItem(const QPointF &p, const QTextItem &textItem);
    void drawPath(const QPainterPath &path);
    void drawPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode);

    void drawPixmap(const QRectF &r, const QPixmap &pm, const QRectF &sr);
    void drawImage(const QRectF &r, const QImage &pm, const QRectF &sr, Qt::ImageConversionFlag = Qt::AutoColor);

    QPaintEngine::Type type() const { return QPaintEngine::SVG; }

    inline QTextStream &stream()    { return *d_func()->stream; }

    QSize size() const { return d_func()->size; }
    void setSize(const QSize &size) {
        d_func()->size = size;
    }
    QRectF viewBox() const { return d_func()->viewBox; }
    void setViewBox(const QRectF &viewBox) {
        d_func()->viewBox = viewBox;
        // These are for fancy fixed-width formattting
        d_func()->xDigits = QString::number(qRound(viewBox.width())).size();
        d_func()->yDigits = QString::number(qRound(viewBox.height())).size();
    }
    QString documentTitle() const { return d_func()->attributes.title; }
    void setDocumentTitle(const QString &title) {
        d_func()->attributes.title = title;
    }
    QString documentDescription() const { return d_func()->attributes.desc; }
    void setDocumentDescription(const QString &description) {
        d_func()->attributes.desc = description;
    }
    QIODevice *outputDevice() const { return d_func()->outputDevice; }
    void setOutputDevice(QIODevice *device) {
        Q_ASSERT(!isActive());
        d_func()->outputDevice = device;
    }
    int resolution() { return d_func()->resolution; }
    void setResolution(int resolution) {
        d_func()->resolution = resolution;
    }
};

///////////////////////////
// SvgPaintEngine::begin()
///////////////////////////
bool SvgPaintEngine::begin(QPaintDevice *)
{
    Q_D(SvgPaintEngine);

    // Check for errors
    if (!d->outputDevice) {
        qWarning("SvgPaintEngine::begin(), no output device");
        return false;
    }
    if (!d->outputDevice->isOpen()) {
        if (!d->outputDevice->open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning("SvgPaintEngine::begin(), could not open output device: '%s'",
                     qPrintable(d->outputDevice->errorString()));
            return false;
        }
    }
    else if (!d->outputDevice->isWritable()) {
        qWarning("SvgPaintEngine::begin(), could not write to read-only output device: '%s'",
                 qPrintable(d->outputDevice->errorString()));
        return false;
    }

    // Initialize the <defs> string
    d->defs = SVG_DEFS_BEGIN;

    // Initialize the stream, for other functions to populate
    d->stream = new QTextStream(&d->body);
    initStream(d->stream);

    // SMAWS - but can't check _isSMAWS because it isn't set yet:
    // Set this flag to default value
    _isScrollVertical = false;

    return true;
}

/////////////////////////
// SvgPaintEngine::end()
/////////////////////////
bool SvgPaintEngine::end()
{
    Q_D(SvgPaintEngine);

    _isFrozen = _isSMAWS && !_isScrollVertical;

    // Stream the headers
    stream().setString(&d->header);

    // The reason for waiting to stream the headers until end() is to set the
    // scroll axis and check _isMulti [this used to happen in begin()].
    const QString scrollAxis = _isScrollVertical ? "y" : "x";

    if (_isSMAWS) // SMAWS uses an external CSS file
        stream() << XML_STYLE_MUSE;

    // The <svg> element:
    stream() << SVG_BEGIN
                << XML_NAMESPACE << (_isFrozen ? XML_XLINK : "") << SVG_4SPACES
                << SVG_VIEW_BOX  << d->viewBox.left()            << SVG_SPACE
                                 << d->viewBox.top()             << SVG_SPACE
                                 << d->viewBox.width()           << SVG_SPACE
                                 << d->viewBox.height()          << SVG_QUOTE
                << SVG_WIDTH     << d->size.width()              << SVG_QUOTE
                << SVG_HEIGHT    << d->size.height()             << SVG_QUOTE
                << endl;
    if (_isSMAWS) {
        stream()<< SVG_4SPACES   << SVG_PRESERVE_XYMIN_SLICE << SVG_CURSOR << endl
                << SVG_4SPACES   << SVG_POINTER_EVENTS       << endl
                << SVG_4SPACES   << SVG_CLASS  << SMAWS      << SVG_QUOTE
                                 << SVG_SCROLL << scrollAxis << SVG_QUOTE
                                 << SVG_ATTR
                                 << SVG_MAX    << _maxNote   << SVG_QUOTE;
        if (_isMulti)
            stream()             << SVG_STAVES << _nStaves   << SVG_QUOTE;
        else
            stream()             << _cue_id; // ??? is this still needed? it's for gray-out cues, right? Here it is, a full data-cue="" string of comma-separated cue ids
    }

    stream() << SVG_GT << endl  // Document attributes:
             << SVG_TITLE_BEGIN << d->attributes.title << SVG_TITLE_END << endl
             << SVG_DESC_BEGIN  << d->attributes.desc  << SVG_DESC_END  << endl;


    if (_isSMAWS) { // Cursor, Gray, Fade rects at the end of the current body
        stream().setString(&d->body);
        stream() << SVG_RECT
                    << SVG_CLASS          << CLASS_CURSOR  << SVG_QUOTE
                    << SVG_X << SVG_QUOTE << "-5"          << SVG_QUOTE
                    << SVG_Y << SVG_QUOTE << _cursorTop    << SVG_QUOTE
                    << SVG_WIDTH          << SVG_ZERO      << SVG_QUOTE
                    << SVG_HEIGHT         << _cursorHeight << SVG_QUOTE
                 << SVG_ELEMENT_END << endl;

        // Two gray-out <rect>s (left/right) for graying out inactive bars
        for (int i = 0; i < 2; i++)
            stream() << SVG_RECT
                        << SVG_CLASS          << CLASS_GRAY  << SVG_QUOTE
                        << SVG_X << SVG_QUOTE << SVG_ZERO    << SVG_QUOTE
                        << SVG_Y << SVG_QUOTE << SVG_ZERO    << SVG_QUOTE
                        << SVG_WIDTH          << SVG_ZERO    << SVG_QUOTE
                        << SVG_HEIGHT << d->viewBox.height() << SVG_QUOTE
                        << SVG_FILL_OPACITY   << SVG_ZERO    << SVG_QUOTE
                     << SVG_ELEMENT_END << endl;

        if (_isMulti) // Terminate the Staves group
            stream() << SVG_GROUP_END << endl;

        // The fader <rect> for crossfading between frozen and thawed
        stream() << SVG_RECT
                    << SVG_ID       << "Fader"             << SVG_QUOTE
                    << SVG_WIDTH    << FROZEN_WIDTH        << SVG_QUOTE
                    << SVG_HEIGHT   << d->viewBox.height() << SVG_QUOTE
                    << SVG_FILL_URL << "gradFader"         << SVG_RPAREN_QUOTE
                 << SVG_ELEMENT_END << endl;
    }

    // Deal with Frozen Pane, if it exists
    if (_isFrozen) {
        // Frozen body - _isFrozen depends on _isSMAWS, setString(&d->body) above
        int i;
        FDefs::iterator def;
        FDef::iterator  elms;
        const QString tempoKey = getDefKey(0, EType::TEMPO_TEXT);

        if (_isMulti) {
            // Frozen <use> elements by staff. SVG_GROUP_ consolidates events.
            //!!! FOR NOW THIS IS ALWAYS "top.funcName(evt)". The "top." should be an option somewhere, somehow.
            const QString frozenEvents = " onclick=\"top.frozenClick(evt)\" onmouseover=\"top.frozenOver(evt)\" onmouseout=\"top.frozenOut(evt)\" onmouseup=\"top.frozenUp(evt)\"";

            stream() << SVG_GROUP_BEGIN << frozenEvents << SVG_GT << endl;

            for (i = 0; i < _iNames->size(); i++)
                stream() << SVG_4SPACES << _multiUse[i]
                         << SVG_ID     << (*_iNames)[i] << SVG_QUOTE
                         << XLINK_HREF << (*_iNames)[i] << SVG_DOT << CUE_ID_ZERO << SVG_QUOTE
                         << SVG_ELEMENT_END << endl;

            stream() << SVG_GROUP_END << endl;
        }
        else {
            // StaffLines/SystemBarLine(s) once, in the body, not in the defs
            for (i = 0; i < _nStaves; i++)
                stream() << *(frozenLines[i]);

            // One <use> element
            stream() << SVG_USE    << XLINK_HREF      << CUE_ID_ZERO
                     << SVG_QUOTE  << SVG_ELEMENT_END << endl;
        }

        // Frozen defs
        stream().setString(&d->defs);
        // The Fader and FrozenLines gradients (!!!note literal 100 in x2 value for gradFrozenLines)
        stream() << "    <linearGradient id=\"gradFader\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\">\n        <stop stop-color=\"white\" stop-opacity=\"1\" offset=\"0.50\"/>\n        <stop stop-color=\"white\" stop-opacity=\"0\" offset=\"0.55\"/>\n    </linearGradient>\n";
        stream() << "    <linearGradient id=\"gradFrozenLines\" x1=\"0\" y1=\"0\" x2=\"100\" y2=\"0\" gradientUnits=\"userSpaceOnUse\">\n        <stop stop-color=\"black\" stop-opacity=\"1\" offset=\"0.50\"/>\n        <stop stop-color=\"black\" stop-opacity=\"0\" offset=\"0.55\"/>\n    </linearGradient>\n";

        // Iterate by cue_id
        for (def = frozenDefs.begin(); def != frozenDefs.end(); ++def) {
            int     i;
            int     idxStaff  = -1;
            int     idxStd    = -1;
            bool    hasKeySig = false;
            QString cue_id    = def.key();
            FDef*   value     = def.value();

            // Iterate by staff
            // elms.key() == QString(idx-EType) elms.value() == QList<QString*>*
            for (elms = value->begin(); elms != value->end(); ++elms) {
                const int idx = elms.key().section(SVG_DASH, 0, 0).toInt();

                if (idxStaff < 0 || (_isMulti && idxStaff != idx)) {
                    // Close previous group if _isMulti == true && _nStaves > 1
                    if (idxStaff > -1)
                        stream() << SVG_4SPACES << SVG_GROUP_END << endl;

                    beginDef(idx, cue_id);
                    idxStaff = idx;

                    if (!_isMulti) // Non-multi tempo, once per def, not per staff
                        stream() << *((*(*value)[tempoKey])[0]);
                }
                if (_isMulti || elms.key() != tempoKey) {
                    // Iterate by element type, clef, keysig, etc.
                    for (i = 0; i < elms.value()->size();  i++) {
                        // Stream the intrument name, clef, keysig, or timesig
                        stream() << *((*elms.value())[i]);

                        // For non-standard staves: (see the bottom of the outer for loop)
                        if (!hasKeySig && elms.key().section(SVG_DASH, 1, 1).toInt() == (int)EType::KEYSIG) {
                            hasKeySig = true;
                            idxStd = idx; // reference staff index for a standard staff
                        }
                    }
                }
            }
            stream() << SVG_4SPACES << SVG_GROUP_END << endl;

            // Percussion and tablature staves do not have key signatures, but
            // they do have time sigs, and those should be aligned with the
            // time sigs in the other staves. When a key sig changes, and the
            // time sig moves horizontally in all the standard notation staves,
            // the time sig must also move in the non-standard staves. But
            // those staves don't have any change at this cue_id. All the clef
            // and key sig activity is in the standard staves exclusively.
            // This code finds the places where this happens and creates defs
            // with the proper time sig x-coord for the non-standard staves.
            // It assumes that non-standard staves never have clef changes.
            if (hasKeySig && _nonStdStaves != 0 && _nonStdStaves->size() > 0 && cue_id != CUE_ID_ZERO) {
                for (i = 0; i < _nonStdStaves->size(); i++) {
                    QString timeKey = getDefKey(i, EType::TIMESIG);
                    if ((*frozenDefs[cue_id])[timeKey] != 0 && (*frozenDefs[cue_id])[timeKey]->size() > 0) {
                        beginDef((*_nonStdStaves)[i], cue_id);

                        if ((*_nonStdStaves)[i] != _idxGrid) // Slashes-only "grid" staff has no clef either (it should be a percussion staff)
                            stream() << *((*(*frozenDefs[CUE_ID_ZERO])[getDefKey((*_nonStdStaves)[i], EType::CLEF)])[0]); // only one element for clefs

                        const StrPtrList* spl = (*frozenDefs[cue_id])[getDefKey(idxStd, EType::TIMESIG)];
                        for (int j = 0; j < spl->size(); j++)
                            stream() << *((*spl)[j]); // 1 or 2 elements for timesig, possibly more

                        stream() << *((*(*frozenDefs[CUE_ID_ZERO])[getDefKey((*_nonStdStaves)[i], EType::INSTRUMENT_NAME)])[0]); // only one element for instrument name - this does not handle instrument name changes!!! not easy to do, as I would need to completely rebuild this svg element text using the cue_id start tick - or keep track of the last def-cue_id for this staff, which is a bummer.

                        stream() << SVG_4SPACES << SVG_GROUP_END << endl;
                    }
                }
            }
        } // end for(each frozen def)
    } //if(Frozen Pane)

    // Point the stream at the real output device (the .svg file)
    stream().setDevice(d->outputDevice);
    stream().setCodec(_codec);
    initStream(&(stream()));

    // Stream our strings out to the device, in order: header, defs, body
    stream() << d->header;

    if (d->defs != SVG_DEFS_BEGIN)
        stream() << d->defs << SVG_DEFS_END;

    // Multi wraps all the staves in an outer group for simpler x-axis scrolling
    // The group is terminated in the frozen pane code a few lines up from here
    if (_isMulti)
        stream() << SVG_GROUP_BEGIN  << SVG_ID << "Staves" << SVG_QUOTE << SVG_GT << endl;

    stream() << d->body;

    // </svg>\n
    stream() << SVG_END << endl;

    // Clean up
    delete &(stream());

    return true;
}

/////////////////////////////////
// SvgPaintEngine::updateState()
/////////////////////////////////
void SvgPaintEngine::updateState(const QPaintEngineState &state)
{
    QTextStream qts;

    // Is this element part of a frozen pane?
    _isFrozen =  _isSMAWS
             && !_isScrollVertical
             && (_et == EType::TEMPO_TEXT
              || _et == EType::INSTRUMENT_NAME
              || _et == EType::INSTRUMENT_CHANGE
              || _et == EType::CLEF
              || _et == EType::KEYSIG
              || _et == EType::TIMESIG
              || _et == EType::STAFF_LINES
              ||(_et == EType::BAR_LINE && _e->parent()->type() == EType::SYSTEM));

    // classState = class + optional data-cue + transform attributes
    classState.clear();
    // styleState = all other attributes, only for elements NOT styled by CSS
    styleState.clear();

    // SVG class attribute, based on Ms::Element::Type, among other things
    qts.setString(&classState);
    initStream(&qts);

    qts << SVG_CLASS;
    _classValue = getClass();
    if ((_cue_id.isEmpty() || _et == EType::BAR_LINE) && !(_isMulti && _idxStaff == _nStaves)) {
        // No cue id or BarLine = no fancy formatting
        qts << _classValue << SVG_QUOTE;
        if (!_cue_id.isEmpty())
            // But bar lines need cue ids
            qts << SVG_CUE << _cue_id << SVG_QUOTE;
    }
    else {
        // First stream the class attribute, with fancy fixed formatting
        int w;
        if (_isMulti && _idxStaff == _nStaves)
            w = 14;            // "System" staff: RehearsalMark" = 14
        else if (_isFrozen)
            w = 17;            // Frozen:      InstrumentChange" = 17
        else
            w = 11;            // Highlight:         Accidental" = 11
        qts.setFieldWidth(w);
        qts << QString("%1%2").arg(_classValue).arg(SVG_QUOTE);
        qts.setFieldWidth(0);
        // Then stream the Cue ID
        if (!_cue_id.isEmpty())
            qts << SVG_CUE << _cue_id << SVG_QUOTE;
    }

    // Translations, SVG transform="translate()", are handled separately from
    // other transformations such as rotation. Qt translates everything, but
    // other transformations occur rarely. They are included in classState
    // because they affect CSS-styled elements too.
    // For the elements that need transformations other than translation
    QTransform t = state.transform();

    // Tablature Note Text:
    // These 2 have floating point flotsam, for example: 1.000000629
    // Both values should be integer 1, because no scaling is intended.
    // This rounds to three decimal places, as MuseScore does elsewhere.
    const qreal m11 = qRound(t.m11() * 1000) / 1000.0;
    const qreal m22 = qRound(t.m22() * 1000) / 1000.0;

    if ((m11 == 1 && m22 == 1 && t.m12() == t.m21()) // No scaling, no rotation
    || _classValue == CLASS_CLEF_COURTESY) {         // All courtesy clefs
        // No transformation except translation
        _dx = t.m31();
        _dy = t.m32();
        _isFullMatrix = false;
    }
    else {
        // Other transformations are more straightforward with a full matrix
        _dx = 0;
        _dy = 0;
        _isFullMatrix = true;
        qts << SVG_MATRIX << t.m11() << SVG_COMMA
                          << t.m12() << SVG_COMMA
                          << t.m21() << SVG_COMMA
                          << t.m22() << SVG_COMMA
                          << t.m31() << SVG_COMMA
                          << t.m32() + _yOffset << SVG_RPAREN_QUOTE;
    }

    // Set attributes for element types not styled by CSS
    switch (_et) {
    case EType::ACCIDENTAL         :
    case EType::ARTICULATION       :
    case EType::BEAM               :
    case EType::BRACKET            :
    case EType::CLEF               :
    case EType::GLISSANDO_SEGMENT  :
    case EType::HARMONY            :
    case EType::HOOK               :
    case EType::INSTRUMENT_CHANGE  :
    case EType::INSTRUMENT_NAME    :
    case EType::KEYSIG             :
    case EType::LEDGER_LINE        :
    case EType::LYRICS             :
    case EType::LYRICSLINE_SEGMENT :
    case EType::NOTE               :
    case EType::NOTEDOT            :
    case EType::REHEARSAL_MARK     :
//  case EType::REST : // Rest <polyline>s can't be handled in CSS. Rest <text> handled in drawTextItem().
    case EType::SLUR_SEGMENT       :
    case EType::STAFF_LINES        :
    case EType::STEM               :
    case EType::SYSTEM             :
    case EType::TEXT               :
    case EType::TIMESIG            :
    case EType::TREMOLO            :
    case EType::TUPLET             :
        break; // Styled by CSS
    default:
        // The best way so far to style NORMAL bar lines in CSS w/o messing up
        // other types' styling, which may or may not be plausible in CSS
        if (_et != EType::BAR_LINE || static_cast<const Ms::BarLine*>(_e)->barLineType()
                != BLType::NORMAL) {
            // Brush and Pen attributes only affect elements NOT styled by CSS
            qts.setString(&styleState);
            qts << qbrushToSvg(state.brush());
            qts <<   qpenToSvg(state.pen());
        }
        break;
    }
}

//////////////////////////////
// SvgPaintEngine::qpenToSVG()
//////////////////////////////
const QString SvgPaintEngine::qpenToSvg(const QPen &spen)
{
    QString     qs;
    QTextStream qts(&qs);
    initStream(&qts);

    // Set stroke, stroke-dasharray, stroke-dashoffset attributes
    switch (spen.style()) {
    case Qt::NoPen:
        return qs; // Default value for stroke = "none" = Qt::NoPen = NOOP;
        break;

    case Qt::SolidLine:
    case Qt::DashLine:
    case Qt::DotLine:
    case Qt::DashDotLine:
    case Qt::DashDotDotLine:
    case Qt::CustomDashLine: {
        // These values are class variables because they are needed by
        // drawTextItem(). This is the fill color/opacity for text.
        translate_color(spen.color(), &_color, &_colorOpacity);

        // default stroke="none" is handled by case Qt::NoPen above
        qts << SVG_STROKE << _color << SVG_QUOTE;

        // stroke-opacity is seldom used, usually set to default 1
        if (_colorOpacity != SVG_ONE)
            qts << SVG_STROKE_OPACITY << _colorOpacity << SVG_QUOTE;

        // If it's a solid line, were done for now
        if (spen.style() == Qt::SolidLine)
            break;

        // It's a dashed line
        qreal penWidth = spen.width() == 0 ? qreal(1) : spen.widthF();

        QString dashPattern, dashOffset;
        translate_dashPattern(spen.dashPattern(), penWidth, &dashPattern);
        dashOffset = QString::number(spen.dashOffset() * penWidth); // SVG uses absolute offset

        qts << SVG_STROKE_DASHARRAY  << dashPattern << SVG_QUOTE;
        qts << SVG_STROKE_DASHOFFSET << dashOffset  << SVG_QUOTE;
        break; }
    default:
        qWarning("Unsupported pen style");
        break;
    }
    // Set stroke-width attribute, unless it's zero or 1 (default is 1)
    if (spen.widthF() > 0 && spen.widthF() != 1)
        qts << SVG_STROKE_WIDTH << spen.widthF() << SVG_QUOTE;

    // Set stroke-linecap attribute
    switch (spen.capStyle()) {
    case Qt::FlatCap:
        // This is the default stroke-linecap value
        //qts << SVG_STROKE_LINECAP << SVG_BUTT << SVG_QUOTE;
        break;
    case Qt::SquareCap:
        qts << SVG_STROKE_LINECAP << SVG_SQUARE << SVG_QUOTE;
        break;
    case Qt::RoundCap:
        qts << SVG_STROKE_LINECAP << SVG_ROUND << SVG_QUOTE;
        break;
    default:
        qWarning("Unhandled cap style");
        break;
    }
    // Set stroke-linejoin, stroke-miterlimit attributes
    switch (spen.joinStyle()) {
    case Qt::MiterJoin:
    case Qt::SvgMiterJoin:
        qts << SVG_STROKE_LINEJOIN   << SVG_MITER         << SVG_QUOTE
            << SVG_STROKE_MITERLIMIT << spen.miterLimit() << SVG_QUOTE;
        break;
    case Qt::BevelJoin:
        qts << SVG_STROKE_LINEJOIN   << SVG_BEVEL << SVG_QUOTE;
        break;
    case Qt::RoundJoin:
        qts << SVG_STROKE_LINEJOIN   << SVG_ROUND << SVG_QUOTE;
        break;
    default:
        qWarning("Unhandled join style");
        break;
    }
    // An uncommon, possibly non-existent in MuseScore, effect
    if (spen.isCosmetic())
        qts << SVG_VECTOR_EFFECT;

    return qs;
}

/////////////////////////////////
// SvgPaintEngine::qbrushToSVG()
/////////////////////////////////
const QString SvgPaintEngine::qbrushToSvg(const QBrush &sbrush)
{
    QString     qs;
    QTextStream qts(&qs);
    initStream(&qts);

    QString color, colorOpacity;

    switch (sbrush.style()) {
    case Qt::SolidPattern:
        translate_color(sbrush.color(), &color, &colorOpacity);

        // Default fill color is black
        if (color != SVG_BLACK)
            qts << SVG_FILL << color << SVG_QUOTE;

        // Default fill-opacity is 100%
        if (colorOpacity != SVG_ONE)
            qts << SVG_FILL_OPACITY << colorOpacity << SVG_QUOTE;

        break;

    case Qt::NoBrush:
        qts << SVG_FILL << SVG_NONE <<  SVG_QUOTE;
        break;

    default:
       break;
    }
    return qs;
}

/////////////////////
// drawXXX functions
/////////////////////
void SvgPaintEngine::drawPixmap(const QRectF &r, const QPixmap &pm, const QRectF &sr)
{
    drawImage(r, pm.toImage(), sr);
}

void SvgPaintEngine::drawImage(const QRectF &r, const QImage &image,
                               const QRectF &sr, Qt::ImageConversionFlag flags)
{
    Q_UNUSED(sr);
    Q_UNUSED(flags);

    const qreal yOff = _isFullMatrix ? 0 : _yOffset;

    if (_isMulti)
        stream() << SVG_4SPACES;

    stream() << SVG_IMAGE           << classState         << styleState
             << SVG_X << SVG_QUOTE  << r.x() + _dx        << SVG_QUOTE
             << SVG_Y << SVG_QUOTE  << r.y() + _dy + yOff << SVG_QUOTE
             << SVG_WIDTH           << r.width()          << SVG_QUOTE
             << SVG_HEIGHT          << r.height()         << SVG_QUOTE;

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QBuffer::ReadWrite);
    image.save(&buffer, "PNG");
    buffer.close();
    stream() << " xlink:href=\"data:image/png;base64,"
             << data.toBase64() << SVG_QUOTE << SVG_ELEMENT_END << endl;
}

void SvgPaintEngine::drawPath(const QPainterPath &p)
{
    const qreal yOff = _isFullMatrix ? 0 : _yOffset;

    if (_isMulti)
        stream() << SVG_4SPACES;

    if (_isSMAWS && _et == EType::REHEARSAL_MARK) {
        // Rehearsal mark frame is rect or circle, no need for a complex path.
        // I can't find a way to determine if it's a rect or a circle here, so
        // I hardcode to the rect style that I like. The size of the rect in
        // MuseScore is wrong to begin with, so this looks the best in the end.
        _textFrame.setX(_dx);
        _textFrame.setY(qMax(_dy - _e->height(), 2.0) + yOff);
        _textFrame.setWidth(qMax(_e->width() + 2, 16.0));
        _textFrame.setHeight(13);

        stream() << SVG_RECT    << classState << styleState;
        streamXY(_textFrame.x(),_textFrame.y());
        stream() << SVG_CUE     << _cue_id                   << SVG_QUOTE
                 << SVG_WIDTH   << _textFrame.width()        << SVG_QUOTE
                 << SVG_HEIGHT  << _textFrame.height()       << SVG_QUOTE
                 << SVG_RX      << SVG_ONE                   << SVG_QUOTE
                 << SVG_RY      << SVG_ONE                   << SVG_QUOTE
                 << SVG_ONCLICK << SVG_ELEMENT_END           << endl;
        return; // That's right, we're done here, no path to draw
    }

    // Not a text frame: draw an actual path. Stream class/style states.
    stream() << SVG_PATH << classState << styleState;

    // fill-rule is here because UpdateState() doesn't have a QPainterPath arg
    // Majority of <path>s use the default value: fill-rule="nonzero"
    switch (_et) {
    case EType::BEAM         :
    case EType::BRACKET      :
    case EType::SLUR_SEGMENT :
    case EType::TREMOLO      :
    case EType::NOTE         : // Tablature has rects behind numbers
        break;                // fill-rule styled by CSS
    default:
        if (p.fillRule() == Qt::OddEvenFill)
            stream() << SVG_FILL_RULE;
        break;
    }

    // Path data
    stream() << SVG_D;
    for (int i = 0; i < p.elementCount(); ++i) {
        const QPainterPath::Element &ppe = p.elementAt(i);
        qreal x = ppe.x + _dx;
        qreal y = ppe.y + _dy + yOff;
        switch (ppe.type) {
        case QPainterPath::MoveToElement:
            stream() << SVG_M << x << SVG_COMMA << y;
            break;
        case QPainterPath::LineToElement:
            stream() << SVG_L << x << SVG_COMMA << y;
            break;
        case QPainterPath::CurveToElement:
            stream() << SVG_C << x << SVG_COMMA << y;
            ++i;
            while (i < p.elementCount()) {
                const QPainterPath::Element &ppeCurve = p.elementAt(i);
                if (ppeCurve.type == QPainterPath::CurveToDataElement) {
                    stream() << SVG_SPACE << ppeCurve.x + _dx
                             << SVG_COMMA << ppeCurve.y + _dy + yOff;
                    ++i;
                }
                else {
                    --i;
                    break;
                }
            }
            break;
        default:
            break;
        }
        if (i <= p.elementCount() - 1)
            stream() << SVG_SPACE;
    }
    stream() << SVG_QUOTE << SVG_ELEMENT_END << endl;
}

void SvgPaintEngine::drawPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode)
{
    Q_ASSERT(pointCount >= 2);

    const qreal yOff = _isFullMatrix ? 0 : _yOffset;

    if (mode == PolylineMode) {
        if (_isMulti) stream() << SVG_4SPACES;

        stream() << SVG_POLYLINE << classState << styleState << SVG_POINTS;
        for (int i = 0; i < pointCount; ++i) {
            const QPointF &pt = points[i];

            stream() << pt.x() + _dx << SVG_COMMA << pt.y() + _dy + yOff;

            if (i != pointCount - 1)
                stream() << SVG_SPACE;

        }
        stream() << SVG_QUOTE << SVG_ELEMENT_END <<endl;

        // For Frozen Pane (horizontal scrolling only):
        // StaffLines and System BarLine(s)
        if (_isFrozen && _idxStaff != _idxGrid) {
            if (frozenLines[_idxStaff] == 0)
                frozenLines[_idxStaff] = new QString;

            const bool isStaffLines = (_et == EType::STAFF_LINES);

            const qreal x1 = points[0].x() + _dx;
            const qreal x2 = points[1].x() + _dx;
            const qreal y1 = points[0].y() + _dy + yOff;
            const qreal y2 = points[1].y() + _dy + yOff;

            QTextStream qts(frozenLines[_idxStaff]);
            initStream(&qts);

            if (_isMulti) qts << SVG_8SPACES;

            if (isStaffLines) {
                // Frozen pane staff lines must be <line>, not <polyline>,
                // due to CSS color management and user color control.
                qts << SVG_LINE
                       << SVG_CLASS      << "FrozenLines"     << SVG_QUOTE // a variation on StaffLines
                       << SVG_STROKE_URL << "gradFrozenLines" << SVG_RPAREN_QUOTE
                       << SVG_X1         << x1                << SVG_QUOTE
                       << SVG_Y1         << y1                << SVG_QUOTE
                       << SVG_X2         << FROZEN_WIDTH      << SVG_QUOTE
                       << SVG_Y2         << y2                << SVG_QUOTE
                    << SVG_ELEMENT_END        << endl;

                if (_xLeft == 0)
                    _xLeft = points[0].x() + _dx;
            }
            else {
                qts << SVG_POLYLINE  << SVG_CLASS;
                qts.setFieldWidth(11); // literal value: StaffLines" = 11
                qts << QString("%1%2").arg(_classValue).arg(SVG_QUOTE);
                qts.setFieldWidth(0);
                qts << SVG_POINTS
                    << fixedFormat("", x1, 3, false) << SVG_COMMA // literal value: frozen pane max width is 100
                    << fixedFormat("", y1, d_func()->yDigits, false)
                    << SVG_SPACE
                    << fixedFormat("", x2, 3, false) << SVG_COMMA // literal value: frozen pane max width is 100
                    << fixedFormat("", y2, d_func()->yDigits, false)
                    << SVG_QUOTE << SVG_ELEMENT_END << endl;
            }
        }
    }
    else { // not PolylineMode
        QPainterPath path(points[0]);
        for (int i = 1; i < pointCount; ++i)
            path.lineTo(points[i]);
        path.closeSubpath();
        drawPath(path);
    }
}

void SvgPaintEngine::drawTextItem(const QPointF &p, const QTextItem &textItem)
{
    // Just in case, this avoids crashes
    if (_e == NULL)
        return;

    // Variables, constants, initial setup
    qreal x = p.x() + _dx; // The de-translated coordinates
    qreal y = p.y() + _dy + (_isFullMatrix ? 0 : _yOffset);

    const QFont   font       = textItem.font();
    const QString fontFamily = font.family();
    const QString fontSize   = QString::number(font.pixelSize() != -1
                             ? font.pixelSize()
                             : font.pointSizeF());
    if (_isMulti)
        stream() << SVG_4SPACES;

    // Begin the <text>
    stream() << SVG_TEXT_BEGIN << classState;

    switch (_et) {
    case EType::ACCIDENTAL        :
    case EType::ARTICULATION      :
    case EType::CLEF              :
    case EType::GLISSANDO_SEGMENT :
    case EType::HARMONY           : // Chord text/symbols for song book, fake book, etc,
    case EType::HOOK              :
    case EType::INSTRUMENT_CHANGE :
    case EType::INSTRUMENT_NAME   :
    case EType::KEYSIG            :
    case EType::LYRICS            :
    case EType::NOTE              :
    case EType::NOTEDOT           :
    case EType::REST              :
    case EType::STAFF_TEXT        :
    case EType::TEMPO_TEXT        :
    case EType::TEXT              : // Measure numbers only, AFAIK
    case EType::TIMESIG           :
    case EType::TUPLET            :
        // These elements are all styled by CSS, no need to specify attributes,
        break;

    case EType::REHEARSAL_MARK :
        // This text is centered inside a rect or circle: x and y must relocate
        x = _textFrame.x() + (_textFrame.width()  / 2);
        y = _textFrame.y() + (_textFrame.height() - 3);
        break;

    default:
        // Attributes normally contained in styleState. updateState() swaps
        // the stroke/fill values in <text> elements; this is the remedy:
        if (_color != SVG_BLACK)
            stream() << SVG_FILL         << _color        << SVG_QUOTE;
        if (_colorOpacity != SVG_ONE)
            stream() << SVG_FILL_OPACITY << _colorOpacity << SVG_QUOTE;

        // The font attributes, not handled in updateState()
        stream() << SVG_FONT_FAMILY << fontFamily << SVG_QUOTE
                 << SVG_FONT_SIZE   << fontSize   << SVG_QUOTE;
        break;
    }

    // Stream the fancily formatted x and y coordinates
    streamXY(x, y);

    // These elements get onClick events, which requires pointer-events="visible"
    switch (_et) {
    case EType::NOTE           :
    case EType::NOTEDOT        :
    case EType::REST           :
    case EType::ACCIDENTAL     :
    case EType::STEM           :
    case EType::HOOK           :
    case EType::BEAM           :
    case EType::BAR_LINE       :
    case EType::REHEARSAL_MARK :
    case EType::LYRICS         :
    case EType::HARMONY        :
        stream() << SVG_ONCLICK << SVG_POINTER_EVENTS;
        break;
    case EType::TEXT :
        if (static_cast<const Ms::Text*>(_e)->textStyleType() == Ms::TextStyleType::MEASURE_NUMBER)
            stream() << SVG_ONCLICK << SVG_POINTER_EVENTS;
        break;
    default:
        break;
    }

    stream() << SVG_GT; // end attributes

    // The Content, as in: <text>Content</text>
    QString     textContent;
    QTextStream streamContent(&textContent);

    // Some tempo/instrument changes are invisible = no content here, instead
    // it's in the frozen pane file (see code below).
    if (_e->visible()) {
        if (fontFamily.left(6) == "MScore") { //???TODO: should this be for all extended, non-ascii chars?
            // MScore fonts are all Private Use Area unicode chars, nothing
            // alphanumeric, so it's best to render them as hex XML entities.
            // Most are one char per text element, so it lines up vertically.
            const QChar* data = textItem.text().constData();
            while (!data->isNull()) {
                streamContent << XML_ENTITY_BEGIN
                              << QString::number(data->unicode(), 16).toUpper()
                              << XML_ENTITY_END;
                ++data;
            }
        }
        else
            streamContent << textItem.text();
    }
    stream() << textContent << SVG_TEXT_END << endl;

    // Handle Frozen Pane elements
    if (_isFrozen) {
        const int     i   = _isMulti && _et == EType::TEMPO_TEXT ? _nStaves : _idxStaff;
        const QString key = getDefKey(i, _et);

        FDef*       def;
        QString     defClass;
        qreal       line;

        if (!frozenDefs.contains(_cue_id))
            frozenDefs[_cue_id] = new FDef;

        def      = frozenDefs[_cue_id];
        defClass = _classValue;

        // This switch() is MuseScore-draw-order dependent
        switch (_et) {
        case EType::INSTRUMENT_NAME   :
        case EType::INSTRUMENT_CHANGE :
            // No frozen class="InstrumentChange", only "InstrumentName"
            _et = EType::INSTRUMENT_NAME;
            defClass = _e->name(_et);
            x = 1;          // That's as far left as it goes
            break;

        case EType::TEMPO_TEXT :
            x = _xLeft;     // Left-aligns with System barline
            break;

        case EType::CLEF :
            x = _xLeft + 4; //!!! literal value: clefOffset

            // No frozen class="ClefCourtesy", only "Clef"
            defClass = _e->name(_et);

            // A change in clef w/o a change in keysig might require y-offset
            line = (qreal)(Ms::ClefInfo::lines(static_cast<const Ms::Clef*>(_e)->clefType())[0])
                 * 2.5; //!!!This is SPATIUM20 / 2, but I should really get the score's spatium

            // C/Am is the absence of a KeySig, so a score can start w/o a
            // KeySig. Not true for Clef of TimeSig. The first KeySig will
            // never require a translation. This is a y-axis offset.
            if (yLineKeySig[_idxStaff] != line && _cue_id != CUE_ID_ZERO)
                yOffsetKeySig[_idxStaff] = line
                                        - yLineKeySig[_idxStaff]
                                        + yOffsetKeySig[_idxStaff];
            yLineKeySig[_idxStaff] = line;
            break;

        case EType::KEYSIG :
            if (!def->contains(key) || (*def)[key]->size() == 0) {
                // First accidental in new KeySig:
                // Reset the list of frozen keysigs for this staff
                frozenKeyY[_idxStaff].clear();

                // Reset any Clef-induced y-axis offset
                yOffsetKeySig[_idxStaff] = 0;

                // The x-offset for the ensuing time signature is determined by the number of accidentals
                if (!xOffsetTimeSig.contains(_cue_id) && _e->staff()->isPitchedStaff())
                    xOffsetTimeSig.insert(_cue_id, qAbs((int)(static_cast<const Ms::KeySig*>(_e)->keySigEvent().key())) * 5); //!!! literal keysig-accidental-width
            }
            // Natural signs are not frozen
            if (*(textItem.text().unicode()) != NATURAL_SIGN)
                // Set the accidentals in left-to-right order in the vector
                frozenKeyY[_idxStaff].insert(0, y);
            break;

        case EType::TIMESIG :
            if (!def->contains(key)) {
                // First character in new TimeSig:
                // Reset the list of frozen timesigs for this staff
                frozenTimeY[_idxStaff].clear();
            }
            // Add the timesig character to the vector, order not an issue here
            frozenTimeY[_idxStaff].append(y);
            break;

        default:
            break;
        }

        QString* elm = new QString;
        QTextStream qts(elm);
        initStream(&qts);
        if (_idxStaff != _idxGrid || _et == EType::TIMESIG) {
            if (_et == EType::KEYSIG || _et == EType::TIMESIG)
                // KeySigs/TimeSigs simply cache the text content until freezeDef()
                qts << textContent;
            else if (_idxStaff != _idxGrid)
                // The other element types cache a fully-defined <text> element
                qts << getFrozenElement(textContent, defClass, x, y);

            // Ensure that tempo defs have the correct width
            if (_isMulti && _et == EType::TEMPO_TEXT) {
                // Tempos are part of the "system" staff, and always paint last
                if (!frozenWidths.contains(_cue_id)) {
                    // No other frozen elements for this cue_id, find the previous
                    // cue_id. This code only runs when there is a tempo change w/o
                    // a key/timesig or clef change. Very rarely > twice per score.
                    int width = 0;
                    QStringList::iterator i;
                    QStringList keys = frozenWidths.keys();
                    keys.sort();
                    for (i = keys.begin(); i != keys.end(); ++i) {
                        if (*i < _cue_id)
                            width = frozenWidths[*i];
                        else if (*i > _cue_id)
                            break;
                    }
                    frozenWidths.insert(_cue_id, width);
                }
            }

            // Add the value to the list for key = idxStaff-EType
            if (!def->contains(key)) {
                StrPtrList* spl = new StrPtrList;
                def->insert(key, spl);
            }

            if (_et == EType::KEYSIG) {
                // Set KeySig order: left-to-right (and exclude natural signs)
                if (*(textItem.text().unicode()) != NATURAL_SIGN)
                    (*def)[key]->insert(0, elm);
            }
            else
                (*def)[key]->append(elm);
        }
    }
}

//
// SvgPaintEngine private functions
//

// Gets the contents of the SVG class attribute, based on element type/name
QString SvgPaintEngine::getClass()
{
    QString eName;

    // Add element type as "class"
    if (_e == NULL)
        return eName; // _e should never be null, but this prevents a crash if it is

    switch(_et) {
    case EType::BAR_LINE :
        // BarLine sub-types
        if (static_cast<Ms::Segment*>(_e->parent())->segmentType() == Ms::Segment::Type::BeginBarLine) {
            // System BarLines - the system start-of-bar line
            eName = _e->name(EType::SYSTEM);
        }
        else {
            // Measure BarLines by BarLineType
            BLType blt = static_cast<const Ms::BarLine*>(_e)->barLineType();
            if  (blt == BLType::NORMAL)
                eName = _e->name(_et); // just like the default case
            else
                // Non-NORMAL BarLineTypes use the BLType name
                eName = static_cast<const Ms::BarLine*>(_e)->barLineTypeName();
        }
        break;
    case EType::CLEF :
        // For horizontal scrolling, all but the firt clef are courtesy clefs.
        // Unfortunately, everything is in reverse order, so the first clef is
        // the last one to pass through here. cue_id is draw-order-independent.
        if(!_isScrollVertical && _cue_id != CUE_ID_ZERO)
            eName = CLASS_CLEF_COURTESY;
        else
            eName = _e->name(_et); // just like the default case
        break;
    // Text sub-types = TextStyleData.name():
    case EType::TEXT :
        // EType::Text covers a bunch of different MuseScore styles, some have
        // spaces in the name, e.g. "Measure Number". CSS is easier w/o spaces.
        eName = static_cast<const Ms::Text*>(_e)->textStyle().name().remove(SVG_SPACE);
        break;
    case EType::STAFF_TEXT :
        // To distinguish between Staff and System text
        eName = QString("%1%2")
                .arg(static_cast<const Ms::Text*>(_e)->textStyle().name())
                .arg("Text");
        break;
    case EType::NOTE :
    case EType::STEM :
    case EType::BEAM :
        // Tablature staves get prefixed class names for these element types
        if (_e->staff()->isTabStaff()) {
            eName= QString("%1%2").arg(SVG_PREFIX_TAB).arg(_e->name(_et));
            break;
        } // else fall-through to default for these element types
    default:
        // For most cases it's simply the element type name
        eName = _e->name(_et);
        break;
    }
    return eName;
}

void SvgPaintEngine::streamXY(const qreal x, const qreal y)
{
    stream() << fixedFormat(SVG_X, x, d_func()->xDigits, true);
    stream() << fixedFormat(SVG_Y, y, d_func()->yDigits, true);
}

// Duplicated in file.cpp, formatReal()...
QString SvgPaintEngine::fixedFormat(const QString& attr,
                                    const qreal    n,
                                    const int      maxDigits,
                                    const bool     withQuotes)
{
    QString qsN = QString::number(n, 'f', SVG_PRECISION);
    QString qs;
    QTextStream qts(&qs);
    qts << attr;
    qts.setFieldAlignment(QTextStream::AlignRight);
    qts.setFieldWidth(maxDigits + SVG_PRECISION + (withQuotes ? 2: 0) + 1);
    if (withQuotes)
        qts << QString("%1%2%3").arg(SVG_QUOTE).arg(qsN).arg(SVG_QUOTE);
    else
        qts << QString("%1").arg(qsN);

    return qs;
}

// Streams the start of a frozen pane def, including the staff lines,
// consolidating some code in SvgPaintEngine::end()
void SvgPaintEngine::beginDef(const int      idx,
                              const QString& cue_id)
{
    const QString id  = !_isMulti ? cue_id
                                  : QString("%1%2%3")
                                       .arg((*_iNames)[idx])
                                       .arg(SVG_DOT)
                                       .arg(cue_id);

    stream() << SVG_4SPACES << SVG_GROUP_BEGIN
             << SVG_ID      << id                   << SVG_QUOTE
             << SVG_WIDTH   << frozenWidths[cue_id] << SVG_QUOTE
             << SVG_GT      << endl;

    if (_isMulti) {
        if (idx < _nStaves && idx != _idxGrid)
            // StaffLines/System BarLine(s) in all defs except System and Grid staves
            stream() << *(frozenLines[idx]);
    }
}

// Completes the open frozen pane definition in the frozenDefs collection.
// Conceptually: _cue_id + this function freeze the staves at a moment in time.
// Called by SaveSMAWS() in mscore/file.cpp.
// Called every time (before) _cue_id changes.
// Some class variables are vectors by staff.
// Protected: only called by SvgGenerator::freezeIt()
void SvgPaintEngine::freezeDef(int idxStaff)
{
    int         idx, w;
    QString     key;
    qreal       timeX;                      // x-coord where timesig starts
    const qreal keyX = _xLeft + 4 + 16;     // x-coord where keysig  starts
    FDef*       def  = frozenDefs[_cue_id]; // the frozen def we'll be updating

    // Tempo is in the "system" staff, which is always based on the topmost
    // staff in the score, idx == 0. if (_isMulti) these are always the last
    // frozen defs in the file, all grouped together, tempo-only definitions.
    // This procedure is never called for the "system" staff, so this procedure
    // only deals with tempo for NOT(_isMulti) exports.
    if (!_isMulti  && _prevDef != 0) {
        key = getDefKey(0, EType::TEMPO_TEXT);
        if (!def->contains(key) && _prevDef->contains(key)) // Nothing new for this cue
            def->insert(key, (*_prevDef)[key]);             // id, use the prevDef.
    }

    // If the current cue_id is missing elements, fill them in with _prevDef
    for (idx = 0; idx < _nStaves; idx++) {
        if (idxStaff > -1)
            idx = idxStaff; // Freeze one staff only

        if (idx != _idxGrid) {   // grid staff frozen pane only has timesigs
            if (_prevDef != 0) {
                // InstrumentNames
                key = getDefKey(idx, EType::INSTRUMENT_NAME);
                if (!def->contains(key) && _prevDef->contains(key))
                    def->insert(key, (*_prevDef)[key]);

                // Clefs
                key = getDefKey(idx, EType::CLEF);
                if (!def->contains(key) && _prevDef->contains(key))
                    def->insert(key, (*_prevDef)[key]);
            }
            // KeySigs
            freezeSig(def, idx, frozenKeyY, EType::KEYSIG, keyX);
        }
        // TimeSigs
        timeX = keyX + xOffsetTimeSig[_cue_id] + 3; //!!! fixed margin between KeySig/TimeSig. Default setting is 0.5 * spatium, but it ends up more like 3 than 2.5. not sure why.
        freezeSig(def, idx, frozenTimeY, EType::TIMESIG, timeX);

        if (idxStaff > -1)
            break; // Freeze only one staff
    }

    // The width of the entire frozen pane for this _cue_id
    // if (_isMulti) this runs more than once, so we use the widest value.
    // If MuseScore ever allows different keysigs by staff, this code is ready. :-)
    w = qRound(timeX + 13);                                                     //!!! 13 = timesig width plus 3 for margin/rounding
    if (!frozenWidths.contains(_cue_id) || frozenWidths[_cue_id] < w)
        frozenWidths.insert(_cue_id, w);

    // For the next time. If freezing by staff, _prevDef is reset to zero, by
    // staff, before streaming any elements, when MuseScore::paintStaffLines()
    // calls svgGenerator::beginMultiGroup(). Each staff must begin with a clef
    // and a timesig, so the risk of null pointer errors should be zero.
    _prevDef = def;
    _prevCue = _cue_id;
}

void SvgPaintEngine::freezeSig(FDef* def, int idx, RealListVect& frozenY, EType eType, qreal x)
{
    int         i;
    QString*    elm;
    QString     key, content, type;
    QTextStream qts;
    StrPtrList* spl;
    const bool  isKeySig = (eType == EType::KEYSIG);

    key  = getDefKey(idx, eType);
    type = _e->name(eType);
    if (!def->contains(key)) {
        spl = new StrPtrList;
        def->insert(key, spl);
    }
    for (i = 0; i < frozenY[idx].size(); i++) {
        if ((*def)[key]->size() == i) {
            if (_prevDef != 0 && _prevDef->contains(key) && (*_prevDef)[key]->size() > i)
            {                                            // Better than maintaining another vector by staff
                elm     = (*(*_prevDef)[key])[i];        // Each character = 1 XML element (time sig has no support for sub-divided numerators, like 7/8 as 4+3/8. It's a frozen pane width issue too.)
                content = elm->mid(elm->size() - 16, 8); // &#xE260;          =  8 chars
                if (isKeySig)                            // &#xE260;</text>\n = 16 chars
                    xOffsetTimeSig[_cue_id] = xOffsetTimeSig[_prevCue];
            }
            else
                content = QString("Staff number %1 has a %2 problem.").arg(idx).arg(type);

            elm = new QString;
            spl->append(elm);
        }
        else {
            elm     = (*(*def)[key])[i];
            content = *elm;
            elm->resize(0);
        }
        qts.setString(elm);
        qts << getFrozenElement(content,
                                type,
                                x + (isKeySig ? i * 5 : 0),
                                frozenY[idx][i] + (isKeySig ? yOffsetKeySig[idx] : 0));
    }
}

// Returns a fully defined <text> element for frozen pane def
QString SvgPaintEngine::getFrozenElement(const QString& textContent,
                                         const QString& classValue,
                                         const qreal    x,
                                         const qreal    y)
{
    QString     qs;
    QTextStream qts(&qs);
    initStream(&qts);

    qts << SVG_8SPACES << SVG_TEXT_BEGIN << SVG_CLASS;
    qts.setFieldWidth(15);                                // InstrumentName"=15
    qts << QString("%1%2").arg(classValue).arg(SVG_QUOTE);
    qts.setFieldWidth(0);
    qts << fixedFormat(SVG_X, x, 3, true)                           //!!! literal value for field width
        << fixedFormat(SVG_Y, y, d_func()->yDigits, true) << SVG_GT
        << textContent << SVG_TEXT_END << endl;

    return qs;
}

// One line of code, but it's long enougn and used often enough to consolidate
QString SvgPaintEngine::getDefKey(int idx, EType eType)
{
    return QString("%1%2%3").arg(idx).arg(SVG_DASH).arg((int)eType);
}

// Most of the streams are initialized with the same properties.
void SvgPaintEngine::initStream(QTextStream* stream)
{
    stream->setFieldAlignment(QTextStream::AlignLeft);
    stream->setRealNumberNotation(QTextStream::FixedNotation);
    stream->setRealNumberPrecision(SVG_PRECISION);
}

///////////////////////////////////////////////////////////////////////////////
class SvgGeneratorPrivate
{
public:
    SvgPaintEngine *engine;

    uint owns_iodevice : 1;
    QString fileName;
};

/*!
    \class SvgGenerator
    \ingroup painting
    \since 4.3
    \brief The SvgGenerator class provides a paint device that is used to create SVG drawings.
    \reentrant
    This paint device represents a Scalable Vector Graphics (SVG) drawing. Like QPrinter, it is
    designed as a write-only device that generates output in a specific format.
    To write an SVG file, you first need to configure the output by setting the \l fileName
    or \l outputDevice properties. It is usually necessary to specify the size of the drawing
    by setting the \l size property, and in some cases where the drawing will be included in
    another, the \l viewBox property also needs to be set.
    \snippet examples/painting/svggenerator/window.cpp configure SVG generator
    Other meta-data can be specified by setting the \a title, \a description and \a resolution
    properties.
    As with other QPaintDevice subclasses, a QPainter object is used to paint onto an instance
    of this class:
    \snippet examples/painting/svggenerator/window.cpp begin painting
    \dots
    \snippet examples/painting/svggenerator/window.cpp end painting
    Painting is performed in the same way as for any other paint device. However,
    it is necessary to use the QPainter::begin() and \l{QPainter::}{end()} to
    explicitly begin and end painting on the device.
    The \l{SVG Generator Example} shows how the same painting commands can be used
    for painting a widget and writing an SVG file.
    \sa SvgRenderer, SvgWidget, {About SVG}
*/

/*!
    Constructs a new generator.
*/
SvgGenerator::SvgGenerator()
    : d_ptr(new SvgGeneratorPrivate)
{
    Q_D(SvgGenerator);

    d->engine = new SvgPaintEngine;
    d->owns_iodevice = false;
}

/*!
    Destroys the generator.
*/
SvgGenerator::~SvgGenerator()
{
    Q_D(SvgGenerator);
    if (d->owns_iodevice)
        delete d->engine->outputDevice();
    delete d->engine;
}

/*!
    \property SvgGenerator::title
    \brief the title of the generated SVG drawing
    \since 4.5
    \sa description
*/
QString SvgGenerator::title() const
{
    Q_D(const SvgGenerator);

    return d->engine->documentTitle();
}

void SvgGenerator::setTitle(const QString &title)
{
    Q_D(SvgGenerator);

    d->engine->setDocumentTitle(title);
}

/*!
    \property SvgGenerator::description
    \brief the description of the generated SVG drawing
    \since 4.5
    \sa title
*/
QString SvgGenerator::description() const
{
    Q_D(const SvgGenerator);

    return d->engine->documentDescription();
}

void SvgGenerator::setDescription(const QString &description)
{
    Q_D(SvgGenerator);

    d->engine->setDocumentDescription(description);
}

/*!
    \property SvgGenerator::size
    \brief the size of the generated SVG drawing
    \since 4.5
    By default this property is set to \c{QSize(-1, -1)}, which
    indicates that the generator should not output the width and
    height attributes of the \c<svg> element.
    \note It is not possible to change this property while a
    QPainter is active on the generator.
    \sa viewBox, resolution
*/
QSize SvgGenerator::size() const
{
    Q_D(const SvgGenerator);
    return d->engine->size();
}

void SvgGenerator::setSize(const QSize &size)
{
    Q_D(SvgGenerator);
    d->engine->setSize(size);
}

/*!
    \property SvgGenerator::viewBox
    \brief the viewBox of the generated SVG drawing
    \since 4.5
    By default this property is set to \c{QRect(0, 0, -1, -1)}, which
    indicates that the generator should not output the viewBox attribute
    of the \c<svg> element.
    \note It is not possible to change this property while a
    QPainter is active on the generator.
    \sa viewBox(), size, resolution
*/
QRectF SvgGenerator::viewBoxF() const
{
    Q_D(const SvgGenerator);
    return d->engine->viewBox();
}

/*!
    \since 4.5
    Returns viewBoxF().toRect().
    \sa viewBoxF()
*/
QRect SvgGenerator::viewBox() const
{
    Q_D(const SvgGenerator);
    return d->engine->viewBox().toRect();
}

void SvgGenerator::setViewBox(const QRectF &viewBox)
{
    Q_D(SvgGenerator);
    d->engine->setViewBox(viewBox);
}

void SvgGenerator::setViewBox(const QRect &viewBox)
{
    setViewBox(QRectF(viewBox));
}

/*!
    \property SvgGenerator::fileName
    \brief the target filename for the generated SVG drawing
    \since 4.5
    \sa outputDevice
*/
QString SvgGenerator::fileName() const
{
    Q_D(const SvgGenerator);
    return d->fileName;
}

void SvgGenerator::setFileName(const QString &fileName)
{
    Q_D(SvgGenerator);
    if (d->engine->isActive()) {
        qWarning("SvgGenerator::setFileName() must be called prior to QPainter::begin()");
        return;
    }

    if (d->owns_iodevice)
        delete d->engine->outputDevice();

    d->owns_iodevice = true;

    d->fileName = fileName;
    QFile *file = new QFile(fileName);
    d->engine->setOutputDevice(file);

    // Even if there is no frozen pane for this score, this is easiest here
    d->engine->frozenFile.setFileName(QString("%1_frz.svg")
                                      .arg(fileName.left(fileName.length() - 4)));
}

/*!
    \property SvgGenerator::outputDevice
    \brief the output device for the generated SVG drawing
    \since 4.5
    If both output device and file name are specified, the output device
    will have precedence.
    \sa fileName
*/
QIODevice *SvgGenerator::outputDevice() const
{
    Q_D(const SvgGenerator);
    return d->engine->outputDevice();
}

void SvgGenerator::setOutputDevice(QIODevice *outputDevice)
{
    Q_D(SvgGenerator);
    if (d->engine->isActive()) {
        qWarning("SvgGenerator::setOutputDevice() must be called prior to QPainter::begin()");
        return;
    }
    d->owns_iodevice = false;
    d->engine->setOutputDevice(outputDevice);
    d->fileName = QString();
}

/*!
    \property SvgGenerator::resolution
    \brief the resolution of the generated output
    \since 4.5
    The resolution is specified in dots per inch, and is used to
    calculate the physical size of an SVG drawing.
    \sa size, viewBox
*/
int SvgGenerator::resolution() const
{
    Q_D(const SvgGenerator);
    return d->engine->resolution();
}

void SvgGenerator::setResolution(int dpi)
{
    Q_D(SvgGenerator);
    d->engine->setResolution(dpi);
}

/*!
    Returns the paint engine used to render graphics to be converted to SVG
    format information.
*/
QPaintEngine *SvgGenerator::paintEngine() const
{
    Q_D(const SvgGenerator);
    return d->engine;
}

/*!
    \reimp
*/
int SvgGenerator::metric(QPaintDevice::PaintDeviceMetric metric) const
{
    Q_D(const SvgGenerator);
    switch (metric) {
    case QPaintDevice::PdmDepth:
        return 32;
    case QPaintDevice::PdmWidth:
        return d->engine->size().width();
    case QPaintDevice::PdmHeight:
        return d->engine->size().height();
    case QPaintDevice::PdmDpiX:
        return d->engine->resolution();
    case QPaintDevice::PdmDpiY:
        return d->engine->resolution();
    case QPaintDevice::PdmHeightMM:
        return qRound(d->engine->size().height() / Ms::DPMM);
    case QPaintDevice::PdmWidthMM:
        return qRound(d->engine->size().width() / Ms::DPMM);
    case QPaintDevice::PdmNumColors:
        return 0xffffffff;
    case QPaintDevice::PdmPhysicalDpiX:
        return d->engine->resolution();
    case QPaintDevice::PdmPhysicalDpiY:
        return d->engine->resolution();
    case QPaintDevice::PdmDevicePixelRatio:
    case QPaintDevice::PdmDevicePixelRatioScaled:
        return 1;
    default:
        qWarning("SvgGenerator::metric(), unhandled metric %d\n", metric);
        break;
    }
    return 0;
}

/*!
    setElement() function
    Sets the _e and _et variables in SvgPaintEngine.
    Called by saveSVG() in mscore/file.cpp.
*/
void SvgGenerator::setElement(const Ms::Element* e) {
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());
    pe->_e  = e;
    pe->_et = e->type(); // The _e's type appears often, so we get it only once
}

/*!
    setSMAWS() function
    Sets the _isSMAWS variable in SvgPaintEngine to true.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setSMAWS() {
    static_cast<SvgPaintEngine*>(paintEngine())->_isSMAWS = true;
}

/*!
    setCueID() function
    Sets the _cue_id variable in SvgPaintEngine.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setCueID(const QString& qs) {
    static_cast<SvgPaintEngine*>(paintEngine())->_cue_id = qs;
}

/*!
    setScrollAxis() function
    Sets the _isScrollVertical variable in SvgPaintEngine.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setScrollAxis(bool axis) {
    static_cast<SvgPaintEngine*>(paintEngine())->_isScrollVertical = axis;
}

/*!
    setNonStandardStaves() function
    Sets the _hasNonStandard variable in SvgPaintEngine.
    Called by paintStaffLines() in mscore/file.cpp.
*/
void SvgGenerator::setNonStandardStaves(QVector<int>* nonStdStaves) {
    static_cast<SvgPaintEngine*>(paintEngine())->_nonStdStaves = nonStdStaves;
}

/*!
    setNStaves() function
    Sets the _nStaves variable in SvgPaintEngine: number of visible staves
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setNStaves(int n) {
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());
    pe->_nStaves = n;
    pe->frozenKeyY.resize(n);
    pe->frozenTimeY.resize(n);
    pe->frozenLines.resize(n);
    pe->yLineKeySig.resize(n);
    pe->yOffsetKeySig.resize(n);
    for (int i = 0; i < n; i++) {
        pe->frozenLines[i]   = 0; // QString*
        pe->yOffsetKeySig[i] = 0; // qreal
    }
}

/*!
    setStaffIndex() function
    Sets the _idxStaff variable in SvgPaintEngine: current visible-staff index
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setStaffIndex(int idx) {
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());
    pe->_idxStaff      = idx;
}

/*!
    setCursorTop() function
    Sets the _cursorTop variable in SvgPaintEngine.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setCursorTop(qreal top) {
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());

    if (pe->_cursorTop == 0 || top < pe->_cursorTop)
        pe->_cursorTop = top;
}

/*!
    setCursorHeight() function
    Sets the _cursorHeight variable in SvgPaintEngine.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setCursorHeight(qreal height) {
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());

    if (height > pe->_cursorHeight)
        pe->_cursorHeight = height;
}

/*!
    !!!OBSOLETE!!! setStartMSecs() function
    Sets the _startMSecs variable in SvgPaintEngine.
    Called by saveSMAWS() in mscore/file.cpp.
*/
//void SvgGenerator::setStartMSecs(int start) {
//    static_cast<SvgPaintEngine*>(paintEngine())->_startMSecs = start;
//}

/*!
    freezeIt() function (SMAWS)
    Calls SvgPaintEngine::freezeDef() to complete a frozen pane def
    Called by paintStaffSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::freezeIt(int idxStaff) {
    static_cast<SvgPaintEngine*>(paintEngine())->freezeDef(idxStaff);
}

/*!
    streamDefs() function (SMAWS)
    Calls SvgPaintEngine::streamDefs() to point the stream at the defs string
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::streamDefs() {
    static_cast<SvgPaintEngine*>(paintEngine())->streamDefs();
}

/*!
    streamBody() function (SMAWS)
    Calls SvgPaintEngine::streamBody() to point the stream at the body string
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::streamBody() {
    static_cast<SvgPaintEngine*>(paintEngine())->streamBody();
}

/*!
    beginMultiGroup() function (SMAWS)
    Calls SvgPaintEngine::beginMultiGroup() to stream the necessary text.
    For Multi-SelectStaves, so it sets _isMulti = true. It also signals the
    beginning of a new staff, so it reinitializes the _prevDef pointer.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::beginMultiGroup(QStringList* pINames, const QString& fullName, const QString& className, qreal height, qreal top, const QString& cues) {
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());
    pe->_isMulti = true;
    pe->_prevDef = 0;
    if (pINames != 0) {
        pe->_iNames = pINames;
        pe->beginMultiGroup(pINames->last(), fullName, className, height, top, cues);
    }
    else // this applies to lyrics pseudo-staves
        pe->beginMultiGroup(pe->_iNames->last().toUpper(), fullName, className, height, top, cues);
}

/*!
    endMultiGroup() function (SMAWS)
    Calls SvgPaintEngine::endMultiGroup() to stream the necessary text
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::endMultiGroup() {
    static_cast<SvgPaintEngine*>(paintEngine())->endMultiGroup();
}

/*!
    setYOffset() function
    Sets the _yOffset variable in SvgPaintEngine.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setYOffset(qreal y) {
    static_cast<SvgPaintEngine*>(paintEngine())->_yOffset = y;
}

/*!
    setMaxNote() function
    Sets the _maxNote variable in SvgPaintEngine.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setMaxNote(int max) {
    static_cast<SvgPaintEngine*>(paintEngine())->_maxNote = max;
}

/*!
    createMultiUse() function
    Streams the <use> elements for Multi-Select Staves frozen pane file only
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::createMultiUse(qreal y) {
    static_cast<SvgPaintEngine*>(paintEngine())->createMultiUse(y);
}
