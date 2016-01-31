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

// In updateState() fill/stroke (brush/pen) attributes are switched for text
    QString _color;
    QString _colorOpacity;

// For centering text in frames. The frame is drawn first, before the text.
    QRectF _textFrame;

// Set by updateState(), used various places
    QString  _classValue;

// For UpdateState() and the drawXXX() functions.
    QString     classState; // all elements
    QString     styleState; // only: Non-<text> elements, Not styled by CSS

// For Frozen Pane (SMAWS horizontal scrolling only)
    QString      frozenLines;
    QStringList  frozenDefs;
    QTextStream* frozenStream;
    QFile        frozenFile;
    qreal        _xLeft; // StaffLines left x-coordinate, to align Clefs/Signatures

// Gets the SVG class attribute for an element
    QString getClass();

// For fancy text formatting inside the SVG file
    void streamXY(qreal x, qreal y);
    QString fixedFormat(const QString& attr, const qreal n, const int maxDigits);

//  For Frozen Pane, returns a <def>'s <g> element's attributes string.
    QString defGroup(const QString& groupID,
                     const int      idxStaff,
                     const QString& classValue = QString(""));

protected:
    const Ms::Element* _e;  // The Ms::Element being generated now
          EType        _et; // That element's ::Type - it's used everywhere

// SMAWS only:
    QString _cue_id;           // The current VTT cue ID
    bool    _isSMAWS;          // In order to use SMAWS code only as necessary
    bool    _isScrollVertical; // Only 2 axes: x = false, y = true.

///////////////////
// for Frozen Pane:
    bool    _isFrozen; // Is _e part of a frozen pane?
    int     _nStaves;  // Number of staves in the current score

    // The ids of all the <use> elements, duplicates removed, sorted by
    // idxStaff-defClass. and terminated with SVG_QUOTE; ready to stream.
    QStringList frozenUseIDs;

    // These vary by Staff:
    using vQReal   = QVector<qreal>;
    using vQString = QVector<QString>;
    // Varies by Clef
    vQReal   yOffsetKeySig;
    // Varies by Staff y-coordinate and yOffsetKeySig
    vQString keySigs;
    // For when the Clef changes, but the KeySig does not
    vQReal   yTranslateKeySig;
    // An incremental x-axis value for each accidental in the KeySig
    vQReal   xOffsetAccidental;
    // Varies by Staff y-coordinate only
    vQString timeSigs;
    // Contains values that vary by Staff
    vQString openSigs;

    // These do not vary by Staff:
    // Drum clefs don't have KeySigs, but all TimeSigs must align vertically
    qreal xOffsetTimeSig;
    // For when the KeySig changes, but the TimeSig does not
    qreal xTranslateTimeSig;
    // True if the Key/TimeSig changes within the current _cue_id value
    bool  isNewKeySig;
    bool  isNewTimeSig;

    void freezeSigs(); // Called by SvgGenerator::freezeIt()
// end Frozen Pane
///////////////////

