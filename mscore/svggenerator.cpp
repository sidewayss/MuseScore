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
// For the elements that need transformations other than translation
    QTransform _transformer;

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
    using Str2IntMap = QMap<QString, int>;
    using StrPtrList = QList<QString*>;
    using FDef  = QMap<QString, StrPtrList*>;       // Key    = idxStaff+EType
    using FDefs = QMap<QString, FDef*>; //by cue_id // Values = <text> elements

    Str2IntMap   frozenWidths;
    FDefs        frozenDefs;
    FDef*        prevDef;
    QString      frozenLines;
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
    bool    _isSMAWS;          // In order to use SMAWS code only as necessary
    bool    _isScrollVertical; // Only 2 axes: x = false, y = true.
    qreal   _cursorTop;        // For calculating the height of (vertical bar)
    qreal   _cursorHeight;     // sheet music playback position cursor.
    int     _startMSecs;       // The elements start time in milliseconds -Yes, it kind of duplicates _cue_id, but it serves a different purpose for now. an oddly important yet minor kludge.

////////////////////
// for Frozen Pane:
//
    bool    _isFrozen; // Is _e part of a frozen pane?
    int     _nStaves;  // Number of staves in the current score

    // These vary by Staff:
    using RealVect     = QVector<qreal>;
    using RealList     = QList<qreal>;
    using RealVectList = QVector<RealList>;

    RealVectList frozenKeyY;  // vector by staff, list = y-coords left-to-right
    RealVectList frozenTimeY; // vector by staff, list = y-coords top-to-bot
    RealVect   yLineKeySig;   // vector by staff, clef's start "staff line" for first accidental (range = 0-9 for 5-line staff)
    RealVect yOffsetKeySig;   // vector by staff, non-zero if clef changes

    // These do not vary by Staff:
    qreal xOffsetTimeSig; // TimeSig x-coord varies by KeySig.

    // Completes open fDef in frozenDefs. Called by SvgGenerator::freezeIt().
    void freezeDef();
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

        prevDef        = 0; // FDef*
        _xLeft         = 0; // qreal
        _nStaves       = 0; // int
        xOffsetTimeSig = 0; // qreal
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

    // Stream the headers
    stream().setString(&d->header);

    // The entire reason for waiting to stream the headers until end() is to
    // set the scroll axis [this used to happen in begin()].
    const QString scrollAxis = _isScrollVertical ? "y" : "x";

    // Standard SVG attributes
    stream() << XML_STYLESHEET << endl
             << SVG_BEGIN      << XML_NAMESPACE
             << SVG_PRESERVE_ASPECT << SVG_XYMIN_SLICE << SVG_QUOTE << endl
             << SVG_VIEW_BOX    << d->viewBox.left()   << SVG_SPACE
                                << d->viewBox.top()    << SVG_SPACE
                                << d->viewBox.width()  << SVG_SPACE
                                << d->viewBox.height() << SVG_QUOTE
             << SVG_WIDTH       << d->size.width()     << SVG_QUOTE
             << SVG_HEIGHT      << d->size.height()    << SVG_QUOTE
     // Custom attributes/values for SMAWS
             << SVG_POINTER_EVENTS                     << endl
             << SVG_CURSOR      // to avoid I-Beam text cursor
             << SVG_CLASS       << SMAWS               << SVG_QUOTE
             << SVG_SCROLL      << scrollAxis          << SVG_QUOTE << endl
             << SVG_ATTR        << SVG_GT              << endl
     // Document attributes
             << SVG_TITLE_BEGIN << d->attributes.title << SVG_TITLE_END << endl
             << SVG_DESC_BEGIN  << d->attributes.desc  << SVG_DESC_END  << endl;

    // Point the stream at the real output device (the .svg file)
    stream().setDevice(d->outputDevice);
    stream().setCodec(_codec);

    // Stream our strings out to the device, in order
    stream() << d->header;
    stream() << d->body;

    // SMAWS: 1) vertical bar cursor to indicate playback position, off-canvas
    // until playback begins (see negative x-coordinate below). Fill and width
    // are set by the container.
    //        2) two <rect> elements (left/right) for graying out inactive bars
    if (_isSMAWS) {
        stream() << SVG_RECT
                    << SVG_CLASS          << CLASS_CURSOR  << SVG_QUOTE
                    << SVG_X << SVG_QUOTE << "-5"          << SVG_QUOTE
                    << SVG_WIDTH          << SVG_ZERO      << SVG_QUOTE
                    << SVG_Y << SVG_QUOTE << _cursorTop    << SVG_QUOTE
                    << SVG_HEIGHT         << _cursorHeight << SVG_QUOTE
                 << SVG_ELEMENT_END << endl;

        if (!_isScrollVertical) {
            for (int i = 0; i < 2; i++)
                stream() << SVG_RECT
                            << SVG_CLASS          << CLASS_GRAY  << SVG_QUOTE
                            << SVG_X << SVG_QUOTE << SVG_ZERO    << SVG_QUOTE
                            << SVG_Y << SVG_QUOTE << SVG_ZERO    << SVG_QUOTE
                            << SVG_WIDTH          << SVG_ZERO    << SVG_QUOTE
                            << SVG_FILL_OPACITY   << SVG_ZERO    << SVG_QUOTE
                            << SVG_HEIGHT << d->viewBox.height() << SVG_QUOTE
                         << SVG_ELEMENT_END << endl;
        }
    }

    // End the <svg> element
    stream() << SVG_END << endl;

    // Clean up
    delete &(stream());

    // Deal with Frozen Pane, if it exists
    if (_isSMAWS && !_isScrollVertical) {
        frozenFile.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream qts;
        qts.setDevice(&frozenFile);
        qts.setCodec(_codec);
        initStream(&qts);

        // Standard SVG headers plus XLink and some indentation
        qts << XML_STYLESHEET  << endl
            << SVG_BEGIN       << XML_NAMESPACE << endl
            << SVG_4SPACES     << XML_XLINK     << endl
            << SVG_4SPACES     << SVG_PRESERVE_ASPECT
                               << SVG_XYMIN_SLICE << SVG_QUOTE << endl
            << SVG_4SPACES     << SVG_VIEW_BOX <<   0 << SVG_SPACE
                                               <<   0 << SVG_SPACE
                                               << 100 << SVG_SPACE //!!! literal value: max keysig width (7-flats) + etc. - it works for me for now.
                                               << d->viewBox.height() << SVG_QUOTE
                               << SVG_WIDTH    << 100                 << SVG_QUOTE
                               << SVG_HEIGHT   << d->size.height()    << SVG_QUOTE
                               << SVG_GT << endl
            << SVG_TITLE_BEGIN << "Frozen Pane for "
                               << d->attributes.title << SVG_TITLE_END << endl
            << SVG_DESC_BEGIN  << d->attributes.desc  << SVG_DESC_END  << endl;

        // <defs>
        const QString tempoKey = QString("0%1").arg(int(EType::TEMPO_TEXT));
        FDefs::iterator def;
        FDef::iterator  elms;
        qts << SVG_DEFS_BEGIN  << endl;
        for (def = frozenDefs.begin(); def != frozenDefs.end(); ++def) {
            qts << SVG_4SPACES << SVG_GROUP_BEGIN
                << SVG_ID      << def.key()               << SVG_QUOTE
                << SVG_WIDTH   << frozenWidths[def.key()] << SVG_QUOTE
                << SVG_GT      << endl;

            // Tempo first - only once per def, not per staff (unneccesary, but it looks nice)
            qts << *(def.value()->value(tempoKey)->value(0));
            for (elms  = def.value()->begin(); elms != def.value()->end(); ++elms) {
                if (elms.key() != tempoKey) {
                    for (int i = 0; i < elms.value()->size();  i++)
                        qts << *(elms.value()->value(i));
                }
            }
            qts << SVG_4SPACES << SVG_GROUP_END << endl;
        }
        qts << SVG_DEFS_END    << endl       // </defs>
            << frozenLines                   // StaffLines and System bar lines
            << SVG_USE         << XLINK_HREF // <use> links to <g> by HREF=cue_id
            << CUE_ID_ZERO     << SVG_QUOTE  // Initial value is start of score
            << SVG_ELEMENT_END << endl       // Only one <use> element in the file
            << SVG_END         << endl;      // Terminate the SVG

        // Write and close the Frozen Pane file
        qts.flush();
        frozenFile.close();
    }
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
    if (_cue_id.isEmpty() || _et == EType::BAR_LINE) {
        // No cue id or BarLine = no fancy formatting
        qts << _classValue << SVG_QUOTE;
        if (!_cue_id.isEmpty())
            // But bar lines need cue ids
            qts << SVG_CUE << _cue_id << SVG_QUOTE;
    }
    else {
        // First stream the class attribute, with fancy fixed formatting
        int w = _isFrozen ? 17 : 11;  // Frozen: InstrumentChange" = 17
        qts.setFieldWidth(w);         // Highlight:    Accidental" = 11
        qts << QString("%1%2").arg(_classValue).arg(SVG_QUOTE);
        qts.setFieldWidth(0);
        // Then stream the Cue ID
        qts << SVG_CUE << _cue_id << SVG_QUOTE;
    }

    // Translations, SVG transform="translate()", are handled separately from
    // other transformations such as rotation. Qt translates everything, but
    // other transformations occur rarely. They are included in classState
    // because they affect CSS-styled elements too.
    _transformer = state.transform();
    if (_transformer.m11() == _transformer.m22()    // No scaling
     && _transformer.m12() == _transformer.m21()) { // No rotation, etc.
        // No transformation except translation
        _dx = _transformer.m31();
        _dy = _transformer.m32();
    }
    else {
        // Other transformations are more straightforward with a full matrix
        _dx = 0;
        _dy = 0;
        qts << SVG_MATRIX << _transformer.m11() << SVG_COMMA
                          << _transformer.m12() << SVG_COMMA
                          << _transformer.m21() << SVG_COMMA
                          << _transformer.m22() << SVG_COMMA
                          << _transformer.m31() << SVG_COMMA
                          << _transformer.m32() << SVG_TRANSFORM_END;
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

    stream() << SVG_IMAGE           << classState  << styleState
             << SVG_X << SVG_QUOTE  << r.x() + _dx << SVG_QUOTE
             << SVG_Y << SVG_QUOTE  << r.y() + _dy << SVG_QUOTE
             << SVG_WIDTH           << r.width()   << SVG_QUOTE
             << SVG_HEIGHT          << r.height()  << SVG_QUOTE
             << SVG_PRESERVE_ASPECT << SVG_NONE    << SVG_QUOTE;

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
    // Rehearsal mark frame is rect or circle, no need for a long, complex path
    if (_et == EType::REHEARSAL_MARK) {
        // I can't find a way to determine if it's a rect or a circle here, so
        // I hardcode to the rect style that I like. The size of the rect in
        // MuseScore is wrong to begin with, so this looks the best in the end.
        _textFrame.setX(_dx);
        _textFrame.setY(qMax(_dy - _e->height(), 2.0));
        _textFrame.setWidth(qMax(_e->width() + 2, 16.0));
        _textFrame.setHeight(13);

        stream() << SVG_RECT << classState << styleState;

        streamXY(_textFrame.x(), _textFrame.y());
        stream() << SVG_WIDTH  << _textFrame.width()  << SVG_QUOTE
                 << SVG_HEIGHT << _textFrame.height() << SVG_QUOTE
                 << SVG_RX << SVG_RY << SVG_ELEMENT_END  << endl;
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
        break; // fill-rule styled by CSS
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
        qreal y = ppe.y + _dy;
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
                             << SVG_COMMA << ppeCurve.y + _dy;
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

    if (mode == PolylineMode) {
        stream() << SVG_POLYLINE << classState << styleState << SVG_POINTS;
        for (int i = 0; i < pointCount; ++i) {
            const QPointF &pt = points[i];

            stream() << pt.x() + _dx << SVG_COMMA << pt.y() + _dy;

            if (i != pointCount - 1)
                stream() << SVG_SPACE;

        }
        stream() << SVG_QUOTE << SVG_ELEMENT_END <<endl;

        // For Frozen Pane (horizontal scrolling only):
        // StaffLines and System BarLine(s)
        if (_isFrozen) {
            QTextStream qts(&frozenLines);
            initStream(&qts);

            // These are straight lines, only two points (StaffLines" = 11)
            qts << SVG_POLYLINE  << SVG_CLASS;
            qts.setFieldWidth(11);
            qts << QString("%1%2").arg(_classValue).arg(SVG_QUOTE);
            qts.setFieldWidth(0);
            qts << SVG_POINTS
                << fixedFormat("", points[0].x() + _dx, 3, false) << SVG_COMMA
                << fixedFormat("", points[0].y() + _dy, d_func()->yDigits, false)
                << SVG_SPACE;

            if (_et == EType::STAFF_LINES) {
                if (_xLeft == 0)
                    _xLeft = points[0].x() + _dx;
                qts << 100.00;                                                     //!!! same literal used in ::end()
            }
            else
                qts << fixedFormat("", points[1].x() + _dx, 3, false);

            qts << SVG_COMMA
                << fixedFormat("", points[1].y() + _dy, d_func()->yDigits, false)
                << SVG_QUOTE << SVG_ELEMENT_END << endl;
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
    qreal y = p.y() + _dy;

    const int idxStaff = _e->track() / Ms::VOICES;

    const QFont   font       = textItem.font();
    const QString fontFamily = font.family();
    const QString fontSize   = QString::number(font.pixelSize() != -1
                             ? font.pixelSize()
                             : font.pointSizeF());
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

    // These elements get onClick events and data-start attribute
    switch (_et) {
    case EType::HARMONY        :
    case EType::LYRICS         :
    case EType::NOTE           :
    case EType::NOTEDOT        :
    case EType::REST           :
    case EType::REHEARSAL_MARK :
        stream() << " onclick=\"top.clickMusic(evt)\""
                 << SVG_START << _startMSecs << SVG_QUOTE;
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
    if (_isFrozen)
    {
        const QString key = QString("%1%2").arg(idxStaff).arg(int(_et));

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
            if (yLineKeySig[idxStaff] != line && _cue_id != CUE_ID_ZERO)
                yOffsetKeySig[idxStaff] = line
                                        - yLineKeySig[idxStaff]
                                        + yOffsetKeySig[idxStaff];
            yLineKeySig[idxStaff] = line;
            break;

        case EType::KEYSIG :
            if (!def->contains(key) || def->value(key)->size() == 0) {
                // First accidental in new KeySig:
                // Reset the list of frozen keysigs for this staff
                frozenKeyY[idxStaff].clear();

                // Reset any Clef-induced y-axis offset
                yOffsetKeySig[idxStaff] = 0;

                // The x-offset for the ensuing time signature
                xOffsetTimeSig = qAbs((int)(static_cast<const Ms::KeySig*>(_e)->keySigEvent().key()))
                               * 5; //!!! literal keysig-accidental-width
            }
            // Natural signs are not frozen
            if (*(textItem.text().unicode()) != NATURAL_SIGN)
                // Set the accidentals in left-to-right order in the vector
                frozenKeyY[idxStaff].insert(0, y);
            break;

        case EType::TIMESIG :
            if (!def->contains(key)) {
                // First character in new TimeSig:
                // Reset the list of frozen timesigs for this staff
                frozenTimeY[idxStaff].clear();
            }
            // Add the timesig character to the vector, order not an issue here
            frozenTimeY[idxStaff].append(y);
            break;

        default:
            break;
        }

        QString* elm = new QString;
        QTextStream qts(elm);
        initStream(&qts);
        if (_et == EType::KEYSIG || _et == EType::TIMESIG)
            // KeySigs/TimeSigs simply cache the text content until freezeDef()
            qts << textContent;
        else
            // The other element types cache a fully-defined <text> element
            qts << getFrozenElement(textContent, defClass, x, y);

        // Add the value to the list for key = idxStaff + EType
        if (!def->contains(key))
            def->insert(key, new StrPtrList);

        if (_et == EType::KEYSIG) {
            // Set KeySig order: left-to-right (and exclude natural signs)
            if (*(textItem.text().unicode()) != NATURAL_SIGN)
                def->value(key)->insert(0, elm);
        }
        else
            def->value(key)->append(elm);
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
        if (_e->parent()->type() == EType::SYSTEM) {
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

QString SvgPaintEngine::fixedFormat(const QString& attr,
                                    const qreal    n,
                                    const int      maxDigits,
                                    const bool     withQuotes)
{
    int w;
    w  = maxDigits + SVG_PRECISION + 1; // 1 for decimal point
    w += withQuotes ? 2: 0;

    QString qsN;
    QTextStream qtsN(&qsN);
    initStream(&qtsN);
    qtsN << n;

    QString qs;
    QTextStream qts(&qs);
    qts << attr;
    qts.setFieldAlignment(QTextStream::AlignRight);
    qts.setFieldWidth(w);
    if (withQuotes)
        qts << QString("%1%2%3").arg(SVG_QUOTE).arg(qsN).arg(SVG_QUOTE);
    else
        qts << QString("%1").arg(qsN);

    return qs;
}

// Completes the open frozen pane definition in the frozenDefs collection.
// Conceptually: _cue_id + this function freeze the staves at a moment in time.
// Called by SaveSMAWS() in mscore/file.cpp.
// Called every time (before) _cue_id changes.
// Some class variables are vectors by staff.
// Protected: only called by SvgGenerator::freezeIt()
void SvgPaintEngine::freezeDef()
{
    int         i, idxStaff;
    QString*    elm;
    QString     key, content, type;
    QTextStream qts;
    StrPtrList* spl;
    const qreal timeX =_xLeft + 4 + 16 + xOffsetTimeSig + 3; //!!! fixed margin between KeySig/TimeSig. Default setting is 0.5 * spatium, but it ends up more like 3 than 2.5. not sure why.;

    // If the current cue_id is missing elements, fill them in with prevDef
    FDef* def = frozenDefs[_cue_id];

    // Tempo is staff-independent, always staff zero
    key = QString("0%1").arg(int(EType::TEMPO_TEXT));
    if (!def->contains(key) && prevDef->contains(key))
        def->insert(key, prevDef->value(key));

    for (idxStaff = 0; idxStaff < _nStaves; idxStaff++) {
        // InstrumentNames
        key = QString("%1%2").arg(idxStaff).arg(int(EType::INSTRUMENT_NAME));
        if (!def->contains(key) && prevDef->contains(key))
            def->insert(key, prevDef->value(key));

        // Clefs
        key = QString("%1%2").arg(idxStaff).arg(int(EType::CLEF));
        if (!def->contains(key) && prevDef->contains(key))
            def->insert(key, prevDef->value(key));

        // KeySigs
        key = QString("%1%2").arg(idxStaff).arg(int(EType::KEYSIG));
        type = _e->name(EType::KEYSIG);
        if (!def->contains(key)) {
            spl = new StrPtrList;
            def->insert(key, spl);
        }
        for (i = 0; i < frozenKeyY[idxStaff].size(); i++) {
            if (def->value(key)->size() == i) {
                if (prevDef->contains(key)
                 && prevDef->value(key)->size() > i)
                {                                            // Better than maintaining another vector by staff
                    elm     = prevDef->value(key)->value(i); // EType::KEYSIG = 1 accidental = 1 XML element
                    content = elm->mid(elm->size() - 16, 8); // &#xE260;          =  8 chars
                }                                            // &#xE260;</text>\n = 16 chars
                else
                    content = QString("Staff number %1 has a key signature problem.")
                                      .arg(idxStaff);
                elm = new QString;
                spl->append(elm);
            }
            else {
                elm      = def->value(key)->value(i);
                content = *elm;
                elm->resize(0);
            }
            qts.setString(elm);
            qts << getFrozenElement(content, type,
                        _xLeft + 4 + 16 + (i * 5),                           //!!! literal values: clefOffset + clefWidth. literal value: not SPATIUM20! SPATIUM20 = height, this = width.
                        frozenKeyY[idxStaff][i] + yOffsetKeySig[idxStaff]);
        }

        // TimeSigs
        key = QString("%1%2").arg(idxStaff).arg(int(EType::TIMESIG));
        type = _e->name(EType::TIMESIG);
        if (!def->contains(key)) {
            spl = new StrPtrList;
            def->insert(key, spl);
        }
        for (i = 0; i < frozenTimeY[idxStaff].size(); i++) {
            if (def->value(key)->size() == i) {
                if (prevDef->contains(key)
                 && prevDef->value(key)->size() > i)
                {                                            // Better than maintaining another vector by staff
                    elm     = prevDef->value(key)->value(i); // EType::TIMESIG = 1 char = 1 XML element (no support for sub-divided numerators, like 7/8 as 4+3/8. It's a frozen pane width issue too.)
                    content = elm->mid(elm->size() - 16, 8); // &#xE084;          =  8 chars
                }                                            // &#xE084;</text>\n = 16 chars
                else
                    content = QString("Staff number %1 has a key signature problem.")
                                      .arg(idxStaff);
                elm = new QString;
                spl->append(elm);
            }
            else {
                elm     = def->value(key)->value(i);
                content = *elm;
                elm->resize(0);
            }
            qts.setString(elm);
            qts << getFrozenElement(content, type, timeX,
                                    frozenTimeY[idxStaff][i]);
        }
    }

    // The width of the entire frozen pane for this cue_id
    frozenWidths.insert(_cue_id, qRound(timeX + 12));      //!!! 12 = timesig width plus 2 for margin/rounding

    // For the next time around
    prevDef = def;
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
    setNStaves() function
    Sets the _nStaves variable in SvgPaintEngine.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setNStaves(int n) {
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());
    pe->_nStaves = n;
    pe->frozenKeyY.resize(n);
    pe->frozenTimeY.resize(n);
    pe->yLineKeySig.resize(n);
    pe->yOffsetKeySig.resize(n);
    for (int i = 0; i < n; i++)
        pe->yOffsetKeySig[i] = 0;
}
/*!
    setCursorTop() function
    Sets the _cursorTop variable in SvgPaintEngine.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setCursorTop(qreal top) {
    static_cast<SvgPaintEngine*>(paintEngine())->_cursorTop = top;
}

/*!
    setCursorHeight() function
    Sets the _cursorHeight variable in SvgPaintEngine.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setCursorHeight(qreal height) {
    static_cast<SvgPaintEngine*>(paintEngine())->_cursorHeight = height;
}

/*!
    setCursorHeight() function
    Sets the _cursorHeight variable in SvgPaintEngine.
    Called by saveSMAWS() in mscore/file.cpp.
*/
void SvgGenerator::setStartMSecs(int start) {
    static_cast<SvgPaintEngine*>(paintEngine())->_startMSecs = start;
}

/*!
    freezeIt() function (SMAWS)
    Calls SvgPaintEngine::freezeDef() to complete a frozen pane def
    Called by saveSVG() in mscore/file.cpp.
*/
void SvgGenerator::freezeIt() {
    static_cast<SvgPaintEngine*>(paintEngine())->freezeDef();
}
