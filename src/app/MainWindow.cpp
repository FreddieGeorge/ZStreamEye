#include "app/MainWindow.h"

#include "app/DecodeSession.h"
#include "app/ExportController.h"
#include "app/UpdateChecker.h"
#include "ui/BitstreamHexView.h"
#include "ui/FrameListView.h"
#include "ui/LogDock.h"
#include "ui/PropertyTreeView.h"
#include "ui/StatsDock.h"
#include "ui/VideoCanvas.h"

#include "core/analysis/AnalysisStats.h"
#include "core/analysis/BitstreamSearch.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMetaType>
#include <QMimeData>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QStandardPaths>
#include <QSettings>
#include <QSignalBlocker>
#include <QStringList>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QSlider>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace
{
QString firstWritableLocation(QStandardPaths::StandardLocation location)
{
    const QStringList locations = QStandardPaths::standardLocations(location);
    for (const QString &entry : locations) {
        if (!entry.isEmpty()) {
            return entry;
        }
    }
    return QDir::homePath();
}

int frameAnalysisQpValueCount(const FrameAnalysis &analysis, int *minQp = nullptr, int *maxQp = nullptr)
{
    int count = 0;
    int localMin = 0;
    int localMax = 0;
    for (const AnalysisRegion &region : analysis.regions) {
        if (region.kind != AnalysisRegionKind::Macroblock || region.qp < 0) {
            continue;
        }
        if (count == 0) {
            localMin = region.qp;
            localMax = region.qp;
        } else {
            localMin = std::min(localMin, region.qp);
            localMax = std::max(localMax, region.qp);
        }
        ++count;
    }

    if (minQp != nullptr) {
        *minQp = localMin;
    }
    if (maxQp != nullptr) {
        *maxQp = localMax;
    }
    return count;
}

bool frameAnalysisHasDiagnosticCode(const FrameAnalysis &analysis, const QString &code)
{
    for (const AnalysisDiagnostic &diagnostic : analysis.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

QString motionVectorStatusText(const FrameAnalysis &analysis)
{
    if (analysis.codecKind != CodecKind::H264) {
        return QObject::tr("MV unsupported for this codec.");
    }

    if (!analysis.motionVectors.isEmpty()) {
        return QObject::tr("Motion vectors: %1 parsed").arg(analysis.motionVectors.size());
    }

    if (analysis.frameType == QStringLiteral("I")) {
        return QObject::tr("MV: none expected for I-frame.");
    }

    if (frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("cabac_unsupported"))) {
        return QObject::tr("MV: frame uses CABAC; CABAC parsing not implemented.");
    }

    if (frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("b_direct_macroblock_unsupported"))) {
        return QObject::tr("MV: B_Direct parsing not implemented.");
    }

    if (frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("b8x8_sub_macroblock_unsupported"))) {
        return QObject::tr("MV: B_8x8 parsing not implemented.");
    }

    if (frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("b_slice_macroblock_unsupported"))) {
        return QObject::tr("MV: unsupported B-slice macroblock type.");
    }

    if (analysis.frameType == QStringLiteral("B")) {
        return QObject::tr("MV: no supported B-slice vectors parsed.");
    }

    if (frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("p8x8_sub_macroblock_unsupported"))
        || frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("p8x8_sub_macroblock_type_unsupported"))) {
        return QObject::tr("MV: unsupported P_8x8 sub-macroblock type.");
    }

    if (frameAnalysisHasDiagnosticCode(analysis, QStringLiteral("interlaced_or_fmo_unsupported"))) {
        return QObject::tr("MV: MBAFF/FMO parsing not implemented.");
    }

    return QObject::tr("MV: no supported vectors parsed for this frame.");
}

QString searchAccessUnitTypeText(const FrameAnalysis &analysis)
{
    if (analysis.accessUnitKind == AccessUnitKind::VideoFrame && !analysis.frameType.isEmpty()) {
        return analysis.frameType;
    }
    return accessUnitKindName(analysis.accessUnitKind);
}