public:
    SvgPaintEngine()
        : QPaintEngine(*new SvgPaintEnginePrivate,
                       svgEngineFeatures())
    {
        _codec = QTextCodec::codecForName("UTF-8");

        _e  = NULL;
        _et = Ms::Element::Type::INVALID;

        _xLeft            = 0;
        _nStaves          = 0;
        frozenStream      = 0;
        xOffsetTimeSig    = 0;
        xTranslateTimeSig = 0;

        isNewKeySig  = false;
        isNewTimeSig = false;
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
    d->stream->setFieldAlignment(QTextStream::AlignLeft);
    d->stream->setRealNumberNotation(QTextStream::FixedNotation);
    d->stream->setRealNumberPrecision(SVG_PRECISION);

    // Set this flag to default value, return
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
    d->stream->setString(&d->header);

    // The entire reason for waiting to stream the headers until end() is to
    // set the scroll axis [this used to happen in begin()].
    const QString scrollAxis = _isScrollVertical ? "y" : "x";
    // This is necessary for frozen pane animation across staves
    const int nStaves =_e->score()->nstaves();

    // Standard SVG attributes
    stream() << XML_STYLESHEET << endl
             << SVG_BEGIN      << XML_NAMESPACE
             << SVG_PRESERVE_ASPECT << SVG_XYMIN_SLICE << SVG_QUOTE << endl
             << SVG_VIEW_BOX    << d->viewBox.left()   << SVG_SPACE
                                << d->viewBox.top()    << SVG_SPACE
                                << d->viewBox.width()  << SVG_SPACE
                                << d->viewBox.height() << SVG_QUOTE
             << SVG_WIDTH       << d->size.width()     << SVG_QUOTE
             << SVG_HEIGHT      << d->size.height()    << SVG_QUOTE << endl
     // Custom attributes/values for SMAWS
             << SVG_CLASS       << SMAWS               << SVG_QUOTE
             << SVG_STAVES      << nStaves             << SVG_QUOTE
             << SVG_SCROLL      << scrollAxis          << SVG_QUOTE << endl
             << SVG_ATTR        << SVG_HI << SVG_LO    << SVG_GT    << endl
     // Document attributes
             << SVG_TITLE_BEGIN << d->attributes.title << SVG_TITLE_END << endl
             << SVG_DESC_BEGIN  << d->attributes.desc  << SVG_DESC_END  << endl;

    // Point the stream at the real output device (the .svg file)
    d->stream->setDevice(d->outputDevice);
    d->stream->setCodec(_codec);

    // Stream our strings out to the device, in order
    stream() << d->header;
    stream() << d->body;
    stream() << SVG_END << endl;
    // Clean up
    delete d->stream;

    // Deal with Frozen Pane, if it exists
    if (frozenStream != 0) {
        frozenFile.open(QIODevice::WriteOnly | QIODevice::Text);
        frozenStream->setDevice(&frozenFile);
        frozenStream->setCodec(_codec);
        // Standard SVG headers plus XLink and some indentation
        *frozenStream << XML_STYLESHEET  << endl
                      << SVG_BEGIN       << XML_NAMESPACE << endl
                      << SVG_4SPACES     << XML_XLINK     << endl
                      << SVG_4SPACES     << SVG_PRESERVE_ASPECT
                                         << SVG_XYMIN_SLICE << SVG_QUOTE << endl
                      << SVG_4SPACES     << SVG_VIEW_BOX <<   0 << SVG_SPACE
                                                         <<   0 << SVG_SPACE
                                                         << 100 << SVG_SPACE
                                                         << d->viewBox.height() << SVG_QUOTE
                                         << SVG_WIDTH    << 100                 << SVG_QUOTE
                                         << SVG_HEIGHT   << d->size.height()    << SVG_QUOTE
                                         << SVG_GT << endl
                      << SVG_TITLE_BEGIN << "Frozen Pane for "
                                         << d->attributes.title << SVG_TITLE_END << endl
                      << SVG_DESC_BEGIN  << d->attributes.desc  << SVG_DESC_END  << endl;
        // <defs>
//        frozenDefs.sort(); // Chronological + class=alphabetical order
        *frozenStream << SVG_DEFS_BEGIN  << endl;
        for (int i = 0, n = frozenDefs.size(); i < n; i++)
            *frozenStream << frozenDefs[i] << SVG_4SPACES << SVG_GROUP_END << endl;
        *frozenStream << SVG_DEFS_END    << endl;

        // StaffLines and System bar lines
        *frozenStream << frozenLines     << endl;

        frozenStream->setFieldAlignment(QTextStream::AlignLeft);

        frozenUseIDs.removeDuplicates();
        frozenUseIDs.sort();
        if (!frozenUseIDs.isEmpty()) {
            for (QStringList::iterator i  = frozenUseIDs.begin();
                                       i != frozenUseIDs.end();
                                     ++i) {
                *frozenStream << SVG_USE << SVG_ID;
                frozenStream->setFieldWidth(19);
                *frozenStream << *i;
                frozenStream->setFieldWidth(0);
                *frozenStream << XLINK_HREF << CUE_ID_ZERO << SVG_DASH;
                frozenStream->setFieldWidth(18);
                *frozenStream << *i;
                frozenStream->setFieldWidth(0);
                *frozenStream  << SVG_ELEMENT_END << endl;
            }
        }

        // </svg> = end of SVG
        *frozenStream << SVG_END << endl;

        // Write and close the Frozen Pane file
        frozenStream->flush();
        frozenFile.close();
    }
    return true;
}

