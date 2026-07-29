#pragma once

#include "core/model/DecodedVideoFrame.h"
#include "core/model/FrameAnalysis.h"

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>

class QColor;
class QFrame;
class QLabel;
class QPainter;
class QResizeEvent;
struct SwsContext;

class VideoCanvas : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit VideoCanvas(QWidget *parent = nullptr);
    ~VideoCanvas() override;

    void setOverlayMessage(const QString &message);
    void setPlaybackInfo(const QString &timestamp,
                         const QString &bitRate,
                         const QString &resolution,
                         const QString &codec);
    void clearPlaybackInfo();

public slots:
    void setFrame(const DecodedVideoFramePtr &frame);
    void setAnalysisOverlay(const FrameAnalysis &analysis);
    void setShowGrid(bool enabled);
    void setShowQpHeatmap(bool enabled);
    void setShowMotionVectors(bool enabled);
    void setShowPlaybackInfo(bool enabled);
    void setOverlayOpacity(float opacity);

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void ensureTexture();
    bool uploadCurrentFrame();
    void paintTexture();
    void drawAnalysisOverlay();
    void drawQpHeatmap(QPainter &painter, const QRectF &videoRect);
    void drawMacroblockGrid(QPainter &painter, const QRectF &videoRect);
    void drawMotionVectors(QPainter &painter, const QRectF &videoRect);
    void createOverlayWidgets();
    void updateOverlayWidgetGeometry();
    void updateOverlayWidgetVisibility();
    QRectF videoDisplayRect() const;
    QPointF mapVideoPointToWidget(const QPointF &videoPoint, const QRectF &videoRect) const;
    QRectF analysisRegionWidgetRect(const AnalysisRegion &region, const QRectF &videoRect) const;
    QColor qpHeatColor(int qp) const;

    QString m_overlayMessage;
    DecodedVideoFramePtr m_currentFrame;
    FrameAnalysis m_currentAnalysis;
    bool m_showGrid = true;
    bool m_showQpHeatmap = false;
    bool m_showMotionVectors = false;
    bool m_showPlaybackInfo = true;
    bool m_hasPlaybackInfo = false;
    float m_overlayOpacity = 1.0f;
    QLabel *m_overlayMessageLabel = nullptr;
    QFrame *m_playbackInfoPanel = nullptr;
    QLabel *m_timestampValueLabel = nullptr;
    QLabel *m_bitRateValueLabel = nullptr;
    QLabel *m_resolutionValueLabel = nullptr;
    QLabel *m_codecValueLabel = nullptr;
    SwsContext *m_swsContext = nullptr;
    QByteArray m_rgbaBuffer;
    QSize m_textureSize;
    GLuint m_textureId = 0;
    QOpenGLShaderProgram *m_program = nullptr;
};