QString streamSelectorText(const MediaStreamInfo &stream)
{
    const QString codec = stream.codecName.isEmpty()
        ? codecKindName(stream.codecKind)
        : stream.codecName;
    QString detail;
    if (stream.mediaKind == MediaKind::Video) {
        detail = QObject::tr("%1x%2").arg(stream.width).arg(stream.height);
    } else if (stream.mediaKind == MediaKind::Audio) {
        detail = QObject::tr("%1 Hz, %2 ch").arg(stream.sampleRate).arg(stream.channels);
    }

    return detail.isEmpty()
        ? QObject::tr("Stream #%1 %2 %3")
              .arg(stream.streamIndex)
              .arg(mediaKindName(stream.mediaKind))
              .arg(codec)
        : QObject::tr("Stream #%1 %2 %3 (%4)")
              .arg(stream.streamIndex)
              .arg(mediaKindName(stream.mediaKind))
              .arg(codec)
              .arg(detail);
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    qRegisterMetaType<StreamInfo>("StreamInfo");
    qRegisterMetaType<DecodedVideoFramePtr>("DecodedVideoFramePtr");
    qRegisterMetaType<FrameAnalysis>("FrameAnalysis");
    qRegisterMetaType<PacketRawData>("PacketRawData");
    qRegisterMetaType<FrameSeekCheckpoint>("FrameSeekCheckpoint");

    setWindowTitle(tr("ZStreamEye"));
    resize(1440, 900);
    setAcceptDrops(true);
    setDockOptions(QMainWindow::AllowTabbedDocks
                   | QMainWindow::AllowNestedDocks
                   | QMainWindow::AnimatedDocks);

    m_lastOpenDirectory = firstWritableLocation(QStandardPaths::DocumentsLocation);
    m_lastExportDirectory = m_lastOpenDirectory;
    m_decodeSession = new DecodeSession(this);
    m_exportController = new ExportController(this, this);
    m_updateChecker = new UpdateChecker(this, this);

    createActions();
    createDocks();
    m_statsUpdateTimer = new QTimer(this);
    m_statsUpdateTimer->setSingleShot(true);
    m_statsUpdateTimer->setInterval(100);
    connect(m_statsUpdateTimer, &QTimer::timeout,
            this, &MainWindow::updateStatsDock);
    connect(m_exportController, &ExportController::exportDirectoryChanged,
            this, [this](const QString &directory) {
                m_lastExportDirectory = directory;
            });
    connect(m_exportController, &ExportController::statusMessage,
            this, [this](const QString &message, int timeoutMs) {
                statusBar()->showMessage(message, timeoutMs);
            });
    connect(m_exportController, &ExportController::logMessage,
            m_logDock, &LogDock::appendLine);
    connect(m_updateChecker, &UpdateChecker::busyChanged,
            this, [this](bool busy) {
                if (m_checkForUpdatesAction != nullptr) {
                    m_checkForUpdatesAction->setEnabled(!busy);
                }
            });
    connect(m_updateChecker, &UpdateChecker::statusMessage,
            this, [this](const QString &message, int timeoutMs) {
                statusBar()->showMessage(message, timeoutMs);
            });
    connect(m_updateChecker, &UpdateChecker::logMessage,
            m_logDock, &LogDock::appendLine);
    connect(m_updateChecker, &UpdateChecker::applicationCloseRequested,
            this, &QWidget::close);
    connect(m_decodeSession, &DecodeSession::streamOpened, this, [this](const StreamInfo &streamInfo) {
        m_document.updateStreamInfo(streamInfo);
        populateStreamSelector(streamInfo);
        const QString streamSummary = tr("Codec: %1, %2x%3, %4 fps, %5")
                                          .arg(streamInfo.codecName)
                                          .arg(streamInfo.width)
                                          .arg(streamInfo.height)
                                          .arg(streamInfo.frameRate, 0, 'f', 3)
                                          .arg(streamInfo.pixelFormatName);
        if (streamInfo.codecKind != CodecKind::H264 && streamInfo.codecKind != CodecKind::HEVC) {
            const QString unsupportedMessage =
                tr("%1\n\nPlayback is available, but bitstream syntax analysis is not implemented for this codec yet.")
                    .arg(streamSummary);
            statusBar()->showMessage(
                tr("Playback only: %1 bitstream analysis is not supported yet.")
                    .arg(codecKindName(streamInfo.codecKind)),
                7000);
            m_logDock->appendLine(
                tr("[Info] %1 bitstream analysis is not supported yet; playback will continue without syntax overlays.")
                    .arg(codecKindName(streamInfo.codecKind)));
            m_propertyTreeView->showPlaceholder(unsupportedMessage);
            return;
        }

        statusBar()->showMessage(
            tr("Decoding %1 (%2x%3, %4 fps)")
                .arg(streamInfo.fileName)
                .arg(streamInfo.width)
                .arg(streamInfo.height)
                .arg(streamInfo.frameRate, 0, 'f', 3),
            5000);
        m_propertyTreeView->showPlaceholder(streamSummary);
    });
    connect(m_decodeSession, &DecodeSession::frameReady,
            this, &MainWindow::handleFrameReady);
    connect(m_decodeSession, &DecodeSession::accessUnitAnalysisDecoded,
            this, &MainWindow::handleAccessUnitAnalysis);
    connect(m_decodeSession, &DecodeSession::seekCheckpointReady,
            this, &MainWindow::handleSeekCheckpoint);
    connect(m_decodeSession, &DecodeSession::bufferingProgress,
            this, &MainWindow::handleBufferingProgress);
    connect(m_decodeSession, &DecodeSession::logMessage,
            m_logDock, &LogDock::appendLine);
    connect(m_decodeSession, &DecodeSession::errorOccurred, this, [this](const QString &message) {
        statusBar()->showMessage(message, 5000);
        m_logDock->appendLine(tr("[Error] %1").arg(message));
    });
    connect(m_decodeSession, &DecodeSession::stopped, this, [this]() {
        m_rebufferState.reset();
        m_playbackPaused = false;
        updatePlaybackActionState();
        setPlaybackControlsEnabled(hasOpenStream());
        m_logDock->appendLine(tr("[Info] Decode thread stopped."));
    });
    createMenus();
    createToolBars();
    loadSettings();
    updateExportActionState();

    statusBar()->showMessage(tr("Ready"));
    m_logDock->appendLine(tr("[Info] Application started."));
}

MainWindow::~MainWindow()
{
    saveSettings();
    stopDecoder();
}

void MainWindow::createActions()
{
    m_openAction = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("&Open"), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openStream);

    m_playPauseAction = new QAction(style()->standardIcon(QStyle::SP_MediaPause), tr("Pause"), this);
    m_playPauseAction->setShortcut(Qt::Key_Space);
    connect(m_playPauseAction, &QAction::triggered, this, &MainWindow::togglePlayback);

    m_previousFrameAction = new QAction(style()->standardIcon(QStyle::SP_MediaSkipBackward), tr("Previous Frame"), this);
    m_previousFrameAction->setShortcut(QKeySequence(Qt::Key_Left));
    connect(m_previousFrameAction, &QAction::triggered, this, &MainWindow::stepToPreviousFrame);

    m_nextFrameAction = new QAction(style()->standardIcon(QStyle::SP_MediaSkipForward), tr("Next Frame"), this);
    m_nextFrameAction->setShortcut(QKeySequence(Qt::Key_Right));
    connect(m_nextFrameAction, &QAction::triggered, this, &MainWindow::stepToNextFrame);

    m_stopAction = new QAction(style()->standardIcon(QStyle::SP_MediaStop), tr("Stop"), this);
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::stopPlayback);

    m_exportFrameSyntaxJsonAction = new QAction(tr("Export Selected Access Unit JSON"), this);
    connect(m_exportFrameSyntaxJsonAction, &QAction::triggered, this, &MainWindow::exportFrameSyntaxJson);

    m_exportAllFrameSyntaxJsonAction = new QAction(tr("Export All Decoded Access Units JSON"), this);
    connect(m_exportAllFrameSyntaxJsonAction, &QAction::triggered, this, &MainWindow::exportAllFrameSyntaxJson);

    m_exportFrameListCsvAction = new QAction(tr("Export Access Unit List CSV"), this);
    connect(m_exportFrameListCsvAction, &QAction::triggered, this, &MainWindow::exportFrameListCsv);

    m_exportScreenshotAction = new QAction(tr("Export Screenshot"), this);
    connect(m_exportScreenshotAction, &QAction::triggered, this, &MainWindow::exportScreenshot);

    m_checkForUpdatesAction = new QAction(tr("Check for Updates"), this);
    connect(m_checkForUpdatesAction, &QAction::triggered, this, &MainWindow::checkForUpdates);

    m_showGridAction = new QAction(tr("Show Macroblock Grid"), this);
    m_showGridAction->setCheckable(true);
    m_showGridAction->setChecked(true);

    m_showQpHeatmapAction = new QAction(tr("Show QP Heatmap"), this);
    m_showQpHeatmapAction->setCheckable(true);
    m_showQpHeatmapAction->setChecked(false);

    m_showMotionVectorsAction = new QAction(tr("Show Motion Vectors"), this);
    m_showMotionVectorsAction->setCheckable(true);
    m_showMotionVectorsAction->setChecked(false);

    setPlaybackControlsEnabled(false);
    updatePlaybackActionState();
    updateExportActionState();
}

