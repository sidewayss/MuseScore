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

// Gets the contents of the SVG class attribute, based on element type/name
static QString getClass(const Ms::Element *e)
{
    Ms::Element::Type eType;
              QString eName;

    // Add element type as "class"
    if (e == NULL)
        return eName; // e should never be null, but this prevents a crash if it is

    eType = e->type();
    eName = e->name(eType);

    // BarLine sub-types = BarLineType enum + parent() == System
    if (eType == Ms::Element::Type::BAR_LINE) {
        if (e->parent()->type() == Ms::Element::Type::SYSTEM) {
            // System BarLines
            eType = e->parent()->type();
            eName = e->name(eType);
        }
        else {
            // All the Measure BarLines by BarLineType
            Ms::BarLineType blt = static_cast<const Ms::BarLine*>(e)->barLineType();
            if  (blt != Ms::BarLineType::NORMAL) {
                // Non-NORMAL BarLineTypes fit in outside the Element::Type box
                eName = static_cast<const Ms::BarLine*>(e)->barLineTypeName();
            }
        }
    }
    return eName;
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

    QIODevice* outputDevice;
    int        resolution;

    struct _attributes {
        QString title;
        QString desc;
    } attributes;

    QString header;
    QString body;

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
    QString     stateString;
    QTextStream stateStream;

// To eliminate transform attribute on polylines
    qreal _dx;
    qreal _dy;

// Because fill/stroke (brush/pen) attributes are switched for text
    QString _color;
    QString _colorOpacity;

// For centering text in frames. Fortunately the frame is drawn first, because
// class Text is not visible from here, so I can't get the data from _element.
    QRectF _textFrame;

// For fancy text formatting inside the SVG file
    void formatXY(qreal x, qreal y);

protected:
// The Ms::Element being generated right now
    const Ms::Element* _element = NULL;

// The current VTT cue ID
    QString _cue_id;

// SVG floating point precision
#define SVG_PRECISION 2

// SVG strings as constants
#define SVG_SPACE    ' '
#define SVG_QUOTE    "\""
#define SVG_COMMA    ","
#define SVG_GT       ">"
#define SVG_PX       "px"
#define SVG_NONE     "none"
#define SVG_EVENODD  "evenodd"
#define SVG_BUTT     "butt"
#define SVG_SQUARE   "square"
#define SVG_ROUND    "round"
#define SVG_MITER    "miter"
#define SVG_BEVEL    "bevel"
#define SVG_ONE      "1"
#define SVG_BLACK    "#000000"

#define SVG_BEGIN    "<svg"
#define SVG_END      "</svg>"

#define SVG_WIDTH    " width=\""
#define SVG_HEIGHT   " height=\""
#define SVG_VIEW_BOX " viewBox=\""

#define SVG_X        " x="
#define SVG_Y        " y="

#define SVG_RX       " rx=\"1\"" // for now these are constant values
#define SVG_RY       " ry=\"1\""

#define SVG_POINTS   " points=\""
#define SVG_D        " d=\""
#define SVG_MOVE     'M'
#define SVG_LINE     'L'
#define SVG_CURVE    'C'

#define SVG_ELEMENT_END "/>"

#define XML_NAMESPACE   " xmlns=\"http://www.w3.org/2000/svg\""
#define XML_STYLESHEET  "<?xml-stylesheet type=\"text/css\" href=\"MuseScore.svg.css\"?>"

#define SVG_TITLE_BEGIN "<title>"
#define SVG_TITLE_END   "</title>"
#define SVG_DESC_BEGIN  "<desc>"
#define SVG_DESC_END    "</desc>"

#define SVG_TEXT_BEGIN  "<text"
#define SVG_TEXT_END    "</text>"

#define SVG_IMAGE       "<image"
#define SVG_PATH        "<path"
#define SVG_POLYLINE    "<polyline"
#define SVG_RECT_BEGIN  "<rect"

#define SVG_PRESERVE_ASPECT " preserveAspectRatio=\""
#define SVG_XYMIN_SLICE     "xMinYMin slice"

#define SVG_FILL            " fill=\""
#define SVG_STROKE          " stroke=\""
#define SVG_STROKE_WIDTH    " stroke-width=\""
#define SVG_STROKE_LINECAP  " stroke-linecap=\""
#define SVG_STROKE_LINEJOIN " stroke-linejoin=\""
#define SVG_STROKE_DASHARRAY " stroke-dasharray=\""
#define SVG_STROKE_DASHOFFSET " stroke-dashoffset=\""
#define SVG_STROKE_MITERLIMIT " stroke-miterlimit=\""

#define SVG_OPACITY         " opacity=\""
#define SVG_FILL_OPACITY    " fill-opacity=\""
#define SVG_STROKE_OPACITY  " stroke-opacity=\""

#define SVG_FONT_FAMILY     " font-family=\""
#define SVG_FONT_SIZE       " font-size=\""

#define SVG_FILL_RULE       " fill-rule=\"evenodd\""
#define SVG_VECTOR_EFFECT   " vector-effect=\"non-scaling-stroke\""

// For extended characters in MScore & other fonts (unicode Private Use Area)
#define XML_ELEMENT_BEGIN   "&#x"
#define XML_ELEMENT_END     ";"

//#define SVG_COMMENT_BEGIN   "<!--"
//#define SVG_COMMENT_END     "-->"

// Custom SVG attributes (and some default settings)
#define SVG_CLASS " class=\""
#define SVG_ENDX  " data-endx=\""
#define SVG_ENDY  " data-endy=\""
#define SVG_CUE   " data-cue=\""
#define SVG_ATTR  " data-attr=\"fill\""  // the only animated attribute so far
#define SVG_HI    " data-hi=\"#0000bb\"" // medium-bright blue
#define SVG_LO    " data-lo=\"#505050\"" // charcoal gray

// SMAWS
#define SMAWS "SMAWS"

public:
    SvgPaintEngine()
        : QPaintEngine(*new SvgPaintEnginePrivate,
                       svgEngineFeatures()),
          stateStream(&stateString)
    {
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
        Q_ASSERT(!isActive());
        d_func()->size = size;
    }
    QRectF viewBox() const { return d_func()->viewBox; }
    void setViewBox(const QRectF &viewBox) {
        Q_ASSERT(!isActive());
        d_func()->viewBox = viewBox;
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
        Q_ASSERT(!isActive());
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

    // Stream the headers
    d->stream = new QTextStream(&d->header);

             // Standard SVG
    stream() << XML_STYLESHEET  << endl
             << SVG_BEGIN       << XML_NAMESPACE
             << SVG_PRESERVE_ASPECT << SVG_XYMIN_SLICE << SVG_QUOTE << endl
             << SVG_VIEW_BOX    << d->viewBox.left()   << SVG_SPACE
                                << d->viewBox.top()    << SVG_SPACE
                                << d->viewBox.width()  << SVG_SPACE
                                << d->viewBox.height() << SVG_QUOTE
             << SVG_WIDTH       << d->size.width()     << SVG_QUOTE
             << SVG_HEIGHT      << d->size.height()    << SVG_QUOTE << endl
             // Custom for SMAWS
             << SVG_CLASS       << SMAWS               << SVG_QUOTE
             << SVG_ENDX        << d->viewBox.width()     << SVG_QUOTE
             << SVG_ENDY        << d->viewBox.height()    << SVG_QUOTE << endl
             << SVG_ATTR        << SVG_HI << SVG_LO    << SVG_GT    << endl
             // Document attributes
             << SVG_TITLE_BEGIN << d->attributes.title << SVG_TITLE_END << endl
             << SVG_DESC_BEGIN  << d->attributes.desc  << SVG_DESC_END  << endl;

    // Point the stream at the body string, for other functions to populate
    d->stream->setString(&d->body);
    d->stream->setRealNumberPrecision(SVG_PRECISION);
    d->stream->setRealNumberNotation(QTextStream::FixedNotation);
    return true;
}

/////////////////////////
// SvgPaintEngine::end()
/////////////////////////
bool SvgPaintEngine::end()
{
    Q_D(SvgPaintEngine);

    // Point the stream at the real output device (the .svg file)
    d->stream->setDevice(d->outputDevice);
    d->stream->setCodec(QTextCodec::codecForName("UTF-8"));

    // Stream our strings out to the device, in order
    stream() << d->header;
    stream() << d->body;
    stream() << SVG_END << endl;

    delete d->stream;
    return true;
}

/////////////////////////////////
// SvgPaintEngine::updateState()
/////////////////////////////////
void SvgPaintEngine::updateState(const QPaintEngineState &state)
{
    // stateString = Attribute Settings
    stateString.clear();

    // Cue ID for animated elements onlyl
    if (!_cue_id.isEmpty())
        stateStream << SVG_CUE << _cue_id << SVG_QUOTE;

    // SVG class attribute, based on Ms::Element::Type, among other things
    stateStream << SVG_CLASS << getClass(_element) << SVG_QUOTE;

    // Set attributes for element types not styled by CSS
    switch (_element->type()) {
    case Ms::Element::Type::ACCIDENTAL         :
    case Ms::Element::Type::ARTICULATION       :
    case Ms::Element::Type::BEAM               :
    case Ms::Element::Type::BRACKET            :
    case Ms::Element::Type::CLEF               :
    case Ms::Element::Type::HARMONY            :
    case Ms::Element::Type::HOOK               :
    case Ms::Element::Type::KEYSIG             :
    case Ms::Element::Type::LEDGER_LINE        :
    case Ms::Element::Type::LYRICS             :
    case Ms::Element::Type::LYRICSLINE_SEGMENT :
    case Ms::Element::Type::NOTE               :
    case Ms::Element::Type::NOTEDOT            :
    case Ms::Element::Type::REHEARSAL_MARK     :
//  case Ms::Element::Type::REST : // Rest <polyline>s can't be handled in CSS. Rest <text> handled in drawTextItem()
    case Ms::Element::Type::SLUR_SEGMENT       :
    case Ms::Element::Type::STAFF_LINES        :
    case Ms::Element::Type::STEM               :
    case Ms::Element::Type::SYSTEM             :
    case Ms::Element::Type::TEXT               :
    case Ms::Element::Type::TIMESIG            :
    case Ms::Element::Type::TREMOLO            :
    case Ms::Element::Type::TUPLET             :
        break; // Styled by CSS

    default:
        // This is the best way so far to style NORMAL bar lines w/o messing up other types
        if (_element->type() == Ms::Element::Type::BAR_LINE
        && static_cast<const Ms::BarLine*>(_element)->barLineType() == Ms::BarLineType::NORMAL)
            break;

        // Brush and Pen attributes
        stateStream << qbrushToSvg(state.brush());
        stateStream <<   qpenToSvg(state.pen());
    }
    // Set these class variables for later use in the drawXXX() functions
    QMatrix mx = state.matrix();
    _dx = mx.dx();
    _dy = mx.dy();
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

    stream() << SVG_IMAGE           << stateString
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
    const Ms::Element::Type eType = _element->type();

    // Rehearsal mark frame is rect or circle, no need for a long, complex path
    if (eType == Ms::Element::Type::REHEARSAL_MARK) {
        // I can't find a way to determine if it's a rect or a circle here, so
        // I hardcode to the rect style that I like. The size of the rect in
        // MuseScore is wrong to begin with, so this looks the best in the end.
        _textFrame.setX(_dx);
        _textFrame.setY(qMax(_dy - _element->height(), 2.0));
        _textFrame.setWidth(qMax(_element->width() + 2, 16.0));
        _textFrame.setHeight(13);

        stream() << SVG_RECT_BEGIN << stateString;

        formatXY(_textFrame.x(), _textFrame.y());
        stream() << SVG_WIDTH  << _textFrame.width()  << SVG_QUOTE
                 << SVG_HEIGHT << _textFrame.height() << SVG_QUOTE
                 << SVG_RX << SVG_RY << SVG_ELEMENT_END  << endl;
        return; // That's right, we're done here, no path to draw
    }

    // Not a text frame: draw an actual path
    stream() << SVG_PATH << stateString;

    // fill-rule is here because UpdateState() doesn't have a QPainterPath arg
    // Majority of <path>s use the default value: fill-rule="nonzero"
    switch (_element->type()) {
    case Ms::Element::Type::BEAM         :
    case Ms::Element::Type::BRACKET      :
    case Ms::Element::Type::SLUR_SEGMENT :
        break; // fill-rule styled by CSS
    default:
        if (p.fillRule() == Qt::OddEvenFill)
            stream() << SVG_FILL_RULE;
        break;
    }
    // Path data
    stream() << SVG_D;
    for (int i = 0; i < p.elementCount(); ++i) {
        const QPainterPath::Element &e = p.elementAt(i);
                               qreal x = e.x + _dx;
                               qreal y = e.y + _dy;
        switch (e.type) {
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
                const QPainterPath::Element &e = p.elementAt(i);
                if (e.type == QPainterPath::CurveToDataElement) {
                    stream() << SVG_SPACE << e.x + _dx
                             << SVG_COMMA << e.y + _dy;
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

    QPainterPath path(points[0]);
    for (int i = 1; i < pointCount; ++i)
        path.lineTo(points[i]);

    if (mode == PolylineMode) {
        stream() << SVG_POLYLINE << stateString
                 << SVG_POINTS;
        for (int i = 0; i < pointCount; ++i) {
            const QPointF &pt = points[i];
            stream() << pt.x() + _dx << SVG_COMMA << pt.y() + _dy;
            if (i != pointCount - 1)
                stream() << SVG_SPACE;
        }
        stream() << SVG_QUOTE << SVG_ELEMENT_END <<endl;
    }
    else { // not PolylineMode
        path.closeSubpath();
        drawPath(path);
    }
}

void SvgPaintEngine::drawTextItem(const QPointF &p, const QTextItem &textItem)
{
    QString qs;            // For fancy fixed-width formats
    QTextStream qts(&qs);

    qreal x = p.x() + _dx; // The de-translated coordinates
    qreal y = p.y() + _dy;

    const Ms::Element::Type eType = _element->type();

    const QFont   font       = textItem.font();
    const QString fontFamily = font.family();
    const QString fontSize   = QString::number(font.pixelSize() != -1
                             ? font.pixelSize()
                             : font.pointSizeF());
    // Begin the <text>
    stream() << SVG_TEXT_BEGIN;

    // stateString for text is wrong, so the necessary bits are repeated here:
    if (!_cue_id.isEmpty())
        stream() << SVG_CUE << _cue_id << SVG_QUOTE;
    if (_element != NULL) {
        // This may seem like a lot of trouble for formatting, but I like it
        qs.clear();
        qts << getClass(_element) << SVG_QUOTE;
        stream() << SVG_CLASS;
        stream().setFieldAlignment(QTextStream::AlignLeft);
        stream().setFieldWidth(14);
        stream() << qs;
        stream().setFieldWidth(0);
    }
    switch (eType) {
    case Ms::Element::Type::REHEARSAL_MARK :
        // This text is centered inside a rect or circle: x and y must relocate
        x = _textFrame.x() + (_textFrame.width()  / 2);
        y = _textFrame.y() + (_textFrame.height() - 3);
        // no break, let it flow
    case Ms::Element::Type::ACCIDENTAL   :
    case Ms::Element::Type::ARTICULATION :
    case Ms::Element::Type::CLEF         :
    case Ms::Element::Type::HARMONY      : // Chord text/symbols for song book, fake book, etc,
    case Ms::Element::Type::HOOK         :
    case Ms::Element::Type::KEYSIG       :
    case Ms::Element::Type::LYRICS       :
    case Ms::Element::Type::NOTE         :
    case Ms::Element::Type::NOTEDOT      :
    case Ms::Element::Type::REST         :
    case Ms::Element::Type::STAFF_TEXT   :
    case Ms::Element::Type::TEXT         : // Measure numbers only, AFAIK
    case Ms::Element::Type::TIMESIG      :
    case Ms::Element::Type::TUPLET       :
        // These elements are all styled by CSS, no need to specify attributes
        break;
    default:
        // The rest of what is stateString elsewhere
        if (_color != SVG_BLACK)
            stream() << SVG_FILL         << _color        << SVG_QUOTE;
        if (_colorOpacity != SVG_ONE)
            stream() << SVG_FILL_OPACITY << _colorOpacity << SVG_QUOTE;

        // The rest of the <text> attributes
        stream() << SVG_FONT_FAMILY << fontFamily << SVG_QUOTE
                 << SVG_FONT_SIZE   << fontSize   << SVG_QUOTE;
        break;
    }
    // Stream the fancily formatted x and y coordinates
    formatXY(x, y);
    stream() << SVG_GT;

    // The <text>Content</text>
    if (fontFamily.left(6) == "MScore") {
        // MScore fonts are all Private Use Area unicode characters, nothing
        // alphanumeric, so it's best to render them as XML entities.
        const QChar *data = textItem.text().constData();
        while (!data->isNull()) {
            stream() << XML_ELEMENT_BEGIN
                     << QString::number(data->unicode(), 16).toUpper()
                     << XML_ELEMENT_END;
            ++data;
        }
    }
    else
        stream() << textItem.text();

    stream() << SVG_TEXT_END << endl; // The terminator
}

void SvgPaintEngine::formatXY(const qreal x, const qreal y)
{
    QString qs;            // For fancy fixed-width formats
    QTextStream qts(&qs);

    stream().setFieldAlignment(QTextStream::AlignRight);
    stream() << SVG_X;
    stream().setFieldWidth(SVG_PRECISION + 8); // x is often > 10,000
    qs.clear();
    qts.setRealNumberNotation(QTextStream::FixedNotation);
    qts.setRealNumberPrecision(SVG_PRECISION);
    qts << SVG_QUOTE << x << SVG_QUOTE;
    stream() << qs;
    stream().setFieldWidth(0);
    stream() << SVG_Y;
    stream().setFieldWidth(SVG_PRECISION + 7); // y is never > 9,999
    qs.clear();
    qts << SVG_QUOTE << y << SVG_QUOTE;
    stream() << qs;
    stream().setFieldWidth(0);
    stream().setFieldAlignment(QTextStream::AlignLeft);
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
    Sets the _element variable in SvgPaintEngine.
    Called by saveSVG() in mscore/file.cpp.
*/
void SvgGenerator::setElement(const Ms::Element* e) {
    static_cast<SvgPaintEngine*>(paintEngine())->_element = e;
}

/*!
    setCueID() function
    Sets the _cue_id variable in SvgPaintEngine.
    Called by saveSVG() in mscore/file.cpp.
*/
void SvgGenerator::setCueID(const QString& qs) {
    static_cast<SvgPaintEngine*>(paintEngine())->_cue_id = qs;
}
