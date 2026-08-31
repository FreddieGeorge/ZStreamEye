#include "ui/VideoCanvas.h"

#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineF>
#include <QOpenGLShader>
#include <QPainter>
#include <QResizeEvent>

#include <algorithm>

extern "C" {
#include <libswscale/swscale.h>
}

VideoCanvas::VideoCanvas(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setObjectName(QStringLiteral("VideoCanvas"));
    setMinimumSize(320, 240);
    setAutoFillBackground(false);
    m_overlayMessage = tr("Open an H.264 stream to begin.");
    createOverlayWidgets();
    setOverlayMessage(m_overlayMessage);
}

VideoCanvas::~VideoCanvas()
{
    if (context() != nullptr) {
        makeCurrent();
        if (m_textureId != 0) {
            glDeleteTextures(1, &m_textureId);
        }
        delete m_program;
        m_program = nullptr;
        doneCurrent();
    }

    if (m_swsContext != nullptr) {
        sws_freeContext(m_swsContext);
    }
}

void VideoCanvas::setOverlayMessage(const QString &message)
{
    m_overlayMessage = message;
    if (m_overlayMessageLabel != nullptr) {
        m_overlayMessageLabel->setText(message);
    }
    updateOverlayWidgetVisibility();
    updateOverlayWidgetGeometry();
    update();
}

void VideoCanvas::setPlaybackInfo(const QString &timestamp,
                                  const QString &bitRate,
                                  const QString &resolution,
                                  const QString &codec)
{
    m_timestampValueLabel->setText(timestamp);
    m_bitRateValueLabel->setText(bitRate);
    m_resolutionValueLabel->setText(resolution);
    m_codecValueLabel->setText(codec);
    m_codecValueLabel->setToolTip(codec);
    m_hasPlaybackInfo = true;
    updateOverlayWidgetVisibility();
    updateOverlayWidgetGeometry();
}

void VideoCanvas::clearPlaybackInfo()
{
    m_hasPlaybackInfo = false;
    updateOverlayWidgetVisibility();
}

void VideoCanvas::setFrame(const DecodedVideoFramePtr &frame)
{
    m_currentFrame = frame;
    if (m_currentFrame) {
        m_overlayMessage.clear();
        if (m_overlayMessageLabel != nullptr) {
            m_overlayMessageLabel->clear();
        }
    }
    updateOverlayWidgetVisibility();
    updateOverlayWidgetGeometry();
    update();
}

void VideoCanvas::setAnalysisOverlay(const FrameAnalysis &analysis)
{
    m_currentAnalysis = analysis;
    update();
}

void VideoCanvas::setShowGrid(bool enabled)
{
    m_showGrid = enabled;
    update();
}

void VideoCanvas::setShowQpHeatmap(bool enabled)
{
    m_showQpHeatmap = enabled;
    update();
}

void VideoCanvas::setShowMotionVectors(bool enabled)
{
    m_showMotionVectors = enabled;
    update();
}

void VideoCanvas::setShowPlaybackInfo(bool enabled)
{
    m_showPlaybackInfo = enabled;
    updateOverlayWidgetVisibility();
}

void VideoCanvas::setOverlayOpacity(float opacity)
{
    m_overlayOpacity = std::clamp(opacity, 0.0f, 1.0f);
    update();
}

void VideoCanvas::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);

    m_program = new QOpenGLShaderProgram(this);
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
        "attribute vec2 position;\n"
        "attribute vec2 texCoord;\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 0.0, 1.0);\n"
        "    vTexCoord = texCoord;\n"
        "}\n");
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
        "uniform sampler2D frameTexture;\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(frameTexture, vTexCoord);\n"
        "}\n");
    m_program->link();
}

void VideoCanvas::resizeGL(int width, int height)
{
    Q_UNUSED(width);
    Q_UNUSED(height);
}

void VideoCanvas::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (uploadCurrentFrame()) {
        paintTexture();
        drawAnalysisOverlay();
    }

}

void VideoCanvas::resizeEvent(QResizeEvent *event)
{
    QOpenGLWidget::resizeEvent(event);
    updateOverlayWidgetGeometry();
}