void MainWindow::createDocks()
{
    m_videoCanvas = new VideoCanvas(this);
    setCentralWidget(m_videoCanvas);
    connect(m_showGridAction, &QAction::toggled,
            m_videoCanvas, &VideoCanvas::setShowGrid);
    connect(m_showQpHeatmapAction, &QAction::toggled,
            m_videoCanvas, &VideoCanvas::setShowQpHeatmap);
    connect(m_showMotionVectorsAction, &QAction::toggled,
            m_videoCanvas, &VideoCanvas::setShowMotionVectors);
    connect(m_showQpHeatmapAction, &QAction::toggled,
            this, &MainWindow::updateCurrentOverlayStatusHint);
    connect(m_showMotionVectorsAction, &QAction::toggled,
            this, &MainWindow::updateCurrentOverlayStatusHint);
    m_videoCanvas->setShowGrid(m_showGridAction->isChecked());
    m_videoCanvas->setShowQpHeatmap(m_showQpHeatmapAction->isChecked());
    m_videoCanvas->setShowMotionVectors(m_showMotionVectorsAction->isChecked());

    m_frameListView = new FrameListView;
    m_frameDock = new QDockWidget(tr("AccessUnitListView"), this);
    m_frameDock->setObjectName(QStringLiteral("FrameListDock"));
    m_frameDock->setWidget(m_frameListView);
    addDockWidget(Qt::LeftDockWidgetArea, m_frameDock);

    m_propertyTreeView = new PropertyTreeView;
    connect(m_frameListView, &FrameListView::frameSelected,
            this, &MainWindow::handleFrameListSelection);
    connect(m_frameListView, &FrameListView::accessUnitSelected,
            this, [this](const FrameAnalysis &analysis) {
                if (analysis.accessUnitKind == AccessUnitKind::VideoFrame) {
                    return;
                }
                m_analysisStore.setCurrentAnalysis(analysis);
                m_propertyTreeView->showFrameAnalysis(analysis);
                if (m_hexView != nullptr) {
                    m_hexView->showPacket(analysis);
                }
                updateExportActionState();
                statusBar()->showMessage(
                    tr("Selected %1 access unit %2 on stream %3")
                        .arg(mediaKindName(analysis.mediaKind))
                        .arg(analysis.frameIndex)
                        .arg(analysis.streamIndex),
                    2000);
            });
    m_propertyDock = new QDockWidget(tr("PropertyTreeView"), this);
    m_propertyDock->setObjectName(QStringLiteral("PropertyDock"));
    m_propertyDock->setWidget(m_propertyTreeView);
    addDockWidget(Qt::RightDockWidgetArea, m_propertyDock);

    m_statsDock = new StatsDock;
    m_statsDockWidget = new QDockWidget(tr("Analysis Stats"), this);
    m_statsDockWidget->setObjectName(QStringLiteral("StatsDockWidget"));
    m_statsDockWidget->setWidget(m_statsDock);
    addDockWidget(Qt::RightDockWidgetArea, m_statsDockWidget);
    tabifyDockWidget(m_propertyDock, m_statsDockWidget);
    m_propertyDock->raise();

    m_hexView = new BitstreamHexView;
    m_hexDock = new QDockWidget(tr("Bitstream Hex"), this);
    m_hexDock->setObjectName(QStringLiteral("BitstreamHexDock"));
    m_hexDock->setWidget(m_hexView);
    addDockWidget(Qt::BottomDockWidgetArea, m_hexDock);
    connect(m_propertyTreeView, &PropertyTreeView::bitFieldSelected,
            m_hexView, [this](const AnalysisBitField &field) {
                if (m_hexView != nullptr) {
                    m_hexView->highlightBitField(field);
                }
            });
    connect(m_hexView, &BitstreamHexView::bitFieldActivated,
            m_propertyTreeView, &PropertyTreeView::selectBitField);
    connect(m_hexView, &BitstreamHexView::videoSearchRequested,
            this, &MainWindow::searchVideoBitstream);

    m_logDock = new LogDock;
    m_logDockWidget = new QDockWidget(tr("LogDock"), this);
    m_logDockWidget->setObjectName(QStringLiteral("LogDockWidget"));
    m_logDockWidget->setWidget(m_logDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDockWidget);

    m_videoCanvas->setOverlayMessage(tr("Open a video or H.264 stream to begin."));
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exportFrameSyntaxJsonAction);
    fileMenu->addAction(m_exportAllFrameSyntaxJsonAction);
    fileMenu->addAction(m_exportFrameListCsvAction);
    fileMenu->addAction(m_exportScreenshotAction);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    m_docksMenu = viewMenu->addMenu(tr("&Docks"));
    m_docksMenu->addAction(m_frameDock->toggleViewAction());
    m_docksMenu->addAction(m_hexDock->toggleViewAction());
    m_docksMenu->addAction(m_propertyDock->toggleViewAction());
    m_docksMenu->addAction(m_statsDockWidget->toggleViewAction());
    m_docksMenu->addAction(m_logDockWidget->toggleViewAction());
    viewMenu->addSeparator();
    viewMenu->addAction(m_showGridAction);
    viewMenu->addAction(m_showQpHeatmapAction);
    viewMenu->addAction(m_showMotionVectorsAction);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_checkForUpdatesAction);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&About ZStreamEye"), this, &MainWindow::showAboutDialog);
}

