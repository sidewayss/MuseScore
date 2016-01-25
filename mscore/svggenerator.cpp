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
    // For UpdateState(), handles repeated sub-elements within an element
    QString     stateString;
    QTextStream stateStream;

    // For Frozen Pane (horizontal scrolling only)
    QString      frozenLines;
    QStringList  frozenDefs;
    QTextStream* frozenStream;
    QFile        frozenFile;

// To eliminate transform attribute on polylines
    qreal _dx;
    qreal _dy;

// Because fill/stroke (brush/pen) attributes are switched for text
    QString _color;
    QString _colorOpacity;

// For centering text in frames. Fortunately the frame is drawn first, because
// class Text is not visible from here, so I can't get the data from _element.
    QRectF _textFrame;

// Gets the SVG class attribute for an element
    QString getClass(const Ms::Element* e);

// For fancy text formatting inside the SVG file
    void streamXY(qreal x, qreal y);
    QString fixedFormat(const QString& attr, const qreal n, const int maxDigits);

protected:
    const Ms::Element* _element = NULL; // The Ms::Element being generated now

    QString _cue_id;  // The current VTT cue ID
    bool _scrollAxis; // Scroll Axis = bool; only 2 axes: x(false), y(true).

public:
    SvgPaintEngine()
        : QPaintEngine(*new SvgPaintEnginePrivate,
                       svgEngineFeatures()),
          stateStream(&stateString)
    {
        frozenStream = 0;
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

    // Initialize the stream, for other functions to populate
    d->stream = new QTextStream(&d->body);
    d->stream->setFieldAlignment(QTextStream::AlignLeft);
    d->stream->setRealNumberNotation(QTextStream::FixedNotation);
    d->stream->setRealNumberPrecision(SVG_PRECISION);

    // Set this flag to default value, return
    _scrollAxis = false;
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
    const QString scrollAxis = _scrollAxis ? "y" : "x";
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
             << SVG_SCROLL      << scrollAxis          << SVG_QUOTE << endl
             << SVG_ATTR        << SVG_HI << SVG_LO    << SVG_GT    << endl
             // Document attributes
             << SVG_TITLE_BEGIN << d->attributes.title << SVG_TITLE_END << endl
             << SVG_DESC_BEGIN  << d->attributes.desc  << SVG_DESC_END  << endl;

    // Point the stream at the real output device (the .svg file)
    d->stream->setDevice(d->outputDevice);
    d->stream->setCodec(QTextCodec::codecForName("UTF-8"));

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
        // Standard SVG header plus XLink and some indentation
        *frozenStream << XML_STYLESHEET  << endl
                      << SVG_BEGIN       << XML_NAMESPACE << endl
                      << SVG_4SPACES     << XML_XLINK     << endl
                      << SVG_4SPACES     << SVG_PRESERVE_ASPECT
                                         << SVG_XYMIN_SLICE << SVG_QUOTE << endl
                      << SVG_4SPACES     << SVG_VIEW_BOX <<   0 << SVG_SPACE
                                                         <<   0 << SVG_SPACE
                                                         << 100 << SVG_SPACE
                                                         << d->viewBox.height() << SVG_QUOTE
                                         << SVG_WIDTH    << 200                 << SVG_QUOTE
                                         << SVG_HEIGHT   << d->size.height()    << SVG_QUOTE
                                         << SVG_GT << endl
                      << SVG_TITLE_BEGIN << "Frozen Pane for "
                                         << d->attributes.title << SVG_TITLE_END << endl
                      << SVG_DESC_BEGIN  << d->attributes.desc  << SVG_DESC_END  << endl;
        // Defs
        frozenDefs.sort(); // Chronological order
        *frozenStream << SVG_DEFS_BEGIN  << endl;
        for (int i = 0, n = frozenDefs.size(); i < n; i++)
            *frozenStream << frozenDefs[i] << SVG_4SPACES << SVG_GROUP_END << endl;
        *frozenStream << SVG_DEFS_END    << endl;
        // StaffLines and System bar lines
        *frozenStream << frozenLines     << endl;
        // <use> elements, three per staff
        QStringList types;
        types << QString("%1%2").arg(_element->name(Ms::Element::Type::TEMPO_TEXT)).arg(SVG_QUOTE)
              << QString("%1%2").arg(_element->name(Ms::Element::Type::CLEF)).arg(SVG_QUOTE)
              << QString("%1%2").arg(CLASS_SIGNATURES).arg(SVG_QUOTE);

        frozenStream->setFieldAlignment(QTextStream::AlignLeft);
        for (int i = 0; i < _element->score()->nstaves(); i++) {
            // Tempo does not change across staves. It is types[0].
            for (int j = i > 0 ? 1 : 0; j < types.size(); j++) {
                *frozenStream << SVG_USE << SVG_ID << i << SVG_DASH;
                frozenStream->setFieldWidth(12);
                *frozenStream << types[j];
                frozenStream->setFieldWidth(0);
                *frozenStream << XLINK_HREF << CUE_ID_ZERO << SVG_DASH
                                            << i << SVG_DASH;
                frozenStream->setFieldWidth(11);
                *frozenStream << types[j];
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
    // stateString = Attribute Settings
    stateString.clear();

    // SVG class attribute, based on Ms::Element::Type, among other things
    stateStream << SVG_CLASS << getClass(_element) << SVG_QUOTE;

    // Cue ID for animated elements only
    if (!_cue_id.isEmpty())
        stateStream << SVG_CUE << _cue_id << SVG_QUOTE;

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

        stream() << SVG_RECT << stateString;

        streamXY(_textFrame.x(), _textFrame.y());
        stream() << SVG_WIDTH  << _textFrame.width()  << SVG_QUOTE
                 << SVG_HEIGHT << _textFrame.height() << SVG_QUOTE
                 << SVG_RX << SVG_RY << SVG_ELEMENT_END  << endl;
        return; // That's right, we're done here, no path to draw
    }

    // Not a text frame: draw an actual path
    stream() << SVG_PATH << stateString;

    // fill-rule is here because UpdateState() doesn't have a QPainterPath arg
    // Majority of <path>s use the default value: fill-rule="nonzero"
    switch (eType) {
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

        // For Frozen Pane (horizontal scrolling only)
        const Ms::Element::Type eType = _element->type();
        if (frozenStream != 0 && (eType == Ms::Element::Type::SYSTEM
                               || eType == Ms::Element::Type::STAFF_LINES))
        {
            // These are straight lines, only two points
            frozenStream->setString(&frozenLines);
            frozenStream->setRealNumberNotation(QTextStream::FixedNotation);
            frozenStream->setRealNumberPrecision(SVG_PRECISION);
            *frozenStream << SVG_POLYLINE << stateString << SVG_POINTS
                          << points[0].x() + _dx << SVG_COMMA
                          << points[0].y() + _dy << SVG_SPACE;

            if (eType == Ms::Element::Type::STAFF_LINES)
                *frozenStream << 100;
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
    // Just in case, to avoids crashes
    if (_element == NULL)
        return;

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

    // stateString for text is wrong, so the necessary bits are repeated here.
    QString classValue = getClass(_element);
    stream() << SVG_CLASS;

    if (_cue_id.isEmpty())
        // no cue id = no fancy formatting
        stream() << classValue << SVG_QUOTE;
    else {
        // First stream the class attribute, with fancy fixed formatting
        stream().setFieldWidth(13); // ClefCourtesy is the longest so far at 12, + 1 for ".
        stream() << QString("%1%2").arg(classValue).arg(SVG_QUOTE);
        stream().setFieldWidth(0);

        // Then stream the Cue ID
        stream() << SVG_CUE << _cue_id << SVG_QUOTE;
    }

    bool isFrozen = eType == Ms::Element::Type::CLEF
                 || eType == Ms::Element::Type::KEYSIG
                 || eType == Ms::Element::Type::TEMPO_TEXT
                 || eType == Ms::Element::Type::TIMESIG;
    int idxStaff = _element->track() / Ms::VOICES;

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
    case Ms::Element::Type::TEMPO_TEXT   :
    case Ms::Element::Type::TEXT         : // Measure numbers only, AFAIK
    case Ms::Element::Type::TIMESIG      :
    case Ms::Element::Type::TUPLET       :
        // These elements are all styled by CSS, no need to specify attributes,
        // except for these custom attributes for frozen pane elements.
        if (isFrozen) {
            // data-staff attribute
            stream() << SVG_STAFF << idxStaff << SVG_QUOTE;
            // Tempo changes have an extra custom attribute: "data-tempo"
            if(eType == Ms::Element::Type::TEMPO_TEXT)
                stream() << fixedFormat(SVG_TEMPO,
                                        static_cast<const Ms::TempoText*>(_element)->tempo()
                                         * BPS2BPM,
                                        3); // max bpm is realistically around 450 = 3 digits
        }
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
    if (_element->visible()) { // Some tempo changes are invisible = no content
        if (fontFamily.left(6) == "MScore") { //!!!TODO: should this be for all extended, non-ascii chars?
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
    if (frozenStream != 0 && isFrozen)
    {
        const bool groupClass = eType == Ms::Element::Type::CLEF
                             || eType == Ms::Element::Type::TEMPO_TEXT;

        if (eType == Ms::Element::Type::CLEF) // No frozen courtesy clefs
            classValue = _element->name(eType);

        const QString defClass = groupClass ? classValue : CLASS_SIGNATURES;

        const QString cueStaff = QString("%1%2%3%2%4")
                                         .arg(_cue_id)
                                         .arg(SVG_DASH)
                                         .arg(idxStaff)
                                         .arg(defClass);

        const QRegExp rxCueStaff(QString("%1%2%1").arg("*").arg(cueStaff),
                                 Qt::CaseSensitive,
                                 QRegExp::Wildcard);

        const int idxCueStaff = frozenDefs.lastIndexOf(rxCueStaff);

        QString qs;
        if (idxCueStaff != -1) {
            // Multi-element definition already started
            frozenStream->setString(&frozenDefs[idxCueStaff]);
        }
        else {
            // New def, stream the group element begin + attributes
            frozenStream->setString(&qs);
            *frozenStream << SVG_4SPACES << SVG_GROUP_BEGIN
                          << SVG_ID << cueStaff << SVG_QUOTE;
            if (groupClass)
                *frozenStream << SVG_CLASS << classValue << SVG_QUOTE;
            *frozenStream << SVG_GT << endl;
        }
        // Stream the text element
        *frozenStream << SVG_4SPACES << SVG_4SPACES << SVG_TEXT_BEGIN
                      << fixedFormat(SVG_X, x, 3) << fixedFormat(SVG_Y, y, 4);
        if (!groupClass)
            *frozenStream << SVG_CLASS << classValue << SVG_QUOTE;
        *frozenStream << SVG_GT;

        // textContent has somehow lost its value by here, so I do it again!???
        // ??? also: This file has no tolerance for non-ascii chars, but there
        //     is very little text content in this file, so it's all XML hex:
        data = item.constData();
        while (!data->isNull()) {
            *frozenStream << XML_ENTITY_BEGIN
                          << QString::number(data->unicode(), 16).toUpper()
                          << XML_ENTITY_END;
            ++data;
        }

        *frozenStream << SVG_TEXT_END << endl;

        // Add the string to the list, if appropriate
        if (idxCueStaff == -1)
            frozenDefs.append(qs);
    }
}

// Gets the contents of the SVG class attribute, based on element type/name
QString SvgPaintEngine::getClass(const Ms::Element* e)
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
    // For horizontal scrolling, all but the firt clef are courtesy clefs.
    // Unfortunately, everything is in reverse order, so the first clef is
    // the last one to pass through here. So I use the cue_id.
    if (eType == Ms::Element::Type::CLEF && !_scrollAxis
                                         && _cue_id != CUE_ID_ZERO)
        eName = CLASS_CLEF_COURTESY;

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
/*!
    setScrollAxis() function
    Sets the _scrollAxis variable in SvgPaintEngine.
    Called by saveSVG() in mscore/file.cpp.
*/
void SvgGenerator::setScrollAxis(bool axis) {
    // Set the member variable
    SvgPaintEngine* pe = static_cast<SvgPaintEngine*>(paintEngine());
    pe->_scrollAxis = axis;

    // Deal with Frozen Pane (horizontal scrolling only)
    if (!axis) {
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