void VideoCanvas::ensureTexture()
{
    if (m_textureId != 0) {
        return;
    }

    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool VideoCanvas::uploadCurrentFrame()
{
    if (!m_currentFrame || m_currentFrame->width <= 0 || m_currentFrame->height <= 0) {
        return false;
    }

    ensureTexture();

    m_swsContext = sws_getCachedContext(
        m_swsContext,
        m_currentFrame->width,
        m_currentFrame->height,
        m_currentFrame->pixelFormat,
        m_currentFrame->width,
        m_currentFrame->height,
        AV_PIX_FMT_RGBA,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr);

    if (m_swsContext == nullptr) {
        setOverlayMessage(tr("Unable to create sws_scale conversion context."));
        return false;
    }

    const int rgbaStride = m_currentFrame->width * 4;
    m_rgbaBuffer.resize(rgbaStride * m_currentFrame->height);

    const uint8_t *srcData[4] = {
        reinterpret_cast<const uint8_t *>(m_currentFrame->planes[0].constData()),
        reinterpret_cast<const uint8_t *>(m_currentFrame->planes[1].constData()),
        reinterpret_cast<const uint8_t *>(m_currentFrame->planes[2].constData()),
        reinterpret_cast<const uint8_t *>(m_currentFrame->planes[3].constData())
    };
    const int srcLineSize[4] = {
        m_currentFrame->lineSize[0],
        m_currentFrame->lineSize[1],
        m_currentFrame->lineSize[2],
        m_currentFrame->lineSize[3]
    };

    uint8_t *dstData[4] = {reinterpret_cast<uint8_t *>(m_rgbaBuffer.data()), nullptr, nullptr, nullptr};
    int dstLineSize[4] = {rgbaStride, 0, 0, 0};

    sws_scale(m_swsContext,
              srcData,
              srcLineSize,
              0,
              m_currentFrame->height,
              dstData,
              dstLineSize);

    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 m_currentFrame->width,
                 m_currentFrame->height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 m_rgbaBuffer.constData());
    glBindTexture(GL_TEXTURE_2D, 0);

    m_textureSize = QSize(m_currentFrame->width, m_currentFrame->height);
    updateOverlayWidgetGeometry();
    return true;
}

void VideoCanvas::paintTexture()
{
    if (m_program == nullptr || !m_program->isLinked() || m_textureId == 0 || m_textureSize.isEmpty()) {
        return;
    }

    const QSizeF widgetSize = size();
    const QSizeF frameSize = m_textureSize;
    const qreal widgetAspect = widgetSize.width() / widgetSize.height();
    const qreal frameAspect = frameSize.width() / frameSize.height();

    qreal xScale = 1.0;
    qreal yScale = 1.0;
    if (frameAspect > widgetAspect) {
        yScale = widgetAspect / frameAspect;
    } else {
        xScale = frameAspect / widgetAspect;
    }

    const GLfloat vertices[] = {
        -static_cast<GLfloat>(xScale), -static_cast<GLfloat>(yScale), 0.0f, 1.0f,
         static_cast<GLfloat>(xScale), -static_cast<GLfloat>(yScale), 1.0f, 1.0f,
        -static_cast<GLfloat>(xScale),  static_cast<GLfloat>(yScale), 0.0f, 0.0f,
         static_cast<GLfloat>(xScale),  static_cast<GLfloat>(yScale), 1.0f, 0.0f,
    };

    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureId);

    m_program->bind();
    m_program->setUniformValue("frameTexture", 0);

    const int positionLocation = m_program->attributeLocation("position");
    const int texCoordLocation = m_program->attributeLocation("texCoord");
    m_program->enableAttributeArray(positionLocation);
    m_program->enableAttributeArray(texCoordLocation);
    m_program->setAttributeArray(positionLocation, GL_FLOAT, vertices, 2, 4 * sizeof(GLfloat));
    m_program->setAttributeArray(texCoordLocation, GL_FLOAT, vertices + 2, 2, 4 * sizeof(GLfloat));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_program->disableAttributeArray(positionLocation);
    m_program->disableAttributeArray(texCoordLocation);
    m_program->release();

    glBindTexture(GL_TEXTURE_2D, 0);
}