void MainWindow::createToolBars()
{
    auto *fileToolBar = addToolBar(tr("File"));
    fileToolBar->setObjectName(QStringLiteral("FileToolBar"));
    fileToolBar->addAction(m_openAction);

    auto *playbackToolBar = addToolBar(tr("Playback"));
    playbackToolBar->setObjectName(QStringLiteral("PlaybackToolBar"));
    playbackToolBar->addAction(m_previousFrameAction);
    playbackToolBar->addAction(m_playPauseAction);
    playbackToolBar->addAction(m_nextFrameAction);
    playbackToolBar->addAction(m_stopAction);
    playbackToolBar->addSeparator();

    m_frameIndexLabel = new QLabel(tr("Frame - / -"), playbackToolBar);
    m_frameIndexLabel->setMinimumWidth(110);
    playbackToolBar->addWidget(m_frameIndexLabel);

    auto *overlayToolBar = addToolBar(tr("Overlays"));
    overlayToolBar->setObjectName(QStringLiteral("OverlayToolBar"));
    overlayToolBar->addAction(m_showGridAction);
    overlayToolBar->addAction(m_showQpHeatmapAction);
    overlayToolBar->addAction(m_showMotionVectorsAction);
    overlayToolBar->addSeparator();
    overlayToolBar->addWidget(new QLabel(tr("Opacity"), overlayToolBar));

    m_overlayOpacitySlider = new QSlider(Qt::Horizontal, overlayToolBar);
    m_overlayOpacitySlider->setRange(0, 100);
    m_overlayOpacitySlider->setValue(100);
    m_overlayOpacitySlider->setFixedWidth(120);
    overlayToolBar->addWidget(m_overlayOpacitySlider);
    connect(m_overlayOpacitySlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_videoCanvas != nullptr) {
            m_videoCanvas->setOverlayOpacity(value / 100.0f);
        }
    });

    auto *accessUnitToolBar = addToolBar(tr("Access Units"));
    accessUnitToolBar->setObjectName(QStringLiteral("AccessUnitToolBar"));
    accessUnitToolBar->addWidget(new QLabel(tr("Stream"), accessUnitToolBar));
    m_streamSelector = new QComboBox(accessUnitToolBar);
    m_streamSelector->setMinimumWidth(220);
    accessUnitToolBar->addWidget(m_streamSelector);
    accessUnitToolBar->addSeparator();
    accessUnitToolBar->addWidget(new QLabel(tr("Show"), accessUnitToolBar));
    m_accessUnitFilterSelector = new QComboBox(accessUnitToolBar);
    m_accessUnitFilterSelector->addItem(tr("All access units"), static_cast<int>(FrameListView::AccessUnitFilter::All));
    m_accessUnitFilterSelector->addItem(tr("Video only"), static_cast<int>(FrameListView::AccessUnitFilter::Video));
    m_accessUnitFilterSelector->addItem(tr("Audio only"), static_cast<int>(FrameListView::AccessUnitFilter::Audio));
    m_accessUnitFilterSelector->addItem(tr("Diagnostics only"), static_cast<int>(FrameListView::AccessUnitFilter::DiagnosticsOnly));
    accessUnitToolBar->addWidget(m_accessUnitFilterSelector);

    connect(m_streamSelector, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_frameListView == nullptr || m_streamSelector == nullptr || index < 0) {
            return;
        }
        m_frameListView->setStreamFilter(m_streamSelector->itemData(index).toInt());
    });
    connect(m_accessUnitFilterSelector, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_frameListView == nullptr || m_accessUnitFilterSelector == nullptr || index < 0) {
            return;
        }
        m_frameListView->setAccessUnitFilter(
            static_cast<FrameListView::AccessUnitFilter>(m_accessUnitFilterSelector->itemData(index).toInt()));
    });
    resetAccessUnitFilters();
}

void MainWindow::openStream()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open Video or Bitstream"),
        defaultOpenDirectory(),
        tr("Video and bitstream files (*.264 *.h264 *.avc *.h265 *.hevc *.mp4 *.m4v *.mov *.mkv *.avi *.ts *.m2ts *.mts *.webm *.flv *.wmv *.mpg *.mpeg *.3gp *.mp5 *.es *.bin);;"
           "H.264 bitstream (*.264 *.h264 *.avc *.es *.bin);;"
           "MP4 / MOV (*.mp4 *.m4v *.mov);;"
           "All Files (*)"));

    if (filePath.isEmpty()) {
        return;
    }

    openStreamFile(filePath);
}

void MainWindow::openStreamFile(const QString &filePath)
{
    QString errorMessage;
    if (!m_document.openFile(filePath, &errorMessage)) {
        statusBar()->showMessage(errorMessage, 5000);
        m_logDock->appendLine(tr("[Error] %1").arg(errorMessage));
        return;
    }

    const StreamInfo &stream = m_document.streamInfo();
    m_lastOpenDirectory = stream.directory;

    statusBar()->showMessage(tr("Opening %1").arg(stream.fileName), 5000);
    m_logDock->appendLine(tr("[Info] Opened %1").arg(QDir::toNativeSeparators(stream.absoluteFilePath)));
    m_logDock->appendLine(tr("[Info] File size: %1 bytes").arg(stream.sizeBytes));

    m_videoCanvas->setOverlayMessage(
        tr("%1\n\nDecoding video stream...")
            .arg(stream.fileName));
    m_videoCanvas->setAnalysisOverlay(FrameAnalysis {});
    if (m_hexView != nullptr) {
        m_hexView->clearPacket(tr("Select an access unit to inspect packet bytes."));
    }
    m_frameListView->clearFrames();
    resetAccessUnitFilters();
    m_propertyTreeView->showPlaceholder(tr("Property tree will appear here after parsing."));
    m_analysisStore.resetForNewStream();
    updateStatsDock();
    m_rebufferState.reset();
    m_preserveFrameListScroll = false;
    m_playbackPaused = false;
    updateFrameIndexDisplay();
    updateExportActionState();
    updatePlaybackActionState();

    startDecoder(stream.absoluteFilePath);
}

void MainWindow::startDecoder(const QString &filePath,
                              int startFrameIndex,
                              bool pauseAfterFirstFrame,
                              const FrameSeekCheckpoint &seekCheckpoint)
{
    if (m_decodeSession != nullptr) {
        m_decodeSession->start(filePath, startFrameIndex, pauseAfterFirstFrame, seekCheckpoint);
    }
    m_playbackPaused = pauseAfterFirstFrame;
    setPlaybackControlsEnabled(true);
    updatePlaybackActionState();
}

void MainWindow::stopDecoder()
{
    if (m_decodeSession != nullptr) {
        m_decodeSession->stop();
    }
}

void MainWindow::togglePlayback()
{
    if (m_decodeSession == nullptr || !m_decodeSession->isActive()) {
        if (hasOpenStream()) {
            replayFromBeginning();
        }
        return;
    }

    if (m_playbackPaused) {
        resumePlayback();
    } else {
        pausePlayback();
    }
}