/////////////////////////////////
// SvgPaintEngine::updateState()
/////////////////////////////////
void SvgPaintEngine::updateState(const QPaintEngineState &state)
{
    // Is this element part of a frozen pane?
    _isFrozen =  _isSMAWS
             && !_isScrollVertical
             && (_et == EType::TEMPO_TEXT
              || _et == EType::INSTRUMENT_NAME
              || _et == EType::INSTRUMENT_CHANGE
              || _et == EType::CLEF
              || _et == EType::KEYSIG
              || _et == EType::TIMESIG);

    QTextStream qts;

    // classState = class + optional data-cue, transform attributes
    classState.clear();
    // styleState = all other attributes, only for elements NOT styled by CSS
    styleState.clear();

    // SVG class attribute, based on Ms::Element::Type, among other things
    _classValue = getClass();
    qts.setString(&classState);
    qts.setFieldAlignment(QTextStream::AlignLeft);
    qts << SVG_CLASS;

    if (_cue_id.isEmpty() || _et == EType::BAR_LINE)
        // no cue id or BarLine = no fancy formatting
        qts << _classValue << SVG_QUOTE;
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
            stream() << SVG_MOVE  << x << SVG_COMMA << y;
            break;
        case QPainterPath::LineToElement:
            stream() << SVG_LINE  << x << SVG_COMMA << y;
            break;
        case QPainterPath::CurveToElement:
            stream() << SVG_CURVE << x << SVG_COMMA << y;
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

        // For Frozen Pane (horizontal scrolling only)
        if (frozenStream != 0
        && (_et == EType::STAFF_LINES
            || (_et == EType::BAR_LINE
                && _e->parent()->type() == EType::SYSTEM)))
        {
            // These are straight lines, only two points
            frozenStream->setString(&frozenLines);
            frozenStream->setRealNumberNotation(QTextStream::FixedNotation);
            frozenStream->setRealNumberPrecision(SVG_PRECISION);
            *frozenStream << SVG_POLYLINE << SVG_CLASS << getClass() << SVG_QUOTE << SVG_SPACE
                            << SVG_POINTS << points[0].x() + _dx << SVG_COMMA
                                          << points[0].y() + _dy << SVG_SPACE;

            if (_et == EType::STAFF_LINES) {
                if (_xLeft == 0)
                    _xLeft = points[0].x() + _dx;

                *frozenStream << 100;
            }
            else
                *frozenStream << points[1].x() + _dx;

            *frozenStream << SVG_COMMA << points[1].y() + _dy
                          << SVG_QUOTE << SVG_ELEMENT_END <<endl;
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
        // except the custom "data-staff"attribute for frozen pane elements:
        if (_isFrozen)
            stream() << SVG_STAFF << idxStaff << SVG_QUOTE;
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

    streamXY(x, y);     // Stream the fancily formatted x and y coordinates
    stream() << SVG_GT; // end attributes

    // The content, as in: <text>content</text>:
    // This string, along with textItem.text().constData() go missing at some
    // point prior to the Frozen Pane code,...
    QString     textContent;
    QTextStream streamContent(&textContent);
    ///...so this const string maintains a const copy of textItem.text().
    const QString item = textItem.text();
    const QChar* data = textItem.text().constData();
    // Some tempo/instrument changes are invisible = no content here, instead
    // it's in the frozen pane file (see code below).
    if (_e->visible()) {
        if (fontFamily.left(6) == "MScore") { //???TODO: should this be for all extended, non-ascii chars?
            // MScore fonts are all Private Use Area unicode chars, nothing
            // alphanumeric, so it's best to render them as hex XML entities.
            // Most are one-char-per-text-element, so it lines up vertically.
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
        QString      defClass;
        const bool groupClass = _et == EType::CLEF
                             || _et == EType::TEMPO_TEXT
                             || _et == EType::INSTRUMENT_NAME
                             || _et == EType::INSTRUMENT_CHANGE;
        if (groupClass) {
            switch (_et) {
            case  EType::CLEF :
                // No frozen class="ClefCourtesy", only "Clef"
                defClass = _e->name(_et);
                break;
            case  EType::INSTRUMENT_CHANGE :
                // No frozen class="InstrumentChange", only "InstrumentName"
                defClass = _e->name(EType::INSTRUMENT_NAME);
                break;
            default:
                // Otherwise just use the existing _classValue value
                defClass = _classValue;
            }
        }
        else // KeySig/TimeSig defined together due to variable width KeySig
            defClass = CLASS_SIGNATURES;

        // The "URL" for the xlink:href="#URL" attribute in the <use> elements
        // is the <defs><g> element's id="URL".
        const QString idStub  = QString("%1%2%3%4")
                                        .arg(_cue_id).arg(SVG_DASH)
                                        .arg(idxStaff).arg(SVG_DASH);
              QString groupID = QString("%1%2")
                                        .arg(idStub)
                                        .arg(defClass);
        const QRegExp rxGroupID(QString("%1%2%3").arg("*")
                                        .arg(groupID).arg("*"),
                                Qt::CaseSensitive,
                                QRegExp::Wildcard);
        const int idxGroupID = frozenDefs.lastIndexOf(rxGroupID);

        QString qs;
        if (idxGroupID != -1) {
            // Multi-element definition already started
            frozenStream->setString(&frozenDefs[idxGroupID]);
        }
        else {
            // New def, stream the group element begin and attributes
            frozenStream->setString(&qs);
            *frozenStream << defGroup(groupID, idxStaff, defClass);

            // Signatures are left open, to be completed in freezeSigs()
            if (!groupClass)
                openSigs[idxStaff] = qs;
            else if (yTranslateKeySig[idxStaff] != 0) {
                // This clef is causing the ensuing Keysig to move vertically,
                // but only if the KeySig is NOT changing within this Cue ID.
                // We must create a new string with a new group element begin +
                // attributes, but it will be replaced in openSigs if the
                // KeySig or TimeSig change within this Cue ID.
                groupID = QString("%1%2").arg(idStub).arg(CLASS_SIGNATURES);
                openSigs[idxStaff] = defGroup(groupID, idxStaff);
            }
        }

        qreal offset;
        // This switch() is MuseScore-draw-order dependent
        switch (_et) {
        case EType::INSTRUMENT_NAME   :
        case EType::INSTRUMENT_CHANGE :
            x = p.x();      // No offset
            break;
        case EType::TEMPO_TEXT :
            x = _xLeft;     // Left-aligns with System barline
            break;
        case EType::CLEF :
            x = _xLeft + 4; //!!! literal value: clefOffset
            break;
        case EType::KEYSIG :
            if (!isNewKeySig) {
                // This code runs max once per _cue_id value (see freezeSigs())
                isNewKeySig = true;
                // Unless the new KeySig has the same number of accidentals as
                // the previous KeySig, the TimeSig moves horizontally.
                offset = qAbs((int)(static_cast<const Ms::KeySig*>(_e)->keySigEvent().key())) * 5; //!!! literal keysig-accidental-width
                if (offset != xOffsetTimeSig) {
                    xTranslateTimeSig = offset - xOffsetTimeSig;
                    xOffsetTimeSig    = offset;
                }
                for (int i = 0; i < _nStaves - 1; i++) {
                    // Reset KeySig text, KeySig contents vary by Clef/Staff.
                    keySigs[i].clear();
                    // Each staff decrements its own offset, per accidental
                    xOffsetAccidental[i] = offset;
                }
            }
            // Natural signs are not frozen
            if (*(item.unicode()) == NATURAL_SIGN)
                return;
            // Accidental x-offset -= 5 because MuseScore renders right-to-left
            xOffsetAccidental[idxStaff] -= 5; //!!! literal value, not SPATIUM20
                                              //!!! SPATIUM20 = height, this = width.
            // Signatures are added in freezeIt(), here we accumulate
            frozenStream->setString(&keySigs[idxStaff]);
            x = _xLeft + 4 + 16 + xOffsetAccidental[idxStaff]; //!!! literal values: clefOffset + clefWidth
            break;
        case EType::TIMESIG :
            if (!isNewTimeSig) {
                // This code runs max once per _cue_id value (see freezeSigs())
                isNewTimeSig = true;
                // Reset TimeSig text, TimeSig y-coordinate varies Staff.
                for (int i = 0; i < _nStaves - 1; i++)
                    timeSigs[i].clear();
            }
            // Signatures are added in freezeSigs(), here we accumulate
            frozenStream->setString(&timeSigs[idxStaff]);
            x = _xLeft + 4 + 16 + xOffsetTimeSig + 2; //!!! 2 = fixed margin between KeySig/TimeSig
            break;
        default:
            break;
        }

        // Stream the text element
        *frozenStream << SVG_4SPACES << SVG_4SPACES << SVG_TEXT_BEGIN
                      << fixedFormat(SVG_X, x, 3) //!!! literal value
                      << fixedFormat(SVG_Y, y, d_func()->yDigits);
        if (!groupClass)
            *frozenStream << SVG_CLASS << _classValue << SVG_QUOTE;
        *frozenStream << SVG_GT;

        // textContent has somehow lost its value by here, so I do it again!???
        // ??? also: This file has no tolerance for non-ascii chars, but there
        //     is very little text content in this file, so it's all entities.
        data = item.constData();
        while (!data->isNull()) {
            *frozenStream << XML_ENTITY_BEGIN
                          << QString::number(data->unicode(), 16).toUpper()
                          << XML_ENTITY_END;
            ++data;
        }

        *frozenStream << SVG_TEXT_END << endl;

        frozenUseIDs << QString("%1%2%3%4").arg(idxStaff).arg(SVG_DASH)
                                           .arg(defClass).arg(SVG_QUOTE);

        // Add the string to the list, if appropriate
        if (idxGroupID == -1 && groupClass)
            frozenDefs.append(qs);
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
        return eName; // e should never be null, but this prevents a crash if it is

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
        // the last one to pass through here. _cue_id is order-independent.
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
    stream() << fixedFormat(SVG_X, x, d_func()->xDigits);
    stream() << fixedFormat(SVG_Y, y, d_func()->yDigits);
}

QString SvgPaintEngine::fixedFormat(const QString& attr,
                                    const qreal    n,
                                    const int      maxDigits)
{
    QString qsN;
    QTextStream qtsN(&qsN);
    qtsN.setRealNumberNotation(QTextStream::FixedNotation);
    qtsN.setRealNumberPrecision(SVG_PRECISION);
    qtsN << n;

    QString qs;
    QTextStream qts(&qs);
    qts << attr;
    qts.setFieldAlignment(QTextStream::AlignRight);
    qts.setFieldWidth(SVG_PRECISION + 3 + maxDigits); // 3 = . + (2 x ")
    qts << QString("%1%2%3").arg(SVG_QUOTE).arg(qsN).arg(SVG_QUOTE);

    return qs;
}

QString SvgPaintEngine::defGroup(const QString &groupID,
                                 const int     idxStaff,
                                 const QString &classValue)
{
    QString     qs;
    QTextStream qts(&qs);
    qreal       offset;

    qts << SVG_4SPACES << SVG_GROUP_BEGIN
        << SVG_ID << groupID << SVG_QUOTE;
    if (!classValue.isEmpty()) {
        qts << SVG_CLASS << classValue << SVG_QUOTE;
        if (_et == EType::CLEF) {
            offset = (qreal)(Ms::ClefInfo::lines(static_cast<const Ms::Clef*>(_e)->clefType())[0])
                    * 2.5; //!!!This is SPATIUM20 / 2, but I should really get the score's spatium
            if (yOffsetKeySig[idxStaff] != offset) {
                // C/Am is the absence of a KeySig, so a score can start w/o a
                // KeySig. Not true for Clef of TimeSig. And one would never
                // want to translate the first anything, no reference yet.
                if (_cue_id != CUE_ID_ZERO)
                    yTranslateKeySig[idxStaff] = offset - yOffsetKeySig[idxStaff];
                yOffsetKeySig[idxStaff] = offset;
            }
        }
    }
    qts << SVG_GT << endl;
    return(qs);
}

// Completes open Signatures definitions.
// Called by SaveSMAWS() in mscore/file.cpp.
// Called every time (before) _cue_id changes.
// Some class variables are vectors by staff.
// _cue_id and this function group the staves at a moment in time.
void SvgPaintEngine::freezeSigs() // protected, only called by SvgGenerator::freezeIt()
{
    QTextStream qts;

    for (int i = 0; i < _nStaves; i++) {
        if (openSigs[i].isEmpty())
            continue;
        qts.setString(&openSigs[i]);

        // KeySigs
        if (isNewKeySig)
            qts << keySigs[i];     // Stream the new KeySig
        else if (yTranslateKeySig[i] != 0) {
            // New inner group w/transform="translate()":
            qts << SVG_4SPACES       << SVG_4SPACES << SVG_GROUP_BEGIN
                << SVG_TRANSLATE     << "0 "        << yTranslateKeySig[i]
                << SVG_TRANSFORM_END << SVG_GT      << endl;
            qts << keySigs[i];     // Stream the old KeySig
        }

        // TimeSigs
        if (isNewTimeSig) {
            if (!isNewKeySig)
                qts << keySigs[i]; // Stream the old KeySig
            qts << timeSigs[i];    // Stream the new TimeSig
        }
        else if (xTranslateTimeSig != 0) {
            // xTranslateTimeSig is only non-zero if isNewKeySig = true.
            // New inner group w/transform="translate()":
            qts << SVG_4SPACES       << SVG_4SPACES       << SVG_GROUP_BEGIN
                << SVG_TRANSLATE     << xTranslateTimeSig << " 0"
                << SVG_TRANSFORM_END << SVG_GT            << endl;
            qts << timeSigs[i];    // Stream the old TimeSig
        }
        else if (yTranslateKeySig[i] != 0)
            qts << timeSigs[i];    // Stream the old TimeSig

        // Add the completed Signatures def to frozenDefs
        frozenDefs.append(openSigs[i]);

        // Reset for next Signatures
        openSigs[i].clear();
    }
    isNewKeySig  = false;
    isNewTimeSig = false;
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
    Called by saveSVG() in mscore/file.cpp.
*/
void SvgGenerator::setSMAWS() {
    static_cast<SvgPaintEngine*>(paintEngine())->_isSMAWS = true;
}

/*!
    setCueID() function (SMAWS)
    Sets the _cue_id variable in SvgPaintEngine.
    Called by saveSVG() in mscore/file.cpp.
*/
void SvgGenerator::setCueID(const QString& qs) {
    static_cast<SvgPaintEngine*>(paintEngine())->_cue_id = qs;
}
/*!
    setScrollAxis() function (SMAWS)
    Sets the _isScrollVertical variable in SvgPaintEngine.
    Called by saveSVG() in mscore/file.cpp.
*/
void SvgGenerator::setScrollAxis(bool axis) {
    // Set the member variable
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());
    pe->_isScrollVertical = axis;

    // Deal with Frozen Pane (horizontal scrolling only)
    // If you want this to work, you must call setSMAWS() beforehand.
    if (!axis && static_cast<SvgPaintEngine*>(paintEngine())->_isSMAWS) {
        // The Frozen Pane file
        const QString fn = d_func()->fileName;
        pe->frozenFile.setFileName(QString("%1_frz.svg")
                                   .arg(fn.left(fn.length() - 4)));
        // The Frozen Pane stream
        pe->frozenStream = new QTextStream();
        pe->frozenStream->setFieldAlignment(QTextStream::AlignLeft);
        pe->frozenStream->setRealNumberNotation(QTextStream::FixedNotation);
        pe->frozenStream->setRealNumberPrecision(SVG_PRECISION);
    }
}
/*!
    setNStaves() function (SMAWS)
    Sets the _nStaves variable in SvgPaintEngine.
    Called by saveSVG() in mscore/file.cpp.
*/
void SvgGenerator::setNStaves(int n) {
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());
    pe->_nStaves = n;
    pe->keySigs.resize(n);
    pe->timeSigs.resize(n);
    pe->openSigs.resize(n);
    pe->yOffsetKeySig.resize(n);
    pe->yTranslateKeySig.resize(n);
    pe->xOffsetAccidental.resize(n);
}
/*!
    freezeIt() function (SMAWS)
    Calls SvgPaintEngine::freezeSigs() to complete and stream frozen pane defs
    Called by saveSVG() in mscore/file.cpp.
*/
void SvgGenerator::freezeIt() {
    static_cast<SvgPaintEngine*>(paintEngine())->freezeSigs();
}