void VideoCanvas::drawAnalysisOverlay()
{
    if (m_textureSize.isEmpty()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setOpacity(m_overlayOpacity);
    const QRectF videoRect = videoDisplayRect();

    if (m_showQpHeatmap) {
        drawQpHeatmap(painter, videoRect);
    }
    if (m_showGrid) {
        drawMacroblockGrid(painter, videoRect);
    }
    if (m_showMotionVectors) {
        drawMotionVectors(painter, videoRect);
    }
}

void VideoCanvas::drawQpHeatmap(QPainter &painter, const QRectF &videoRect)
{
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setClipRect(videoRect);

    for (const AnalysisRegion &region : m_currentAnalysis.regions) {
        if (region.kind != AnalysisRegionKind::Macroblock || region.qp < 0) {
            continue;
        }
        const QRectF regionRect = analysisRegionWidgetRect(region, videoRect);
        if (!regionRect.isValid()) {
            continue;
        }
        painter.fillRect(regionRect, qpHeatColor(region.qp));
    }

    painter.restore();
}

void VideoCanvas::drawMacroblockGrid(QPainter &painter, const QRectF &videoRect)
{
    if (m_textureSize.isEmpty()) {
        return;
    }

    const int mbCols = std::max(1, (m_textureSize.width() + 15) / 16);
    const int mbRows = std::max(1, (m_textureSize.height() + 15) / 16);

    painter.save();
    painter.setClipRect(videoRect);
    painter.setPen(QPen(QColor(255, 255, 255, 70), 1.0));

    for (int col = 0; col <= mbCols; ++col) {
        const qreal videoX = std::min(col * 16, m_textureSize.width());
        const QPointF top = mapVideoPointToWidget(QPointF(videoX, 0), videoRect);
        const QPointF bottom = mapVideoPointToWidget(QPointF(videoX, m_textureSize.height()), videoRect);
        painter.drawLine(top, bottom);
    }

    for (int row = 0; row <= mbRows; ++row) {
        const qreal videoY = std::min(row * 16, m_textureSize.height());
        const QPointF left = mapVideoPointToWidget(QPointF(0, videoY), videoRect);
        const QPointF right = mapVideoPointToWidget(QPointF(m_textureSize.width(), videoY), videoRect);
        painter.drawLine(left, right);
    }

    painter.restore();
}

void VideoCanvas::drawMotionVectors(QPainter &painter, const QRectF &videoRect)
{
    painter.save();
    painter.setClipRect(videoRect);

    for (const AnalysisMotionVector &mv : m_currentAnalysis.motionVectors) {
        const QColor vectorColor = mv.list == 1
            ? QColor(255, 80, 220, 220)
            : QColor(80, 220, 255, 220);
        QPen vectorPen(vectorColor, 1.0);
        vectorPen.setCapStyle(Qt::RoundCap);
        vectorPen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(vectorPen);
        painter.setBrush(Qt::NoBrush);

        const QPointF currentCenter(mv.sourceX, mv.sourceY);
        const QPointF referenceBase(
            mv.referenceX >= 0 ? mv.referenceX + 8.0 : currentCenter.x(),
            mv.referenceY >= 0 ? mv.referenceY + 8.0 : currentCenter.y());

        const QPointF predictedPoint(
            referenceBase.x() + mv.mvXQuarterPel / 4.0,
            referenceBase.y() + mv.mvYQuarterPel / 4.0);

        const QPointF start = mapVideoPointToWidget(currentCenter, videoRect);
        const QPointF end = mapVideoPointToWidget(predictedPoint, videoRect);
        const QLineF line(start, end);
        if (line.length() < 1.0) {
            continue;
        }

        painter.drawLine(line);

        const QPointF direction = (end - start) / line.length();
        const QPointF normal(-direction.y(), direction.x());
        const qreal arrowLength = std::min<qreal>(4.0, line.length() * 0.65);
        const qreal arrowHalfWidth = arrowLength * 0.5;
        const QPointF arrowBase = end - direction * arrowLength;
        const QPointF arrowP1 = arrowBase + normal * arrowHalfWidth;
        const QPointF arrowP2 = arrowBase - normal * arrowHalfWidth;
        painter.drawLine(end, arrowP1);
        painter.drawLine(end, arrowP2);
    }

    painter.restore();
}

void VideoCanvas::createOverlayWidgets()
{
    m_overlayMessageLabel = new QLabel(this);
    m_overlayMessageLabel->setObjectName(QStringLiteral("VideoOverlayMessage"));
    m_overlayMessageLabel->setAlignment(Qt::AlignCenter);
    m_overlayMessageLabel->setWordWrap(true);
    m_overlayMessageLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_overlayMessageLabel->setStyleSheet(
        QStringLiteral("QLabel#VideoOverlayMessage {"
                       " color: white; background-color: rgba(12, 15, 18, 205);"
                       " border: 1px solid rgba(255, 255, 255, 45); border-radius: 6px;"
                       " padding: 12px; font-weight: 600; }"));

    m_playbackInfoPanel = new QFrame(this);
    m_playbackInfoPanel->setObjectName(QStringLiteral("PlaybackInfoOverlay"));
    m_playbackInfoPanel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_playbackInfoPanel->setStyleSheet(
        QStringLiteral("QFrame#PlaybackInfoOverlay {"
                       " background-color: rgba(12, 15, 18, 210);"
                       " border: 1px solid rgba(255, 255, 255, 45); border-radius: 6px; }"
                       "QLabel[role=caption] { color: rgba(220, 225, 230, 190); }"
                       "QLabel[role=value] { color: white; font-weight: 600; }"));

    auto *layout = new QGridLayout(m_playbackInfoPanel);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setHorizontalSpacing(14);
    layout->setVerticalSpacing(4);

    const auto addRow = [this, layout](int row, const QString &caption, QLabel **valueLabel) {
        auto *captionLabel = new QLabel(caption, m_playbackInfoPanel);
        captionLabel->setProperty("role", QStringLiteral("caption"));
        auto *value = new QLabel(QStringLiteral("-"), m_playbackInfoPanel);
        value->setProperty("role", QStringLiteral("value"));
        value->setTextInteractionFlags(Qt::NoTextInteraction);
        layout->addWidget(captionLabel, row, 0);
        layout->addWidget(value, row, 1);
        *valueLabel = value;
    };
    addRow(0, tr("Time"), &m_timestampValueLabel);
    addRow(1, tr("Frame bitrate"), &m_bitRateValueLabel);
    addRow(2, tr("Resolution"), &m_resolutionValueLabel);
    addRow(3, tr("Codec"), &m_codecValueLabel);
    layout->setColumnStretch(1, 1);
    m_playbackInfoPanel->hide();
}

void VideoCanvas::updateOverlayWidgetGeometry()
{
    if (m_overlayMessageLabel != nullptr && !m_overlayMessage.isEmpty()) {
        const int availableWidth = std::max(120, width() - 48);
        const int labelWidth = std::min(560, availableWidth);
        m_overlayMessageLabel->setFixedWidth(labelWidth);
        m_overlayMessageLabel->adjustSize();
        const QSize labelSize(labelWidth, m_overlayMessageLabel->height());
        m_overlayMessageLabel->setGeometry(
            (width() - labelSize.width()) / 2,
            (height() - labelSize.height()) / 2,
            labelSize.width(),
            labelSize.height());
        m_overlayMessageLabel->raise();
    }

    if (m_playbackInfoPanel == nullptr || !m_playbackInfoPanel->isVisible()) {
        return;
    }
    const QRectF displayRect = videoDisplayRect();
    constexpr int Margin = 12;
    const int availableWidth = std::max(160, static_cast<int>(displayRect.width()) - Margin * 2);
    const int panelWidth = std::min(310, availableWidth);
    m_playbackInfoPanel->setFixedWidth(panelWidth);
    m_playbackInfoPanel->adjustSize();
    m_playbackInfoPanel->move(static_cast<int>(displayRect.left()) + Margin,
                              static_cast<int>(displayRect.top()) + Margin);
    m_playbackInfoPanel->raise();
}

void VideoCanvas::updateOverlayWidgetVisibility()
{
    if (m_overlayMessageLabel != nullptr) {
        m_overlayMessageLabel->setVisible(!m_overlayMessage.isEmpty());
    }
    if (m_playbackInfoPanel != nullptr) {
        m_playbackInfoPanel->setVisible(m_showPlaybackInfo && m_hasPlaybackInfo && m_currentFrame);
    }
}

QRectF VideoCanvas::videoDisplayRect() const
{
    if (m_textureSize.isEmpty() || width() <= 0 || height() <= 0) {
        return rect();
    }

    const QSizeF widgetSize = size();
    const QSizeF frameSize = m_textureSize;
    const qreal widgetAspect = widgetSize.width() / widgetSize.height();
    const qreal frameAspect = frameSize.width() / frameSize.height();

    QSizeF drawSize = widgetSize;
    if (frameAspect > widgetAspect) {
        drawSize.setHeight(widgetSize.width() / frameAspect);
    } else {
        drawSize.setWidth(widgetSize.height() * frameAspect);
    }

    const QPointF topLeft((widgetSize.width() - drawSize.width()) * 0.5,
                          (widgetSize.height() - drawSize.height()) * 0.5);
    return QRectF(topLeft, drawSize);
}

QPointF VideoCanvas::mapVideoPointToWidget(const QPointF &videoPoint, const QRectF &videoRect) const
{
    if (m_textureSize.isEmpty()) {
        return videoPoint;
    }

    return QPointF(
        videoRect.left() + videoPoint.x() / m_textureSize.width() * videoRect.width(),
        videoRect.top() + videoPoint.y() / m_textureSize.height() * videoRect.height());
}

QRectF VideoCanvas::analysisRegionWidgetRect(const AnalysisRegion &region, const QRectF &videoRect) const
{
    if (region.address < 0 || region.width <= 0 || region.height <= 0 || m_textureSize.isEmpty()) {
        return {};
    }

    const QPointF topLeft = mapVideoPointToWidget(QPointF(region.x, region.y), videoRect);
    const QPointF bottomRight = mapVideoPointToWidget(
        QPointF(std::min(region.x + region.width, m_textureSize.width()),
                std::min(region.y + region.height, m_textureSize.height())),
        videoRect);
    return QRectF(topLeft, bottomRight).normalized();
}

QColor VideoCanvas::qpHeatColor(int qp) const
{
    const qreal t = std::clamp(qp / 51.0, 0.0, 1.0);
    const int red = static_cast<int>(80 + t * 175);
    const int green = static_cast<int>(220 - t * 180);
    return QColor(red, green, 40, 80);
}