void MainWindow::pausePlayback()
{
    if (m_decodeSession == nullptr || !m_decodeSession->isActive()) {
        return;
    }

    m_decodeSession->pause();
    m_playbackPaused = true;
    updatePlaybackActionState();
    showFrameFromCache(m_analysisStore.currentFrameIndex(), true, true);
    statusBar()->showMessage(tr("Paused"), 2000);
}

void MainWindow::resumePlayback()
{
    if (m_decodeSession == nullptr || !m_decodeSession->isActive()) {
        return;
    }

    m_decodeSession->play();
    m_playbackPaused = false;
    m_preserveFrameListScroll = false;
    updatePlaybackActionState();
    statusBar()->showMessage(tr("Playing"), 2000);
}

void MainWindow::stepToPreviousFrame()
{
    if (m_analysisStore.currentFrameIndex() <= 0) {
        return;
    }

    if (m_decodeSession != nullptr && m_decodeSession->isActive()) {
        pausePlayback();
    }
    const int targetFrame = m_analysisStore.currentFrameIndex() - 1;
    if (!showFrameFromCache(targetFrame, true, true)) {
        seekToFrame(targetFrame);
    }
}

void MainWindow::stepToNextFrame()
{
    if (m_decodeSession != nullptr && m_decodeSession->isActive()) {
        pausePlayback();
    }

    const int nextIndex = m_analysisStore.currentFrameIndex() + 1;
    if (nextIndex <= m_analysisStore.latestFrameIndex() && showFrameFromCache(nextIndex, true, true)) {
        return;
    }

    if (nextIndex <= m_analysisStore.latestFrameIndex()) {
        seekToFrame(nextIndex);
    } else if (m_decodeSession != nullptr && m_decodeSession->isActive()) {
        m_decodeSession->stepForward();
        m_playbackPaused = true;
        updatePlaybackActionState();
    }
}

void MainWindow::stopPlayback()
{
    stopDecoder();
    m_playbackPaused = false;
    updatePlaybackActionState();
    setPlaybackControlsEnabled(false);
    statusBar()->showMessage(tr("Stopped"), 2000);
}

void MainWindow::replayFromBeginning()
{
    if (!hasOpenStream()) {
        return;
    }

    m_frameListView->clearFrames();
    m_propertyTreeView->showPlaceholder(tr("Property tree will appear here after parsing."));
    m_videoCanvas->setAnalysisOverlay(FrameAnalysis {});
    m_analysisStore.resetDecodedData(false);
    updateStatsDock();
    m_rebufferState.reset();
    m_playbackPaused = false;
    updateFrameIndexDisplay();
    updateExportActionState();

    const StreamInfo &stream = m_document.streamInfo();
    statusBar()->showMessage(tr("Replaying %1").arg(stream.fileName), 3000);
    m_logDock->appendLine(tr("[Info] Replaying from the beginning."));
    startDecoder(stream.absoluteFilePath);
}

void MainWindow::exportFrameSyntaxJson()
{
    if (m_exportController != nullptr) {
        m_exportController->exportSelectedAccessUnitJson(m_document.streamInfo(),
                                                         m_analysisStore.hasCurrentAnalysis(),
                                                         m_analysisStore.currentAnalysis(),
                                                         defaultExportDirectory());
    }
}

void MainWindow::exportAllFrameSyntaxJson()
{
    if (m_exportController != nullptr) {
        m_exportController->exportAllAccessUnitSyntaxJson(m_document.streamInfo(),
                                                          m_analysisStore.accessUnitAnalyses(),
                                                          defaultExportDirectory());
    }
}

void MainWindow::exportFrameListCsv()
{
    if (m_exportController != nullptr) {
        m_exportController->exportAccessUnitListCsv(m_analysisStore.accessUnitAnalyses(),
                                                    defaultExportDirectory());
    }
}

void MainWindow::exportScreenshot()
{
    if (m_exportController != nullptr) {
        m_exportController->exportScreenshot(m_videoCanvas, defaultExportDirectory());
    }
}

void MainWindow::checkForUpdates()
{
    if (m_updateChecker != nullptr) {
        m_updateChecker->checkForUpdates();
    }
}

void MainWindow::showAboutDialog()
{
    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle(tr("About ZStreamEye"));
    aboutBox.setIconPixmap(windowIcon().pixmap(64, 64));
    aboutBox.setText(tr("ZStreamEye"));
    aboutBox.setInformativeText(
        tr("Version %1\n\n"
           "A desktop tool for inspecting video bitstreams, decoded frames, and codec syntax.\n\n"
           "GitHub: https://github.com/FreddieGeorge/ZStreamEye\n\n"
           "Copyright 2026 ZStreamEye contributors.\n"
           "Open source project. See the repository for license and release information.")
            .arg(QCoreApplication::applicationVersion()));
    QPushButton *githubButton = aboutBox.addButton(tr("Open GitHub"), QMessageBox::ActionRole);
    QPushButton *closeButton = aboutBox.addButton(QMessageBox::Close);
    aboutBox.setEscapeButton(closeButton);
    aboutBox.exec();
    if (aboutBox.clickedButton() == githubButton) {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/FreddieGeorge/ZStreamEye")));
    }
}

void MainWindow::handleFrameReady(int frameIndex,
                                  const DecodedVideoFramePtr &frame,
                                  const FrameAnalysis &analysis)
{
    m_analysisStore.addFrame(frameIndex, frame, analysis);
    showFrameFromCache(frameIndex, true, m_playbackPaused);
    if (m_decodeSession != nullptr && m_rebufferState.complete(m_decodeSession->generation(), frameIndex)) {
        statusBar()->showMessage(tr("Buffered frame %1").arg(frameIndex + 1), 2000);
    }
    updateExportActionState();
}

void MainWindow::handleAccessUnitAnalysis(const FrameAnalysis &analysis)
{
    if (!m_analysisStore.addAccessUnitAnalysis(analysis)) {
        return;
    }

    scheduleStatsDockUpdate();
    m_frameListView->addFrameAnalysis(analysis);
    if (analysis.accessUnitKind != AccessUnitKind::VideoFrame && !m_analysisStore.hasCurrentAnalysis()) {
        m_analysisStore.setCurrentAnalysis(analysis);
        m_propertyTreeView->showFrameAnalysis(analysis);
        if (m_hexView != nullptr) {
            m_hexView->showPacket(analysis);
        }
    }
    updateExportActionState();
}

