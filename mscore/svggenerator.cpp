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

// SMAWS includes
#include "libmscore/score.h"     // for Score::nstaves()
#include "libmscore/tempotext.h" // for TempoText class
#include "libmscore/clef.h"      // for ClefType/ClefInfo
#include "libmscore/keysig.h"
#include "libmscore/key.h"
#include "libmscore/staff.h"     // for Tablature staves
#include "libmscore/text.h"      // for Measure numbers
#include "libmscore/note.h"      // for MIDI note numbers (pitches)
#include "libmscore/segment.h"

#define SVG_DATA_P " data-p=\""  // 4 chars less for every note - it adds up!

static void translate_color(const QColor &color, QString *color_string,
                            QString *opacity_string)
{
    Q_ASSERT(color_string);
    Q_ASSERT(opacity_string);

    *color_string =
        QString::fromLatin1("#%1%2%3")
        .arg(color.red(), 2, 16, QLatin1Char('0'))
        .arg(color.green(), 2, 16, QLatin1Char('0'))
        .arg(color.blue(), 2, 16, QLatin1Char('0'));
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

class SvgPaintEnginePrivate
{
public:
    SvgPaintEnginePrivate()
    {
        size = QSize();
        viewBox = QRectF();
        outputDevice = 0;
        resolution = Ms::DPI;

        attributes.title = QLatin1String("");
        attributes.desc  = QLatin1String("");
    }
    int xDigits;
    int yDigits;
    int resolution;
    QString header;
    QString defs;
    QString body;
    QSize   size;
    QRectF  viewBox;
    QBrush  brush;
    QPen    pen;
    QMatrix matrix;
    QIODevice   *outputDevice;
    QTextStream *stream;

    struct _attributes {
        QString title;
        QString desc;
    } attributes;
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
    SvgPaintEnginePrivate *d_ptr; // the one and only
    QTextCodec*           _codec; // it gets used more than once

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

// For centering rehearsal mark text in frames. The frame is drawn first.
    QRect _textFrame; // yes, integers

////////////////////
// SMAWS
    bool _isFullMatrix; // A full matrix requires different handling of the y-offset for Multi-Select Staves
    bool _isGroupOpen;  // Useful when it's time to close the group: </g>

// For element types that use multiple graphical types to paint.
// CSS requires that each element type have its own class name, so that in SVG
// they must be sorted by element type, inside separate <g>s. This requires
// temporary storage until the next element type arrives.
    QString _leftovers;

// For notes and other elements to get the offsets from stems and ledger lines
//     tick  x,y
    map<int, RealPair> _offsets;

// class="tabNote" position
    Int2DblMap _stemX; // by tick

// Frozen Pane
    using FDef  = QMap<QString, StrPtrList*>;       // Key    = idxStaff-EType
    using FDefs = QMap<QString, FDef*>; //by cue_id // Values = <text> elements

    Str2IntMap   frozenWidths;
    FDefs        frozenDefs;
    Int2BoolMap  frozenClefs; // by tick: true if any staff has non-G clef
    QFile        frozenFile;
    qreal        _xLeft;      // StaffLines left x-coord, for element alignment

    QString getFrozenElement(const QString& textContent,
                             const QString& classValue,
                             const EType    eType,
                             const qreal    x,
                             const qreal    y);
//
////////////////////

// Gets the SVG class attribute for an element
    QString getClass();

// Most streams share these basic settings
    void initStream(QTextStream* stream);

// For fancy text formatting inside the SVG file
    QString formatXY(qreal x, qreal y, bool isFrozen);
    QString fixedFormat(const QString& attr,
                        const qreal    n,
                        const int      maxDigits,
                        const bool     inQuotes);

protected:
    const Ms::Element* _e;  // The Element being generated now
          EType        _et; // That element's ::Type - it's used everywhere

////////////////////
// SMAWS
    QString _cue_id;           // The current VTT cue ID
    bool    _isSMAWS;          // In order to use SMAWS code only when necessary
    bool    _isScrollVertical; // Only 2 axes: x = false, y = true.
    bool    _isMulti;          // Multi-Select Staves file formats/formatting
    qreal   _cursorTop;        // For calculating the height of (vertical bar)
    qreal   _cursorHeight;     // Sheet music playback position cursor.
    qreal   _yOffset;          // Y axis offset used by Multi-Select Staves
    qreal   _sysLeft;          // Left and right edges of staff lines on the
    qreal   _sysRight;         // page for part title and credits formatting
    int     _nLines;           // Number of staff lines for a part
    IntVect _staffLinesY;      // by staff line for the current _idxStaff
////////////////////
// Frozen Pane
//
    bool _hasFrozen; // Does this score have a frozen pane? = _isSMAWS && !_isScrollVertical
    bool _isFrozen;  // Is _e part of a frozen pane?
    bool _isGrand;   // Is the current staff a grand staff or other unlinked multi-stave staff?
    bool _isLinked;  // Is the current staff linked or part of a notes/tabs pair of staves?
    int  _nStaves;   // Number of staves in the current score
    int  _idxStaff;  // The current staff index
    int  _idxSlash;  // The grid staff index

    QVector<int>* _nonStdStaves; // Vector of staff indices for the tablature and percussion staves in this score

    // These vary by Staff: (not sure if KeyY or TimeY need to be by staff, could clear between staves)
    StrPtrVect   brackets;      // in frozen pane, analogous to frozenLines, but not always populated
    StrPtrVect   frozenLines;   // vector by staff, frozen pane staff/system lines
    RealListVect frozenKeyY;    // vector by staff, list = y-coords accidentals in left-to-right order
    RealListVect frozenTimeY;   // vector by staff, list = y-coords numbers in bot-to-top order (reverse order for unlinked multi-staff time sigs)
    RealVect     yLineKeySig;   // vector by staff, clef's start "staff line" for first accidental (range = 0-9 top-to-bottom for 5-line staff)
    RealVect     yOffsetKeySig; // vector by staff, non-zero if clef changes
    Int2RealMap  frozenINameY;  // map by by staff index, vertical center of staff lines for linked staves extra iNames

    FDef*        _prevDef;    // The previous def,    used by freezeDef()
    QString      _prevCue;    // The previous cue_id, used by freezeDef()
    QStringList* _iNames;     // Multi-Select Staves: list of instrument names
    QStringList* _fullNames;  // Multi-Select Staves: list of full/long names
    QStringList  _multiUse;   // ditto: list of <use> element text starters
    QStringList  _multiTitle; // ditto: list of <use> element titles

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
    void beginMultiGroup(const QString& iName,
                         const QString& fullName,
                         const QString& className,
                         int height,
                         int top)
    {
        QString name = (fullName == 0 ? iName : fullName);
        *d_func()->stream << SVG_SPACE << SVG_GROUP_BEGIN
                             << SVG_TRANSFORM
                                << SVG_TRANSLATE << SVG_ZERO    << SVG_SPACE // x
                                                 << top  << SVG_RPAREN_QUOTE // y
                             << SVG_HEIGHT       << height      << SVG_QUOTE
                             << SVG_ID           << iName       << SVG_QUOTE
                             << SVG_CLASS        << className   << SVG_QUOTE
                             << SVG_INAME        << name        << SVG_QUOTE
                          << SVG_GT << endl;
        if (fullName == STAFF_SLASH)
            _idxSlash = _iNames->size() - 1;
    }
    void beginMouseGroup() {
        closeGroup(); // groups by class must be terminated first
        *d_func()->stream << SVG_SPACE   << SVG_SPACE   << SVG_GROUP_BEGIN
                          << SVG_POINTER << SVG_VISIBLE << SVG_QUOTE << SVG_GT
                          << endl;
    }
    void beginGroup(int indent, bool isFrozen) {
        closeGroup(); // groups by class must be terminated first
        for (int i = 1; i <= indent; i++)
            *d_func()->stream << SVG_SPACE;
        *d_func()->stream << SVG_GROUP_BEGIN << SVG_GT << endl;
        _isFrozen = isFrozen;
    }
    void endGroup(int indent, bool isFrozen) {
        closeGroup(); // groups by class must be terminated first
        for (int i = 1; i <= indent; i++)
            *d_func()->stream << SVG_SPACE;
        *d_func()->stream << SVG_GROUP_END << endl;
        if (isFrozen)
            _isFrozen = false; // end of frozen group
    }
    void closeGroup() {
        if (_isGroupOpen) { // these are groups by element type/class
            *d_func()->stream << SVG_3SPACES << SVG_GROUP_END << endl;
            if (!_leftovers.isEmpty()) {     //!!for now it's always <text>
                *d_func()->stream << SVG_3SPACES << SVG_GROUP_BEGIN
                                  << SVG_CLASS   << _classValue   << "Text"
                     << SVG_QUOTE << SVG_GT      << endl          << _leftovers
                                  << SVG_3SPACES << SVG_GROUP_END << endl;;
                _leftovers.clear();
            }
            _isGroupOpen = false;
        }
    }

    // Streams the <use> elements for Multi-Select Staves frozen pane file only
    void createMultiUse(qreal y) {
        _multiUse.append(QString("%1%2")
                         .arg(SVG_USE)
                         .arg(fixedFormat(SVG_Y, y, d_func()->yDigits, true)));
    }
//
////////////////////

public:
    SvgPaintEngine() : QPaintEngine(svgEngineFeatures())
    {
        d_ptr = new SvgPaintEnginePrivate;
        _codec = QTextCodec::codecForName("UTF-8");

        _e  = NULL;
        _et = Ms::ElementType::INVALID;

        _hasFrozen    = false;
        _isFrozen     = false;
        _isGrand      = false;
        _isLinked     = false;
        _prevDef      = 0;   // FDef*
        _iNames       = 0;   // QStringList*
        _fullNames    = 0;   // QStringList*
        _nonStdStaves = 0;   // QVector<int>*
        _nStaves      = 0;   // int
        _nLines       = 0;   // int
        _idxSlash     = -1;  // int
        _xLeft        = 0.0; // qreal
        _cursorTop    = 0.0; // qreal
        _cursorHeight = 0.0; // qreal
        _yOffset      = 0.0; // qreal
        _sysLeft      = 0.0; // qreal
        _sysRight     = 0.0; // qreal
    }

    bool begin(QPaintDevice *device);
    bool end();

    void updateState(const QPaintEngineState &s);

    const QString qpenToSvg(const QPen &spen);
    const QString qbrushToSvg(const QBrush &sbrush);

    void drawPath    (const QPainterPath &path);
    void drawPolygon (const QPointF *points, int pointCount, PolygonDrawMode mode);
    void drawRects   (const QRectF  *rects,  int rectCount);
    void drawTextItem(const QPointF &p, const QTextItem &textItem);
    void drawPixmap  (const QRectF  &r, const QPixmap &pm, const QRectF &sr);
    void drawImage   (const QRectF  &r, const QImage  &pm, const QRectF &sr, Qt::ImageConversionFlag = Qt::AutoColor);

    QPaintEngine::Type type() const { return QPaintEngine::SVG; }

    inline QTextStream &stream()    { return *d_func()->stream; }

    QSize size() const { return d_func()->size; }
    void setSize(const QSize &size) {
//!!        Q_ASSERT(!isActive());
        d_func()->size = size;
    }
    QRectF viewBox() const { return d_func()->viewBox; }
    void setViewBox(const QRectF &viewBox) {
//!!        Q_ASSERT(!isActive());
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
//!!        Q_ASSERT(!isActive());
        d_func()->outputDevice = device;
    }
    int resolution() { return d_func()->resolution; }
    void setResolution(int resolution) {
//!!        Q_ASSERT(!isActive());
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
    // Set these bools to their default values
    _isScrollVertical = false;
    _isMulti          = false;
    _isSMAWS          = false;
    _isGroupOpen      = false;
    _isFrozen         = false;
    _hasFrozen        = false;

    return true;
}

/////////////////////////
// SvgPaintEngine::end()
/////////////////////////
bool SvgPaintEngine::end()
{
    Q_D(SvgPaintEngine);

    // Stream the headers
    stream().setString(&d->header);

    // The reason for waiting to stream the headers until end() is to set the
    // scroll axis and check _isMulti [this used to happen in begin()].
    const QString scrollAxis = _isScrollVertical ? "y" : "x";

//!!obsolete    if (_isSMAWS) // SMAWS uses an external CSS file
//!!obsolete        stream() << XML_STYLE_MUSE;

    // The <svg> element:
    int height = qCeil(d->viewBox.height());
    stream() << SVG_BEGIN   << XML_NAMESPACE << (_hasFrozen ? XML_XLINK : "")
             << SVG_4SPACES << SVG_VIEW_BOX  << qCeil(d->viewBox.left())  << SVG_SPACE
                                             << qCeil(d->viewBox.top())   << SVG_SPACE
                                             << qCeil(d->viewBox.width()) << SVG_SPACE
                                             << height << SVG_QUOTE;
    if (_isScrollVertical)
        stream() << SVG_WIDTH  << d->size.width() << SVG_QUOTE;
    else
        stream() << SVG_HEIGHT << height          << SVG_QUOTE;
    stream()     << SVG_CLASS  << "fgFillStroke"  << SVG_QUOTE << endl;

    if (_isSMAWS) {
        stream() << SVG_4SPACES << SVG_XYMIN_SLICE
                 << SVG_POINTER << SVG_NONE   << SVG_QUOTE
                 << SVG_SCROLL  << scrollAxis << SVG_QUOTE;

        if (!_isMulti)
            stream() << SVG_STAFFLINES <<_nLines << SVG_QUOTE;
    }
    stream() << SVG_GT << endl;

    if (_isSMAWS) // inline <svg> titles pop up as tooltips for entire document
        stream() << SVG_DESC_BEGIN  << d->attributes.title << SVG_SPACE
                                    << d->attributes.desc  << SVG_DESC_END  << endl;
    else
        stream() << SVG_TITLE_BEGIN << d->attributes.title << SVG_TITLE_END << endl
                 << SVG_DESC_BEGIN  << d->attributes.desc  << SVG_DESC_END  << endl;


    if (_isSMAWS) { // Cursor at the end of the current body
        stream().setString(&d->body);

        // Two gray-out <rect>s (left/right) for graying out inactive bars
        QString indent;
        if (_isMulti) { // fixed margins relative to page height, modified in javascript
            indent = SVG_SPACE;
            _cursorHeight = height - (_cursorTop * 2);
        }
        stream() << indent << SVG_RECT
                    << SVG_CLASS          << CLASS_CURSOR  << SVG_QUOTE
                    << SVG_X << SVG_QUOTE << SVG_ZERO      << SVG_QUOTE
                    << SVG_Y << SVG_QUOTE << _cursorTop    << SVG_QUOTE
                    << SVG_WIDTH          << SVG_ZERO      << SVG_QUOTE
                    << SVG_HEIGHT         << _cursorHeight << SVG_QUOTE
                    << SVG_STROKE         << SVG_NONE      << SVG_QUOTE
                 << SVG_ELEMENT_END << endl;

        if (_isMulti) // Terminate the Staves group
            stream() << SVG_GROUP_END << endl;
    }

    // Deal with Frozen Pane, if it exists
    if (_hasFrozen) {
        // Frozen body - _hasFrozen depends on _isSMAWS, setString(&d->body) above
        int i;
        FDefs::iterator def;
        FDef::iterator  elms;
        const QString tempoKey = getDefKey(0, EType::TEMPO_TEXT);

        if (_isMulti) {
            // Frozen <use> elements by staff. SVG_GROUP_ consolidates events.
            stream() << SVG_GROUP_BEGIN
                        << SVG_POINTER << SVG_VISIBLE << SVG_QUOTE
                        << " mask=\"url(#maskFrozen)\""
                     << SVG_GT << endl;

            int last = _iNames->size() - 1;
            for (i = 0; i <= last; i++) {
                stream() << SVG_4SPACES     << _multiUse[i]
                         << SVG_ID          << (*_iNames)[i] << SVG_QUOTE
                         << XLINK_HREF      << (*_iNames)[i] << SVG_DASH
                                            << CUE_ID_ZERO   << SVG_QUOTE;
                if (i != _idxSlash && i != last)
                    stream() << SVG_GT
                         << SVG_TITLE_BEGIN << _multiTitle[i]
                         << SVG_TITLE_END   << SVG_USE_END << endl;
                else
                    stream() << SVG_ELEMENT_END << endl;
            }
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

        // Frozen defs, iterate by cue_id
        stream().setString(&d->defs);
        for (def = frozenDefs.begin(); def != frozenDefs.end(); ++def) {
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
                        stream() << SVG_2SPACES << SVG_GROUP_END << endl;

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
            // It also causes the frozen <def> groups for non-standard staves to
            // appear out of order in the <svg> file (after the "system" staff).
            if (hasKeySig && _nonStdStaves != 0 && _nonStdStaves->size() > 0 && cue_id != CUE_ID_ZERO) {
                for (i = 0; i < _nonStdStaves->size(); i++) {
                    QString timeKey = getDefKey(i, EType::TIMESIG);
                    if ((*frozenDefs[cue_id])[timeKey] != 0 && (*frozenDefs[cue_id])[timeKey]->size() > 0) {
                        beginDef((*_nonStdStaves)[i], cue_id);

                        if ((*_nonStdStaves)[i] != _idxSlash) // Slashes-only "slash" staff has no clef either (it should be a percussion staff)
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
        stream() << SVG_GROUP_BEGIN << SVG_ID << ID_STAVES << SVG_QUOTE
                 << " mask=\"url(#maskS)\""   << SVG_GT    << endl;

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
void SvgPaintEngine::updateState(const QPaintEngineState &s)
{
    // classState = class + optional data-cue + transform attributes
    // styleState = all other attributes, only for elements *not* styled by CSS
    classState.clear();
    styleState.clear();
    QTextStream qts;
    qts.setString(&classState);
    initStream(&qts);

    QString classVal = getClass();
    bool  isNewGroup = (classVal != _classValue);

    if (isNewGroup) {
        closeGroup();
        _classValue = classVal; // must follow closeGroup()
    }
    if (_isFrozen) {
        // Stream the class attribute, with fancy fixed frozen formatting
        // Frozen def:   InstrumentName" = 15
        // Frozen elm: InstrumentChange" = 17
        qts << SVG_CLASS;
        qts.setFieldWidth(_et == EType::BRACKET ? 15 : 17); //!!not a good conditional test, always 15
        qts << QString("%1%2").arg(_classValue).arg(SVG_QUOTE);
        qts.setFieldWidth(0);
    }
    else if (isNewGroup) { // classState gets no class attribute, but a new <g> does
        stream() << SVG_3SPACES << SVG_GROUP_BEGIN
                 << SVG_CLASS   << _classValue << SVG_QUOTE << SVG_GT << endl;
        _isGroupOpen = true;
    }

    // Stream the cue id to classState
    if (!_cue_id.isEmpty() && _et != EType::STAFF_LINES && _et != EType::BRACKET)
        qts << SVG_CUE << _cue_id << SVG_QUOTE;

    // Translations, SVG transform="translate()", are handled separately from
    // other transformations such as rotation. Qt translates everything, but
    // other transformations occur rarely. They are included in classState
    // because they affect CSS-styled elements too.
    // For the elements that need transformations other than translation
    QTransform t = s.transform();

    // Tablature Note Text:
    // These 2 have floating point flotsam, for example: 1.000000629
    // Both values should be integer 1, because no scaling is intended.
    // This rounds to three decimal places, as MuseScore does elsewhere.
    const qreal m11 = rint(t.m11() * 1000) / 1000.0;
    const qreal m22 = rint(t.m22() * 1000) / 1000.0;

    if ((m11 == 1 && m22 == 1 && t.m12() == t.m21()) // No scaling, no rotation
     || _classValue == CLASS_CLEF_COURTESY) {        // Only translate
        _dx = t.m31();                               
        _dy = t.m32();
        _isFullMatrix = false;
    }
    else { // Other transformations are more straightforward with a full matrix
        _dx = 0;
        _dy = 0;
        _isFullMatrix = true; // append the matrix goes to classState
        qts << SVG_MATRIX << t.m11() << SVG_COMMA
                          << t.m12() << SVG_COMMA
                          << t.m21() << SVG_COMMA
                          << t.m22() << SVG_COMMA
                          << t.m31() << SVG_COMMA
                          << t.m32() + _yOffset << SVG_RPAREN_QUOTE;
    }

    // Set attributes for element types *not* styled by CSS
    switch (_et) {
        case EType::ACCIDENTAL:            case EType::MEASURE_NUMBER:
        case EType::ARTICULATION:          case EType::NOTE:
        case EType::BEAM:                  case EType::NOTEDOT:
        case EType::BRACKET:               case EType::REHEARSAL_MARK:
        case EType::CLEF:                //case EType::REST : // CSS can't handle Rest <polyline>s
        case EType::GLISSANDO_SEGMENT:     case EType::SLUR_SEGMENT:
        case EType::HARMONY:               case EType::STAFF_LINES:
        case EType::HOOK:                  case EType::STEM:
        case EType::INSTRUMENT_CHANGE:     case EType::SYSTEM:
        case EType::INSTRUMENT_NAME:       case EType::TEXT:
        case EType::KEYSIG:                case EType::TIE_SEGMENT:
        case EType::LEDGER_LINE:           case EType::TIMESIG:
        case EType::LYRICS:                case EType::TREMOLO:
        case EType::LYRICSLINE_SEGMENT:    case EType::TUPLET:
            break; // Styled by CSS
        default:
            // The best way so far to style NORMAL bar lines in CSS w/o messing up
            // other types' styling, which may or may not be plausible in CSS:
            if (_et != EType::BAR_LINE
             || static_cast<const Ms::BarLine*>(_e)->barLineType() != BLType::NORMAL) {
                // Brush & Pen attributes only affect elements not styled by CSS
                qts.setString(&styleState);
                qts << qbrushToSvg(s.brush());
                qts <<   qpenToSvg(s.pen());
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
        // default SMAWS/MuseScore stroke color is black
        if (_color != SVG_BLACK)
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
    qreal yOff = _dy + (_isFullMatrix ? 0 : _yOffset);

    if (_isMulti)
        stream() << SVG_4SPACES;

    if (_isSMAWS && _et == EType::REHEARSAL_MARK) {
        // Rehearsal mark frame is rect or circle, no need for a complex path.
        // I can't find a way to determine if it's a rect or a circle here, so
        // I'm only supporting rects for now ...until I subclass QPainter and
        // override drawRoundedRect().
        // It's <rect> not <path> because rounded corners are better as rx & ry
        QRectF cpr = p.controlPointRect(); // faster than .boundingRect()
        _textFrame.setX(rint(cpr.x() + _dx));
        _textFrame.setY(rint(cpr.y() + yOff));
        _textFrame.setWidth (rint((_e->width()  * 0.5)) * 2); // even number for
        _textFrame.setHeight(rint((_e->height() * 0.5)) * 2); // centering text
        int rxy = static_cast<const Ms::TextBase*>(_e)->frameRound();

        stream() << SVG_RECT << classState << styleState
                 << SVG_X << SVG_QUOTE << _textFrame.x() << SVG_QUOTE
                 << SVG_Y << SVG_QUOTE << _textFrame.y() << SVG_QUOTE
                 << SVG_WIDTH     << _textFrame.width () << SVG_QUOTE
                 << SVG_HEIGHT    << _textFrame.height() << SVG_QUOTE
                 << SVG_RX << rxy << SVG_QUOTE << SVG_RY << rxy << SVG_QUOTE
                 << SVG_ELEMENT_END  << endl;
        return; // we're done here, no path to draw
    }

    QString qs;
    QTextStream qts(&qs);
    initStream(&qts);
    qts << SVG_PATH << classState << styleState;

    // Keep in mind: staff lines, bar lines and stems are everywhere in a score
    // It's important to render them properly in SVG and minimize their SVG text
    // These are maximum 0.5px adjustments on a canvas that is 5x normal size
    // The position changes are virtually invisible, but rendering is improved
    bool  isStaffLines = (_et == EType::STAFF_LINES);         // stroke-width="2", horz
    bool  isLyricsLine = (_et == EType::LYRICSLINE_SEGMENT);  // stroke-width="4", horz
    bool  isBarLine    = (_et == EType::BAR_LINE);            // stroke-width="4", vert
    bool  isLedger     = (_et == EType::LEDGER_LINE);         // stroke-width="3", horz
    bool  isStem       = (_et == EType::STEM);                // stroke-width="3", vert
    bool  isBeam       = (_et == EType::BEAM);                // no stroke, rhomboid shape
    bool  is24   = isStaffLines || isLyricsLine || isBarLine; // horz || horz || vert
    bool  is3    = isLedger     || isStem;                    // horz || vert
    int   height = _e->bbox().height();
    qreal x, y, z;
    int i, l, n, ix, iy, tick;

    if (is3)          // they only need 1 decimal place for .5
        qts.setRealNumberPrecision(1);
    else if (!is24 && !isBeam
             && _et != EType::NOTE         && _et != EType::BRACKET
             && _et != EType::SLUR_SEGMENT && _et != EType::TIE_SEGMENT
             && _et != EType::TREMOLO
             && p.fillRule() == Qt::OddEvenFill)
        qts << SVG_FILL_RULE; // fill-rule="evenodd"

//    // Barline cues only for the first staff in a system for SCORE
//    if (isBarLine && !_cue_id.isEmpty())
//        qts << SVG_CUE << _cue_id << SVG_QUOTE;

    char cmd  = 0;
    char prev = 0;
    QPointF pt;
    QPainterPath::Element ppe;
    QString dLine;
    QTextStream qtsLine(&dLine);
    
    qts << SVG_D; // elementCount - 1 because last == first == Z = closed path
    for (i = 0,  l = p.elementCount() - 1; i < l; ++i) {
        ppe = p.elementAt(i);
        switch (ppe.type) {
            case QPainterPath::MoveToElement:
            case QPainterPath::LineToElement:
                x   = ppe.x + _dx;
                y   = ppe.y + yOff;
                ix  = rint(x);
                iy  = rint(y);
                cmd = (ppe.type == QPainterPath::MoveToElement ? SVG_M : SVG_L);
                if (cmd == SVG_L) {
                    if (pt.x() == ppe.x)
                        cmd = SVG_V;
                    else if (pt.y() == ppe.y)
                        cmd = SVG_H;
                }
                pt = QPointF(ppe.x, ppe.y);
                if (cmd != prev) 
                    qts << cmd;
                if (cmd == SVG_M || cmd == SVG_L) {
                    // if/else if = M only, H or V lines = Mx,y Hx|Vy = never L
                    tick = _e->tick().ticks();
                    if (is24 || isBeam) {
                        qts << ix << SVG_COMMA << iy;
                        if (isStaffLines) { 
                            qtsLine << SVG_D << cmd   << ix  << SVG_COMMA << iy
                                    << SVG_H << FROZEN_WIDTH << SVG_QUOTE;
                            if (_e->staff()->isTabStaff(_e->tick())) 
                                _staffLinesY.push_back(iy);
                        }
                    }
                    else if (isStem) {
                        z = trunc(x) + 0.5;
                        qts << z << SVG_COMMA << iy;
                        if (_e->staff()->isTabStaff(_e->tick()) && _stemX.find(tick) == _stemX.end())
                            _stemX[tick] = z;
                        _offsets[tick] = RealPair(z - x, 0); // stems first
                    }
                    else if (isLedger) {
                        z = trunc(y) + 0.5;
                        qts << ix << SVG_COMMA << z;
                        if (_offsets.find(tick) != _offsets.end())
                            _offsets[tick].second = z - y;   // stems first
                        else
                            _offsets[tick] = RealPair(0, z - y);
                    }
                    else
                        qts << x << SVG_COMMA << y;
                }
                else if (is24 || is3 || isBeam)
                    qts << (cmd == SVG_H ? ix : iy); // grouped by variable type
                else
                    qts << (cmd == SVG_H ? x : y);
                break;
            case QPainterPath::CurveToElement:
                prev = SVG_C;
                qts << SVG_C;
                for (n = i + 2; i <= n; i++) {
                    ppe = p.elementAt(i);
                    qts << ppe.x + _dx << SVG_COMMA << ppe.y + yOff;
                    if (i < n)
                        qts << SVG_SPACE;
                }
                i = n;
                break;
            default:
                break;
        }
        if (cmd == prev)      // SVG_C never sets cmd, never omits command
            qts << SVG_SPACE; // repeated command requires delimiter
        prev = cmd;
    }
    if (is3) 
        qts.setRealNumberPrecision(SVG_PRECISION); // revert
    else if (!is24 && (cmd == SVG_L || cmd == SVG_H || cmd == SVG_V))
        qts << SVG_Z;           // closes the path
    qts << SVG_QUOTE;

    if (isStaffLines && !_cue_id.isEmpty()) {
        int bottom = ceil(p.elementAt(0).y + height + _dy);
        qts << SVG_CUE    << _cue_id << SVG_QUOTE
            << SVG_BOTTOM << bottom  << SVG_QUOTE;
        _cue_id = ""; // only the top staff line gets the extra attributes
    }
    qts << SVG_ELEMENT_END << endl;

    // brackets and sys barline only in frozen pane, stafflines in both
    if (_isFrozen && (isBarLine || _et == EType::BRACKET)) {
        if (brackets[_idxStaff] == 0) 
            brackets[_idxStaff] = new QString;
        qts.setString(brackets[_idxStaff]);
        qts << SVG_4SPACES << qs;
        return;
    }
    else
        stream() << qs;

    // Frozen Pane (horizontal scrolling only), staff lines only
    if (isStaffLines && _hasFrozen) {
        if (_xLeft == 0)
            _xLeft = p.elementAt(0).x + _dx;;

        if (frozenLines[_idxStaff] == 0) { // staff lines draw first
            frozenLines[_idxStaff] = new QString;
            if (_isLinked)
                frozenINameY.insert(_idxStaff, iy + (height / 2) + INAME_OFFSET);
        }

        qts.setString(frozenLines[_idxStaff]);
        qts << SVG_4SPACES << SVG_2SPACES << SVG_PATH << dLine 
            << SVG_ELEMENT_END << endl;
    }
}

void SvgPaintEngine::drawPolygon(const QPointF *points, int pointCount, PolygonDrawMode mode)
{
    Q_ASSERT(pointCount >= 2);

    if (_et == EType::STAFF_LINES || _et == EType::STEM || _et == EType::BAR_LINE
     || _et == EType::LEDGER_LINE || _et == EType::LYRICSLINE_SEGMENT
     || mode != PolylineMode)
    {      // draw it as a <path> element, line text is more compact that way
        QPainterPath path(points[0]);
        for (int i = 1; i < pointCount; ++i)
              path.lineTo(points[i]);
        path.closeSubpath();
        drawPath(path);
    }
    else { // draw it as a <polyline>
        qreal yOff = _dy + (_isFullMatrix ? 0 : _yOffset);
        QString qs;
        QTextStream qts(&qs);
        initStream(&qts);

        if (_isMulti) qts << SVG_4SPACES;  // isMulti staff is inside a <g>

        qts << SVG_POLYLINE << classState << styleState << SVG_POINTS;
        for (int i = 0; i < pointCount; ++i) {
            qts << points[i].x() + _dx << SVG_COMMA << points[i].y() + yOff;
            if (i != pointCount - 1)
                qts << SVG_SPACE;
        }
        qts << SVG_QUOTE << SVG_ELEMENT_END <<endl;

        if (_isFrozen) { // certain bracket types
            if (brackets[_idxStaff] == 0) 
                brackets[_idxStaff] = new QString;
            qts.setString(brackets[_idxStaff]);
            qts << qs; // already indented 4 spaces above
        }
        else
            stream() << qs;
    }
}

void SvgPaintEngine::drawRects(const QRectF *rects, int rectCount)
{
    int h, i, w, x, y, z;
    for (i = 0; i < rectCount; i++) {
        if (_classValue == "tabNote") {
            w = rint(rects[i].width());
            h = rint(rects[i].height());
            if (!(w % 2)) // stems are 3px stroke width
                w++;
            if (h % 2)    // staff lines are 2px stroke-width
                h--;      // opaque: height only needs to block staff line

            z = _e->tick().ticks();
            if (_stemX.find(z) == _stemX.end()) {
                _stemX[z] = rint(_dx + (w / 2)); // as if there was a stem
                x = rint(_dx);
            }
            else
                x = trunc(_stemX[z] - (w / 2));

            z  = static_cast<const Ms::Note*>(_e)->string();
            y  = _staffLinesY[z] - (h / 2);
            if (!_isMulti && abs(y - (rects[i].y() + _dy)) > 99) {
                // PART, move to next system by removing the previous one's staff lines
                _staffLinesY.remove(0, _e->staff()->staffType(Ms::Fraction())->lines());
                y = _staffLinesY[z] - (h / 2);
            }

            stream() << SVG_4SPACES << SVG_PATH  << SVG_D 
                     << SVG_M << x  << SVG_COMMA << y
                     << SVG_H << x + w // everything is in absolute coordinates,
                     << SVG_V << y + h // though this is simpler w/relative vals.
                     << SVG_H << x << SVG_Z << SVG_QUOTE
                     << SVG_ELEMENT_END << endl;
        }
    }
}

void SvgPaintEngine::drawTextItem(const QPointF &p, const QTextItem &textItem)
{
    // Just in case, this avoids crashes
    if (_e == NULL)
        return;

    qreal x, y;
    bool hasTick = true;

    // Variables, constants, initial setup
    if (_isSMAWS && !_isMulti) {
        if (_et == EType::TEXT) {
            switch(Ms::Tid(static_cast<const Ms::Text*>(_e)->subtype())) {
            case Ms::Tid::TITLE :
            case Ms::Tid::SUBTITLE :
                x = ((_sysRight - _sysLeft) / 2) + _sysLeft; // centered
                hasTick = false;
                break;
            case Ms::Tid::COMPOSER :
                x = _sysLeft;
                hasTick = false;
                break;
            case Ms::Tid::POET :
                x = _sysRight;
                hasTick = false;
                break;
            default:
                x = p.x();
                break;
            }
        }
        else {
            x = p.x();
            if (_et == EType::PAGE)  // header/footer
                hasTick = false;
        }
        x += _dx;
    }
    else
        x = p.x() + _dx; // The de-translated coordinates

    y = p.y() + _dy + (_isFullMatrix ? 0 : _yOffset);

    const QFont   font       = textItem.font();
    const QString fontFamily = font.family();
    const QString fontSize   = QString::number(font.pixelSize() != -1
                             ? font.pixelSize()
                             : font.pointSizeF());
    QString qs;
    QTextStream qts(&qs);
    if (_isMulti)
        qts << SVG_4SPACES;

    // Begin the <text>
    qts << SVG_TEXT_BEGIN << classState;

    int pitch = -1;
    bool isRM      = false;
    bool isTab     = false;
    bool isTabNote = false;
    if (hasTick) {
        const Ms::Note* note;
        int tick  = _e->tick().ticks();
        isTab     = _e->staff()->isTabStaff(_e->tick());
        switch (_et) {
        case EType::NOTE :
            note  = static_cast<const Ms::Note*>(_e);
            pitch = note->pitch();
            if (isTab) {
                isTabNote = true;
                if (_stemX.find(tick) != _stemX.end())
                    x = _stemX[tick];
                y = _staffLinesY[note->string()] + 1; //??stroke-width:2; 1 = 2 / 2?
                break;                                //??numbers = no-sub-baseline?
            } // fallthru
        case EType::ACCIDENTAL   :
        case EType::ARTICULATION :
        case EType::HOOK         :
        case EType::NOTEDOT      :
            if (_offsets.find(tick) != _offsets.end()) {
                RealPair& xy = _offsets[tick];
                x += xy.first;
                y += xy.second;
            } // fallthru
        case EType::BRACKET           :
        case EType::CLEF              :
        case EType::GLISSANDO_SEGMENT :
        case EType::HARMONY           : // Chord text/symbols for song book, fake book, etc,
        case EType::INSTRUMENT_CHANGE :
        case EType::INSTRUMENT_NAME   :
        case EType::KEYSIG            :
        case EType::LYRICS            :
        case EType::MEASURE_NUMBER    :
        case EType::REST              :
        case EType::STAFF_TEXT        :
        case EType::TEMPO_TEXT        :
        case EType::TEXT              : // Measure Numbers, Title, Subtitle, Composer, Poet
        case EType::TIMESIG           :
        case EType::TUPLET            :
            break; // These elements all styled by CSS

        case EType::REHEARSAL_MARK : // center the text inside _textFrame
            isRM = true;
            x = _textFrame.x() + (_textFrame.width()  / 2); // width/height are even
            y = _textFrame.y() + (_textFrame.height() / 2); // integers
            break;

        default:
            // Attributes normally contained in styleState. updateState() swaps
            // the stroke/fill values in <text> elements; this is the remedy:
            if (_color != SVG_BLACK)
                qts << SVG_FILL         << _color        << SVG_QUOTE;
            if (_colorOpacity != SVG_ONE)
                qts << SVG_FILL_OPACITY << _colorOpacity << SVG_QUOTE;

            // The font attributes, not handled in updateState()
            qts << SVG_FONT_FAMILY << fontFamily << SVG_QUOTE
                << SVG_FONT_SIZE   << fontSize   << SVG_QUOTE;
            break;
        }
    }

    // Stream the fancily formatted x and y coordinates
    bool isFrBr = _e->isBracket(); // Brackets are frozen pane elements
    if (isRM)
          qts << SVG_X << SVG_QUOTE << int(x) << SVG_QUOTE
          << SVG_Y << SVG_QUOTE << int(y) << SVG_QUOTE;
    else
          qts << formatXY(x, y, isFrBr);

///!!!not for this release...    // If it's a note, stream the pitch value (MIDI note number 0-127)
///!!!not for this release...    if (pitch != -1)
///!!!not for this release...          qts << SVG_DATA_P << pitch << SVG_QUOTE;

    qts << SVG_GT;

    // The Content, as in: <text>Content</text>
    // Some tempo/instrument changes are invisible = no content here, instead
    // it's in the frozen pane file (see code below).
    QString textContent;
    if (_e->visible()) {
        const QString txt = textItem.text();
        for (int i = 0; i < txt.size(); i++) {
            if (txt.at(i).unicode() > 127) {
                textContent.append(XML_ENTITY_BEGIN);
                textContent.append(QString::number(txt.at(i).unicode(), 16).toUpper());
                textContent.append(XML_ENTITY_END);
            }
            else
                textContent.append(txt.at(i));
        }
    }
    qts << textContent << SVG_TEXT_END << endl;

    if (isFrBr) { // frozen brackets don't need to be in the body
        if (brackets[_idxStaff] == 0) 
            brackets[_idxStaff] = new QString;
        qts.setString(brackets[_idxStaff]);
        qts << qs; // already indented 4 spaces above
        return;
    }
    // stream the text element to the body or the leftovers bin
    if (isRM || isTabNote || _e->isTuplet() || _e->isGlissandoSegment())
        _leftovers.append(qs); // text after shape in z-order
    else
        stream() << qs;

    // Frozen Pane elements (except brackets)
    if (_isFrozen || (_e->isTempoText() && _hasFrozen)) {
        bool multiTempo = _isMulti && _e->isTempoText();
        bool isINameY   = frozenINameY.contains(_idxStaff);
        bool isIName    = false;
        bool isTimeSig  = false;
        bool isKeySig   = false;

        const QString key = getDefKey(multiTempo ? _nStaves : _idxStaff, _et);

        FDef*   def;
        QString defClass;
        qreal   line;

        if (!frozenDefs.contains(_cue_id))
            frozenDefs[_cue_id] = new FDef;

        def      = frozenDefs[_cue_id];
        defClass = _classValue;

        // This switch() is MuseScore-draw-order dependent
        switch (_et) {
        case EType::INSTRUMENT_CHANGE :
            _et = EType::INSTRUMENT_NAME; // fallthru - no frozen class="InstrumentChange"
        case EType::INSTRUMENT_NAME :
            isIName  = true;              // linked staves iName only in 1st staff
            defClass = isINameY ? "iNameLink" : _e->name(_et); // fallthru
        case EType::TEMPO_TEXT :
            x = 1;                        // that's as far left as it goes
            break;

        case EType::CLEF :
            x = _xLeft + CLEF_OFFSET;
            defClass = _e->name(_et);     // no frozen class="ClefCourtesy", only "Clef"

            // A change in clef w/o a change in keysig might require y-offset
            line = (qreal)(Ms::ClefInfo::lines(static_cast<const Ms::Clef*>(_e)->clefType())[0])
                 * 2.5; //!!!literal value, related to SPATIUM20???

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
            isKeySig = true;
            if (!def->contains(key) || (*def)[key]->size() == 0) {
                // First accidental in new KeySig:
                // Reset the list of frozen keysigs for this staff
                frozenKeyY[_idxStaff].clear();

                // Reset any Clef-induced y-axis offset
                yOffsetKeySig[_idxStaff] = 0;

                // The x-offset for the ensuing time signature is determined by the number of accidentals
                if (!xOffsetTimeSig.contains(_cue_id) && _e->staff()->isPitchedStaff(Ms::Fraction()))
                    xOffsetTimeSig.insert(_cue_id,
                                          qAbs((int)(static_cast<const Ms::KeySig*>(_e)->keySigEvent().key()))
                                            * 5 * Ms::DPI_F); //!!! literal keysig-accidental-width
            }
            // Natural signs are not frozen
            if (*(textItem.text().unicode()) != NATURAL_SIGN)
                // Set the accidentals in left-to-right order in the vector
                frozenKeyY[_idxStaff].insert(0, y);
            break;
        case EType::TIMESIG :
            isTimeSig = true;
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
        qts.setString(elm);
        initStream(&qts);
        if (_idxStaff != _idxSlash || isTimeSig) {
            if (isKeySig || isTimeSig)
                // KeySigs/TimeSigs simply cache the text content until freezeDef()
                qts << textContent;
            else if (_idxStaff != _idxSlash) {
                // The other element types cache a fully-defined <text> element
                qts << getFrozenElement(textContent, defClass, _et, x, y);

                if (isIName && isINameY) 
                    // The solo (link off) version of instrument names, one for each
                    // staff. Linked staves' first staff's iname gets two elements.
                    qts << getFrozenElement(textContent,
                                            isTab ? CLASS_INAME_TABS : CLASS_INAME_NOTE,
                                            _et,
                                            x,
                                            frozenINameY[_idxStaff]);
            }

            // Ensure that tempo defs have the correct width
            if (multiTempo) {
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

            if (isKeySig) {
                // Set KeySig order: left-to-right (and exclude natural signs)
                // The reverse order is needed for unlinked multi-staff parts too
                if (*(textItem.text().unicode()) != NATURAL_SIGN)
                    (*def)[key]->insert(0, elm);
            }
            else
                (*def)[key]->insert(0, elm); // reverse order because of unlinked multi-staff parts
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
    if (_e == NULL) return eName; // should never be null, but just in case...

    // Element type as SVG/CSS "class"
    switch(_et) {
    case EType::BRACKET :
        eName = (_isLinked ? CLASS_BRACKET_LINK : _e->name(_et));
        break;
    case EType::CLEF :
        // For horizontal scrolling, all but the first clef are courtesy clefs.
        // Unfortunately, everything is in reverse order, so the first clef is
        // the last one to pass through here. cue_id is draw-order-independent.
        eName = (!_isScrollVertical && _cue_id != CUE_ID_ZERO)
                 ? CLASS_CLEF_COURTESY
                 : _e->name(_et);
        break;
    case EType::BAR_LINE : // BarLine sub-types
        if (_e->parent()->type() == EType::SYSTEM) // System start-of-bar line
            eName = _e->name(EType::SYSTEM);  
        else                                       // BarLines by BarLineType
            eName = (BLType::NORMAL == static_cast<const Ms::BarLine*>(_e)->barLineType()
                  ? _e->name(_et)                  // except NORMAL
                  : static_cast<const Ms::BarLine*>(_e)->barLineTypeName());
        break;
    case EType::TEXT : // text sub-types = TextStyleData.name()
        // EType::Text covers a bunch of different MuseScore styles, some have
        // spaces in the name, e.g. "Measure Number". CSS is easier w/o spaces.
        eName = static_cast<const Ms::Text*>(_e)->subtypeName().remove(SVG_SPACE);
        break;
    case EType::STAFF_TEXT :
        eName = QString("%1%2") // To distinguish between Staff and System text
                .arg(static_cast<const Ms::Text*>(_e)->subtypeName())
                .arg("Text");
        break;
    case EType::NOTE :
    case EType::STEM :
    case EType::BEAM :
    case EType::HOOK :
        // Tablature staves get prefixed class names for these element types
        if (_e->staff()->isTabStaff(Ms::Fraction())) {
            eName= QString("%1%2").arg(SVG_PREFIX_TAB).arg(_e->name(_et));
            break;
        } // fallthru - else fall-through to default for these element types
//!!C++17        [[fallthrough]];
    default:
        // For most cases it's simply the element type name
        eName = _e->name(_et);
        break;
    }
    return eName;
}

QString SvgPaintEngine::formatXY(const qreal x, const qreal y, bool isFrozen)
{
    int xDigits = (isFrozen ? 3 : d_func()->xDigits);
    return fixedFormat(SVG_X, x, xDigits, true).append(
           fixedFormat(SVG_Y, y, d_func()->yDigits, true));
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
                                       .arg(SVG_DASH)
                                       .arg(cue_id);

    stream() << SVG_2SPACES << SVG_GROUP_BEGIN
             << SVG_ID      << id                   << SVG_QUOTE
             << SVG_WIDTH   << frozenWidths[cue_id] << SVG_QUOTE
             << SVG_GT      << endl;

    if (_isMulti) {
        if (idx < _nStaves && idx != _idxSlash) {
            // StaffLines/System BarLine(s) in all defs except System and Grid staves
            stream() << SVG_4SPACES << SVG_GROUP_BEGIN 
                     << SVG_CLASS   << "StaffLines" << SVG_QUOTE << SVG_GT  << endl
                     << *(frozenLines[idx]) << SVG_4SPACES << SVG_GROUP_END << endl;
            if (brackets[idx] != 0) 
                stream() << *(brackets[idx]);
        }
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
    int     idx, w;
    QString key;
    qreal   timeX;                            // timesig x-coord
    qreal   keyX = _xLeft + (20 * Ms::DPI_F); // keysig  x-coord
    FDef*   def  = frozenDefs[_cue_id];       // the current frozen def

    // All this just to offset keysigs and timesigs 5px to the right...
    int  tick = _cue_id.left(CUE_ID_FIELD_WIDTH).toInt();
    bool b    = false;
    if (frozenClefs.find(tick) != frozenClefs.end())
        b = true;
    else {
        Int2BoolMap::iterator i;
        for (i = frozenClefs.begin(); i != frozenClefs.end(); ++i) {
            if (i->first > tick)
                break;
            b = i->second;
        }
    }
    if (b)
        keyX += Ms::DPI_F;


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

        if (idx != _idxSlash) { // slash staff frozen pane only has timesigs
            // InstrumentNames are special because of linked staves
            key = getDefKey(idx, EType::INSTRUMENT_NAME);
            if (!def->contains(key)) {
                int id1 = idx - 1;
                if (frozenINameY.contains(idx) && frozenINameY.contains(id1)) {
                    StrPtrList* spl = new StrPtrList;
                    def->insert(key, spl);

                    // Get the def from the previous staff, then parse in y="N"
                    // and class="iNameNote|iNameTabs"
                    QString* qs  = new QString(*(*(*def)[getDefKey(id1, EType::INSTRUMENT_NAME)])[0]);
                    int      el  = qs->indexOf('\n');

                    if (el != -1)
                        qs->remove(0, el + 1);
                    qs->replace(fixedFormat(SVG_Y, frozenINameY[id1], d_func()->yDigits, true),
                                fixedFormat(SVG_Y, frozenINameY[idx], d_func()->yDigits, true));

                    if (_e->staff()->isTabStaff(Ms::Fraction()))
                        qs->replace(CLASS_INAME_NOTE, CLASS_INAME_TABS);
                    else
                        qs->replace(CLASS_INAME_TABS, CLASS_INAME_NOTE);

                    (*def)[key]->insert(0, qs);
                }
                else if (_prevDef != 0 && _prevDef->contains(key))
                    def->insert(key, (*_prevDef)[key]);
            }
            // Clefs
            if (_prevDef != 0) {
                key = getDefKey(idx, EType::CLEF);
                if (!def->contains(key) && _prevDef->contains(key))
                    def->insert(key, (*_prevDef)[key]);
            }
            // KeySigs
            freezeSig(def, idx, frozenKeyY, EType::KEYSIG, keyX);
        }
        // TimeSigs
        timeX = keyX + xOffsetTimeSig[_cue_id] + (5 * Ms::DPI_F); //!!! fixed margin between KeySig/TimeSig. Default setting is 0.5 * spatium, but it ends up more like 3 than 2.5. not sure why.
        freezeSig(def, idx, frozenTimeY, EType::TIMESIG, timeX);

        if (idxStaff > -1)
            break; // Freeze only one staff
    }

    // The width of the entire frozen pane for this _cue_id
    // if (_isMulti) this runs more than once, so we use the widest value.
    // If MuseScore ever allows different keysigs by staff, this code is ready. :-)
    w = rint(timeX + (13 * Ms::DPI_F));                               //!!!13 = timesig width plus 3 for margin/rounding
    if (!frozenWidths.contains(_cue_id) || frozenWidths[_cue_id] < w) //!!!what about 12/8 timesig???
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
    int         i, j, size, half; // only supports grand staff, 2 unlinked staves max
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
    size = (*def)[key]->size();
    half = (isKeySig && _isGrand) ? size / 2 : 0;

    for (i = 0; i < frozenY[idx].size(); i++) {
        if (size == i) {
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
        j = (half && i >= half) ? i - half : i; // grand staff must reset half-way
        qts << getFrozenElement(content,
                                type,
                                eType,
                                x + (isKeySig ? j * 5 * Ms::DPI_F : 0),
                                frozenY[idx][i] + (isKeySig ? yOffsetKeySig[idx] : 0));
    }
}

// Returns a fully defined <text> element for frozen pane def
QString SvgPaintEngine::getFrozenElement(const QString& textContent,
                                         const QString& classValue,
                                         const EType    eType,
                                         const qreal    x,
                                         const qreal    y)
{
    QString     qs;
    QTextStream qts(&qs);
    initStream(&qts);

    qts << SVG_4SPACES << SVG_TEXT_BEGIN << SVG_CLASS;
    qts.setFieldWidth(15);                                // InstrumentName"=15
    qts << QString("%1%2").arg(classValue).arg(SVG_QUOTE);
    qts.setFieldWidth(0);
    qts << fixedFormat(SVG_X, x, 3, true)                 //!! literal value for field width
        << fixedFormat(SVG_Y, y, d_func()->yDigits, true);

    if (eType == EType::INSTRUMENT_NAME) {
        QStringList list = textContent.split(SVG_COMMA);
        qts << SVG_INAME;
        if (list.size() == 1) // instrument name
            qts << _multiTitle[_idxStaff] << SVG_QUOTE;
        else                  // instrument change
            qts << list[1] << SVG_QUOTE;
    }

    qts << SVG_GT << textContent << SVG_TEXT_END << endl;
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
    if (d->engine->isActive()) {
        qWarning("SvgGenerator::setSize(), cannot set size while SVG is being generated");
        return;
    }
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
    if (d->engine->isActive()) {
        qWarning("SvgGenerator::setViewBox(), cannot set viewBox while SVG is being generated");
        return;
    }
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
        qWarning("SvgGenerator::setFileName(), cannot set file name while SVG is being generated");
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
        qWarning("SvgGenerator::setOutputDevice(), cannot set output device while SVG is being generated");
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
        return rint(d->engine->size().height() / Ms::DPMM);
    case QPaintDevice::PdmWidthMM:
        return rint(d->engine->size().width() / Ms::DPMM);
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
    pe->_et = e->type();
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
    setScrollVertical() function
    Sets the _isScrollVertical variable in SvgPaintEngine.
    Must be called *after* setSMAWS()
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setScrollVertical(bool isVertical) {
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());
    pe->_isScrollVertical = isVertical;
    if (!isVertical && pe->_isSMAWS)
        pe->_hasFrozen = true;
}

/*!
    isScrollVertical() function
    Gets the _isScrollVertical variable in SvgPaintEngine.
    Called by paintStaffLines() in mscore/file.cpp.
*/
bool SvgGenerator::isScrollVertical() {
    return static_cast<SvgPaintEngine*>(paintEngine())->_isScrollVertical;
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
    pe->brackets.resize(n);
    pe->yLineKeySig.resize(n);
    pe->yOffsetKeySig.resize(n);
    for (int i = 0; i < n; i++) {
        pe->frozenLines[i]   = 0; // QString*
        pe->brackets[i]      = 0; // QString*
        pe->yOffsetKeySig[i] = 0; // qreal
    }
}

/*!
    setStaffLines() function
    Sets the _nLines variable in SvgPaintEngine.
    Called by paintStaffLines() in mscore/file.cpp.
*/
void SvgGenerator::setStaffLines(int n) {
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());
    pe->_nLines = n;
    pe->_staffLinesY.clear();
}

/*!
    setStaffIndex() function
    Sets the _idxStaff variable in SvgPaintEngine: current visible-staff index
    Also sets _isGrand and _isLinked, which are staff-level properties
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setStaffIndex(int idx, bool isGrand, bool isLinked) {
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());
    pe->_idxStaff = idx;
    pe->_isGrand  = isGrand;
    pe->_isLinked = isLinked;
    if (idx < pe->_nStaves) {
        // "system" staff does not have key or time signatures
        pe->frozenKeyY[idx].clear();
        pe->frozenTimeY[idx].clear();
    }
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
    frozenClefs() function
    Sets a boolean to true for a cue_id if there is a non-treble clef
    Called by saveSMAWS_Music() in mscore/file.cpp.
*/
void SvgGenerator::frozenClefs(int tick, bool b) {
    Int2BoolMap& ibm = static_cast<SvgPaintEngine*>(paintEngine())->frozenClefs;
    if (b || ibm.find(tick) == ibm.end()) // true or new
        ibm[tick] = b;
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
void SvgGenerator::beginMultiGroup(QStringList* pINames,
                                   QStringList* pFullNames,
                                   const QString& className,
                                   int height,
                                   int top)
{
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());
    QString fullName = (pFullNames == 0 ? 0 : pFullNames->last());
    pe->_isMulti = true;
    pe->_prevDef = 0;
    pe->_staffLinesY.clear();
    if (pINames != 0) {
        pe->_iNames    = pINames;
        pe->_fullNames = pFullNames;
        pe->beginMultiGroup(pINames->last(), fullName, className, height, top);
        if (fullName != 0)
            pe->_multiTitle.append(fullName);
    }
    else // this applies to lyrics pseudo-staves
        pe->beginMultiGroup(pe->_iNames->last() + className, pe->_iNames->last(),
                            className, height, top);
}

/*!
    endGroup() function (SMAWS)
    Calls SvgPaintEngine::endGroup() to stream the necessary text
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::endGroup(int indent, bool isFrozen) {
    static_cast<SvgPaintEngine*>(paintEngine())->endGroup(indent, isFrozen);
}

/*!
    beginMouseGroup() function (SMAWS)
    Calls SvgPaintEngine::beginMouseGroup() to stream the necessary text.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::beginMouseGroup() {
    static_cast<SvgPaintEngine*>(paintEngine())->beginMouseGroup();
}

/*!
    beginGroup() function (SMAWS)
    Calls SvgPaintEngine::beginMouseGroup() to stream the necessary text.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::beginGroup(int indent, bool isFrozen) {
    static_cast<SvgPaintEngine*>(paintEngine())->beginGroup(indent, isFrozen);
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
    createMultiUse() function
    Streams the <use> elements for Multi-Select Staves frozen pane file only
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::createMultiUse(qreal y) {
    static_cast<SvgPaintEngine*>(paintEngine())->createMultiUse(y);
}

/*!
    setLeftRight() function
    Sets the x-coordinates for the left and right edges of a part's system
    Called by paintStaffLines() in mscore/file.cpp.
*/
void SvgGenerator::setLeftRight(qreal left, qreal right) {
    static_cast<SvgPaintEngine*>(paintEngine())->_sysLeft  = left;
    static_cast<SvgPaintEngine*>(paintEngine())->_sysRight = right;
}
