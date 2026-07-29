#pragma once

#include "app/AnalysisStore.h"
#include "core/buffering/RebufferState.h"
#include "core/model/DecodedVideoFrame.h"
#include "core/model/FrameSeekCheckpoint.h"
#include "core/model/StreamDocument.h"

#include <QMainWindow>
#include <QString>

class QAction;
class QComboBox;
class QDockWidget;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QMenu;
class QSlider;
class QTimer;

class DecodeSession;
class BitstreamHexView;
class ExportController;
class FrameListView;
class LogDock;
class PropertyTreeView;
class StatsDock;
class UpdateChecker;
class VideoCanvas;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createDocks();
    void openStream();
    void openStreamFile(const QString &filePath);
    void startDecoder(const QString &filePath,
                      int startFrameIndex = 0,
                      bool pauseAfterFirstFrame = false,
                      const FrameSeekCheckpoint &seekCheckpoint = FrameSeekCheckpoint {});
    void stopDecoder();
    void togglePlayback();
    void pausePlayback();
    void resumePlayback();
    void stepToPreviousFrame();
    void stepToNextFrame();
    void stopPlayback();
    void replayFromBeginning();
    void exportFrameSyntaxJson();
    void exportAllFrameSyntaxJson();
    void exportFrameListCsv();
    void exportScreenshot();
    void checkForUpdates();
    void showAboutDialog();
    void handleFrameReady(int frameIndex, const DecodedVideoFramePtr &frame, const FrameAnalysis &analysis);
    void handleAccessUnitAnalysis(const FrameAnalysis &analysis);
    void handleSeekCheckpoint(const FrameSeekCheckpoint &checkpoint);
    void handleBufferingProgress(int startFrameIndex, int currentFrameIndex, int targetFrameIndex);
    void handleFrameListSelection(int frameIndex);
    void searchVideoBitstream(const QString &query);
    bool showFrameFromCache(int frameIndex, bool selectInList = true, bool updatePropertyTree = true);
    void seekToFrame(int frameIndex);
    const AnalysisStore::CachedFrame *currentCachedFrame() const;
    void setPlaybackControlsEnabled(bool enabled);
    void setNavigationControlsEnabled(bool enabled);
    void updatePlaybackActionState();
    void updateExportActionState();
    void updateFrameIndexDisplay();
    void scheduleStatsDockUpdate();
    void updateStatsDock();
    void updateCurrentOverlayStatusHint();
    void updateOverlayStatusHint(const FrameAnalysis &analysis);
    void resetAccessUnitFilters();
    void populateStreamSelector(const StreamInfo &streamInfo);
    bool hasOpenStream() const;
    QString defaultOpenDirectory() const;
    QString defaultExportDirectory() const;
    void loadSettings();
    void saveSettings() const;

    QAction *m_openAction = nullptr;
    QAction *m_playPauseAction = nullptr;
    QAction *m_previousFrameAction = nullptr;
    QAction *m_nextFrameAction = nullptr;
    QAction *m_stopAction = nullptr;
    QAction *m_exportFrameSyntaxJsonAction = nullptr;
    QAction *m_exportAllFrameSyntaxJsonAction = nullptr;
    QAction *m_exportFrameListCsvAction = nullptr;
    QAction *m_exportScreenshotAction = nullptr;
    QAction *m_checkForUpdatesAction = nullptr;
    QAction *m_showGridAction = nullptr;
    QAction *m_showQpHeatmapAction = nullptr;
    QAction *m_showMotionVectorsAction = nullptr;
    QSlider *m_overlayOpacitySlider = nullptr;
    QLabel *m_frameIndexLabel = nullptr;
    QTimer *m_statsUpdateTimer = nullptr;
    QComboBox *m_streamSelector = nullptr;
    QComboBox *m_accessUnitFilterSelector = nullptr;
    QMenu *m_docksMenu = nullptr;

    QDockWidget *m_frameDock = nullptr;
    QDockWidget *m_hexDock = nullptr;
    QDockWidget *m_propertyDock = nullptr;
    QDockWidget *m_statsDockWidget = nullptr;
    QDockWidget *m_logDockWidget = nullptr;

    FrameListView *m_frameListView = nullptr;
    BitstreamHexView *m_hexView = nullptr;
    PropertyTreeView *m_propertyTreeView = nullptr;
    StatsDock *m_statsDock = nullptr;
    LogDock *m_logDock = nullptr;
    VideoCanvas *m_videoCanvas = nullptr;

    StreamDocument m_document;
    AnalysisStore m_analysisStore;
    DecodeSession *m_decodeSession = nullptr;
    ExportController *m_exportController = nullptr;
    UpdateChecker *m_updateChecker = nullptr;
    QString m_lastOpenDirectory;
    QString m_lastExportDirectory;
    RebufferState m_rebufferState;
    bool m_playbackPaused = false;
    bool m_preserveFrameListScroll = false;
};