void MainWindow::handleSeekCheckpoint(const FrameSeekCheckpoint &checkpoint)
{
    m_analysisStore.addSeekCheckpoint(checkpoint);
}

void MainWindow::handleBufferingProgress(int startFrameIndex, int currentFrameIndex, int targetFrameIndex)
{
    if (m_decodeSession == nullptr || !m_rebufferState.accepts(m_decodeSession->generation(), targetFrameIndex)) {
        return;
    }

    const RebufferState::Progress progress = RebufferState::progress(startFrameIndex,
                                                                     currentFrameIndex,
                                                                     targetFrameIndex);
    statusBar()->showMessage(
        tr("Buffering frame %1 from checkpoint %2: %3/%4 (%5%)")
            .arg(targetFrameIndex + 1)
            .arg(startFrameIndex + 1)
            .arg(progress.bufferedFrames)
            .arg(progress.totalFrames)
            .arg(progress.percent));
}

void MainWindow::handleFrameListSelection(int frameIndex)
{
    if (m_decodeSession != nullptr && m_decodeSession->isActive()) {
        m_decodeSession->pause();
        m_playbackPaused = true;
        m_preserveFrameListScroll = true;
        updatePlaybackActionState();
    }
    if (!showFrameFromCache(frameIndex, false, true)) {
        seekToFrame(frameIndex);
    }
}

void MainWindow::searchVideoBitstream(const QString &query)
{
    struct SearchResult
    {
        int analysisIndex = -1;
        BitstreamSearchMatch match;
    };

    constexpr qsizetype MaxVideoSearchResults = 5000;
    constexpr int SearchResultRole = Qt::UserRole + 1;
    const QVector<FrameAnalysis> &analyses = m_analysisStore.accessUnitAnalyses();
    if (analyses.isEmpty()) {
        QMessageBox::information(this,
                                 tr("Search Video Bitstream"),
                                 tr("No decoded access-unit bytes are available yet."));
        return;
    }

    QVector<SearchResult> results;
    bool truncated = false;
    for (int analysisIndex = 0; analysisIndex < analyses.size(); ++analysisIndex) {
        const qsizetype remaining = MaxVideoSearchResults - results.size();
        if (remaining <= 0) {
            truncated = true;
            break;
        }
        const QVector<BitstreamSearchMatch> matches =
            findBitstreamMatches(analyses.at(analysisIndex).packet.bytes, query, remaining);
        for (const BitstreamSearchMatch &match : matches) {
            results.append({analysisIndex, match});
        }
    }

    if (results.isEmpty()) {
        QMessageBox::information(
            this,
            tr("Search Video Bitstream"),
            tr("No Hex or text matches were found in %1 decoded access units.").arg(analyses.size()));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Video Bitstream Search"));
    dialog.resize(760, 480);

    auto *summary = new QLabel(
        truncated
            ? tr("Showing the first %1 matches from %2 decoded access units.")
                  .arg(results.size())
                  .arg(analyses.size())
            : tr("Found %1 matches in %2 decoded access units.")
                  .arg(results.size())
                  .arg(analyses.size()),
        &dialog);
    auto *resultTree = new QTreeWidget(&dialog);
    resultTree->setColumnCount(6);
    resultTree->setHeaderLabels({tr("AU Index"),
                                 tr("Stream"),
                                 tr("Media"),
                                 tr("Type"),
                                 tr("Byte Offset"),
                                 tr("Length")});
    resultTree->setRootIsDecorated(false);
    resultTree->setAlternatingRowColors(true);
    resultTree->setSelectionMode(QAbstractItemView::SingleSelection);
    resultTree->setUniformRowHeights(true);

    for (int resultIndex = 0; resultIndex < results.size(); ++resultIndex) {
        const SearchResult &result = results.at(resultIndex);
        const FrameAnalysis &analysis = analyses.at(result.analysisIndex);
        auto *item = new QTreeWidgetItem({QString::number(analysis.frameIndex),
                                          QString::number(analysis.streamIndex),
                                          mediaKindName(analysis.mediaKind),
                                          searchAccessUnitTypeText(analysis),
                                          QString::number(result.match.byteOffset),
                                          QString::number(result.match.byteLength)});
        item->setData(0, SearchResultRole, resultIndex);
        resultTree->addTopLevelItem(item);
    }
    resultTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    resultTree->header()->setStretchLastSection(true);
    resultTree->setCurrentItem(resultTree->topLevelItem(0));

    auto *buttons = new QDialogButtonBox(&dialog);
    QPushButton *openButton = buttons->addButton(tr("Open Match"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Close);
    connect(openButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(resultTree, &QTreeWidget::itemDoubleClicked, &dialog, [&dialog](QTreeWidgetItem *, int) {
        dialog.accept();
    });

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(summary);
    layout->addWidget(resultTree, 1);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted || resultTree->currentItem() == nullptr) {
        return;
    }
    const int resultIndex = resultTree->currentItem()->data(0, SearchResultRole).toInt();
    if (resultIndex < 0 || resultIndex >= results.size()) {
        return;
    }

    const SearchResult result = results.at(resultIndex);
    const FrameAnalysis analysis = analyses.at(result.analysisIndex);
    const AnalysisStore::CachedFrame *cached = analysis.accessUnitKind == AccessUnitKind::VideoFrame
        ? m_analysisStore.cachedFrame(analysis.frameIndex)
        : nullptr;
    if (cached != nullptr) {
        showFrameFromCache(analysis.frameIndex, true, true);
    } else {
        m_analysisStore.setCurrentAnalysis(analysis);
        m_propertyTreeView->showFrameAnalysis(analysis);
        if (analysis.accessUnitKind == AccessUnitKind::VideoFrame) {
            m_frameListView->selectFrameIndex(analysis.frameIndex);
        }
    }
    m_hexView->showPacket(analysis);
    m_hexView->showSearchMatch(query, result.match.byteOffset, result.match.byteLength);
    m_hexDock->show();
    m_hexDock->raise();
    updateExportActionState();
    statusBar()->showMessage(
        tr("Opened bitstream match at access unit %1, byte %2.")
            .arg(analysis.frameIndex)
            .arg(result.match.byteOffset),
        3000);
}

bool MainWindow::showFrameFromCache(int frameIndex, bool selectInList, bool updatePropertyTree)
{
    if (frameIndex < 0) {
        return false;
    }

    const AnalysisStore::CachedFrame *cached = m_analysisStore.cachedFrame(frameIndex);
    if (cached == nullptr) {
        return false;
    }

    m_analysisStore.setCurrentFromCachedFrame(*cached);
    m_videoCanvas->setFrame(cached->frame);
    m_videoCanvas->setAnalysisOverlay(cached->analysis);
    if (m_hexView != nullptr) {
        m_hexView->showPacket(cached->analysis);
    }
    if (updatePropertyTree) {
        m_propertyTreeView->showFrameAnalysis(cached->analysis);
        updateOverlayStatusHint(cached->analysis);
    }
    if (selectInList) {
        const bool scrollSelection = !m_playbackPaused && !m_preserveFrameListScroll;
        m_frameListView->selectFrameIndex(frameIndex, scrollSelection);
        if (!scrollSelection) {
            m_preserveFrameListScroll = false;
        }
    }
    updateFrameIndexDisplay();
    updateExportActionState();
    return true;
}

void MainWindow::seekToFrame(int frameIndex)
{
    if (frameIndex < 0 || !m_document.streamInfo().isValid) {
        return;
    }

    m_playbackPaused = true;
    m_preserveFrameListScroll = true;
    updatePlaybackActionState();
    setPlaybackControlsEnabled(false);
    m_videoCanvas->setOverlayMessage(QString());
    const int nextDecoderGeneration = m_decodeSession != nullptr ? m_decodeSession->generation() + 1 : 1;
    const RebufferState::StartResult rebufferStart = m_rebufferState.start(frameIndex, nextDecoderGeneration);
    if (rebufferStart.canceledPrevious) {
        m_logDock->appendLine(tr("[Info] Canceling pending rebuffer for frame %1.")
                                  .arg(rebufferStart.canceledTargetFrameIndex));
    }
    statusBar()->showMessage(tr("Buffering frame %1...").arg(frameIndex + 1));

    const FrameSeekCheckpoint checkpoint = m_analysisStore.bestSeekCheckpointForFrame(frameIndex);

    if (checkpoint.frameIndex >= 0) {
        m_logDock->appendLine(tr("[Info] Frame %1 is outside the recent cache; seeking from checkpoint frame %2.")
                                  .arg(frameIndex)
                                  .arg(checkpoint.frameIndex));
        startDecoder(m_document.streamInfo().absoluteFilePath, frameIndex, true, checkpoint);
    } else {
        m_logDock->appendLine(tr("[Info] Frame %1 is outside the recent cache; no checkpoint is available yet, decoding from the beginning.")
                                  .arg(frameIndex));
        startDecoder(m_document.streamInfo().absoluteFilePath, frameIndex, true);
    }
}

const AnalysisStore::CachedFrame *MainWindow::currentCachedFrame() const
{
    return m_analysisStore.currentCachedFrame();
}

void MainWindow::setPlaybackControlsEnabled(bool enabled)
{
    if (m_playPauseAction != nullptr) {
        m_playPauseAction->setEnabled(enabled);
    }
    setNavigationControlsEnabled(enabled);
    if (m_stopAction != nullptr) {
        m_stopAction->setEnabled(enabled && m_decodeSession != nullptr && m_decodeSession->isActive());
    }
}

void MainWindow::setNavigationControlsEnabled(bool enabled)
{
    if (m_previousFrameAction != nullptr) {
        m_previousFrameAction->setEnabled(enabled);
    }
    if (m_nextFrameAction != nullptr) {
        m_nextFrameAction->setEnabled(enabled);
    }
}

void MainWindow::updatePlaybackActionState()
{
    if (m_playPauseAction == nullptr) {
        return;
    }

    if (m_decodeSession == nullptr || !m_decodeSession->isActive()) {
        m_playPauseAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        m_playPauseAction->setText(hasOpenStream() ? tr("Replay") : tr("Play"));
        if (m_stopAction != nullptr) {
            m_stopAction->setEnabled(false);
        }
        return;
    }

    if (m_playbackPaused) {
        m_playPauseAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        m_playPauseAction->setText(tr("Play"));
    } else {
        m_playPauseAction->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        m_playPauseAction->setText(tr("Pause"));
    }
    if (m_stopAction != nullptr) {
        m_stopAction->setEnabled(true);
    }
}

void MainWindow::updateExportActionState()
{
    const bool hasCurrentFrame = currentCachedFrame() != nullptr;
    const bool hasDecodedSyntax = m_analysisStore.hasDecodedSyntax();

    if (m_exportFrameSyntaxJsonAction != nullptr) {
        m_exportFrameSyntaxJsonAction->setEnabled(m_analysisStore.hasCurrentAnalysis());
    }
    if (m_exportScreenshotAction != nullptr) {
        m_exportScreenshotAction->setEnabled(hasCurrentFrame);
    }
    if (m_exportFrameListCsvAction != nullptr) {
        m_exportFrameListCsvAction->setEnabled(hasDecodedSyntax);
    }
    if (m_exportAllFrameSyntaxJsonAction != nullptr) {
        m_exportAllFrameSyntaxJsonAction->setEnabled(hasDecodedSyntax);
    }
}

void MainWindow::updateFrameIndexDisplay()
{
    if (m_frameIndexLabel == nullptr) {
        return;
    }

    if (m_analysisStore.currentFrameIndex() < 0) {
        m_frameIndexLabel->setText(tr("Frame - / -"));
        return;
    }

    m_frameIndexLabel->setText(tr("Frame %1 / %2")
                                   .arg(m_analysisStore.currentFrameIndex() + 1)
                                   .arg(m_analysisStore.latestFrameIndex() + 1));
}

void MainWindow::scheduleStatsDockUpdate()
{
    if (m_statsUpdateTimer == nullptr) {
        updateStatsDock();
        return;
    }
    m_statsUpdateTimer->start();
}

void MainWindow::updateStatsDock()
{
    if (m_statsDock == nullptr) {
        return;
    }

    if (!m_analysisStore.hasDecodedSyntax()) {
        m_statsDock->showPlaceholder(tr("Open a stream to populate analysis statistics."));
        return;
    }

    m_statsDock->setStats(calculateAnalysisStats(m_analysisStore.accessUnitAnalyses()));
}

void MainWindow::updateCurrentOverlayStatusHint()
{
    const AnalysisStore::CachedFrame *cached = currentCachedFrame();
    if (cached == nullptr) {
        return;
    }

    updateOverlayStatusHint(cached->analysis);
}

void MainWindow::updateOverlayStatusHint(const FrameAnalysis &analysis)
{
    if (m_showMotionVectorsAction != nullptr && m_showMotionVectorsAction->isChecked()) {
        statusBar()->showMessage(motionVectorStatusText(analysis),
                                 analysis.motionVectors.isEmpty() ? 5000 : 3000);
        return;
    }

    if (m_showQpHeatmapAction != nullptr && m_showQpHeatmapAction->isChecked()) {
        int minQp = 0;
        int maxQp = 0;
        const int qpCount = frameAnalysisQpValueCount(analysis, &minQp, &maxQp);
        if (qpCount == 0) {
            statusBar()->showMessage(tr("QP heatmap has no parsed QP values for this frame."), 5000);
            return;
        }

        if (minQp == maxQp) {
            statusBar()->showMessage(
                tr("QP heatmap: %1 values, constant QP %2.")
                    .arg(qpCount)
                    .arg(minQp),
                4000);
            return;
        }

        statusBar()->showMessage(
            tr("QP heatmap: %1 values, range %2 - %3.")
                .arg(qpCount)
                .arg(minQp)
                .arg(maxQp),
            4000);
    }
}

void MainWindow::resetAccessUnitFilters()
{
    if (m_streamSelector != nullptr) {
        const QSignalBlocker blocker(m_streamSelector);
        m_streamSelector->clear();
        m_streamSelector->addItem(tr("All streams"), -1);
        m_streamSelector->setCurrentIndex(0);
        m_streamSelector->setEnabled(false);
    }
    if (m_accessUnitFilterSelector != nullptr) {
        const QSignalBlocker blocker(m_accessUnitFilterSelector);
        m_accessUnitFilterSelector->setCurrentIndex(0);
        m_accessUnitFilterSelector->setEnabled(false);
    }
    if (m_frameListView != nullptr) {
        m_frameListView->setStreamFilter(-1);
        m_frameListView->setAccessUnitFilter(FrameListView::AccessUnitFilter::All);
    }
}

void MainWindow::populateStreamSelector(const StreamInfo &streamInfo)
{
    if (m_streamSelector == nullptr || m_accessUnitFilterSelector == nullptr) {
        return;
    }

    const QVariant currentFilter = m_streamSelector->currentData();
    const int previousStreamFilter = currentFilter.isValid() ? currentFilter.toInt() : -1;

    const QSignalBlocker blocker(m_streamSelector);
    m_streamSelector->clear();
    m_streamSelector->addItem(tr("All streams"), -1);
    int selectedIndex = 0;
    for (const MediaStreamInfo &stream : streamInfo.streams) {
        m_streamSelector->addItem(streamSelectorText(stream), stream.streamIndex);
        if (previousStreamFilter >= 0 && stream.streamIndex == previousStreamFilter) {
            selectedIndex = m_streamSelector->count() - 1;
        }
    }
    m_streamSelector->setCurrentIndex(selectedIndex);
    m_streamSelector->setEnabled(!streamInfo.streams.isEmpty());
    m_accessUnitFilterSelector->setEnabled(!streamInfo.streams.isEmpty());

    if (m_frameListView != nullptr) {
        m_frameListView->setStreamFilter(m_streamSelector->itemData(selectedIndex).toInt());
    }
}

QString MainWindow::defaultOpenDirectory() const
{
    if (!m_lastOpenDirectory.isEmpty()) {
        return m_lastOpenDirectory;
    }
    return firstWritableLocation(QStandardPaths::DocumentsLocation);
}

QString MainWindow::defaultExportDirectory() const
{
    if (!m_lastExportDirectory.isEmpty()) {
        return m_lastExportDirectory;
    }
    return defaultOpenDirectory();
}

bool MainWindow::hasOpenStream() const
{
    const StreamInfo &stream = m_document.streamInfo();
    return stream.isValid && !stream.absoluteFilePath.isEmpty();
}

void MainWindow::loadSettings()
{
    QSettings settings;

    const QByteArray geometry = settings.value(QStringLiteral("ui/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    const QByteArray state = settings.value(QStringLiteral("ui/windowState")).toByteArray();
    if (!state.isEmpty()) {
        restoreState(state);
    }

    m_lastOpenDirectory = settings.value(QStringLiteral("paths/lastOpenDirectory"), m_lastOpenDirectory).toString();
    m_lastExportDirectory = settings.value(QStringLiteral("paths/lastExportDirectory"), m_lastExportDirectory).toString();

    if (m_showGridAction != nullptr) {
        m_showGridAction->setChecked(settings.value(QStringLiteral("overlays/showGrid"), m_showGridAction->isChecked()).toBool());
    }
    if (m_showQpHeatmapAction != nullptr) {
        m_showQpHeatmapAction->setChecked(settings.value(QStringLiteral("overlays/showQpHeatmap"), m_showQpHeatmapAction->isChecked()).toBool());
    }
    if (m_showMotionVectorsAction != nullptr) {
        m_showMotionVectorsAction->setChecked(settings.value(QStringLiteral("overlays/showMotionVectors"), m_showMotionVectorsAction->isChecked()).toBool());
    }
    if (m_overlayOpacitySlider != nullptr) {
        m_overlayOpacitySlider->setValue(settings.value(QStringLiteral("overlays/opacity"), m_overlayOpacitySlider->value()).toInt());
    }
}

void MainWindow::saveSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("ui/windowState"), saveState());
    settings.setValue(QStringLiteral("paths/lastOpenDirectory"), m_lastOpenDirectory);
    settings.setValue(QStringLiteral("paths/lastExportDirectory"), m_lastExportDirectory);
    if (m_showGridAction != nullptr) {
        settings.setValue(QStringLiteral("overlays/showGrid"), m_showGridAction->isChecked());
    }
    if (m_showQpHeatmapAction != nullptr) {
        settings.setValue(QStringLiteral("overlays/showQpHeatmap"), m_showQpHeatmapAction->isChecked());
    }
    if (m_showMotionVectorsAction != nullptr) {
        settings.setValue(QStringLiteral("overlays/showMotionVectors"), m_showMotionVectorsAction->isChecked());
    }
    if (m_overlayOpacitySlider != nullptr) {
        settings.setValue(QStringLiteral("overlays/opacity"), m_overlayOpacitySlider->value());
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }

    QMainWindow::dragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }

        openStreamFile(url.toLocalFile());
        event->acceptProposedAction();
        return;
    }

    QMainWindow::dropEvent(event);
}
