#include "app/ExportController.h"

#include "core/export/AnalysisExportWriter.h"
#include "ui/VideoCanvas.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QTextStream>

#include <utility>

namespace
{
QString csvEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}
}

ExportController::ExportController(QWidget *dialogParent, QObject *parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
{
}

void ExportController::exportSelectedAccessUnitJson(const StreamInfo &streamInfo,
                                                    bool hasCurrentAnalysis,
                                                    const FrameAnalysis &analysis,
                                                    const QString &defaultDirectory)
{
    if (!hasCurrentAnalysis) {
        warn(tr("No selected access unit is available to export."));
        return;
    }

    const QString defaultPrefix = analysis.accessUnitKind == AccessUnitKind::AudioFrame
        ? QStringLiteral("audio-access-unit")
        : QStringLiteral("frame");
    const QString defaultName = QStringLiteral("%1-%2-syntax.json")
                                    .arg(defaultPrefix)
                                    .arg(analysis.frameIndex, 5, 10, QLatin1Char('0'));
    const QString filePath = QFileDialog::getSaveFileName(
        m_dialogParent,
        tr("Export Selected Access Unit JSON"),
        QDir(defaultDirectory).filePath(defaultName),
        tr("JSON Files (*.json);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error(tr("Failed to export JSON: %1").arg(file.errorString()));
        return;
    }

    const QJsonDocument document(selectedFrameExportToJson(streamInfo,
                                                           analysis,
                                                           QCoreApplication::applicationName(),
                                                           QCoreApplication::applicationVersion()));
    file.write(document.toJson(QJsonDocument::Indented));

    reportExported(QFileInfo(filePath).absolutePath(),
                   tr("[Info] Exported access-unit syntax JSON: %1").arg(QDir::toNativeSeparators(filePath)),
                   tr("Exported access-unit syntax JSON"));
}

void ExportController::exportAllAccessUnitSyntaxJson(const StreamInfo &streamInfo,
                                                     const QVector<FrameAnalysis> &analyses,
                                                     const QString &defaultDirectory)
{
    if (analyses.isEmpty()) {
        warn(tr("No decoded access-unit syntax is available to export."));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        m_dialogParent,
        tr("Export All Decoded Access Units JSON"),
        QDir(defaultDirectory).filePath(QStringLiteral("decoded-access-unit-syntax.json")),
        tr("JSON Files (*.json);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error(tr("Failed to export JSON: %1").arg(file.errorString()));
        return;
    }

    const QJsonDocument document(allFramesExportToJson(streamInfo,
                                                       analyses,
                                                       QCoreApplication::applicationName(),
                                                       QCoreApplication::applicationVersion()));
    file.write(document.toJson(QJsonDocument::Indented));

    reportExported(QFileInfo(filePath).absolutePath(),
                   tr("[Info] Exported all decoded access-unit syntax JSON: %1").arg(QDir::toNativeSeparators(filePath)),
                   tr("Exported decoded access-unit syntax JSON"));
}

void ExportController::exportAccessUnitListCsv(const QVector<FrameAnalysis> &analyses,
                                               const QString &defaultDirectory)
{
    if (analyses.isEmpty()) {
        warn(tr("No access-unit list is available to export."));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        m_dialogParent,
        tr("Export Access Unit List CSV"),
        QDir(defaultDirectory).filePath(QStringLiteral("access-unit-list.csv")),
        tr("CSV Files (*.csv);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        error(tr("Failed to export CSV: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << "index,stream_index,media_kind,access_unit_kind,type,pts,dts,poc,frame_num,"
           "stream_packet_index,container_packet_index,packet_pos,packet_size,packet_duration,keyframe,raw_bytes_size\n";
    for (const FrameAnalysis &analysis : std::as_const(analyses)) {
        if (analysis.frameIndex < 0) {
            continue;
        }
        out << csvEscape(QString::number(analysis.frameIndex)) << ','
            << csvEscape(QString::number(analysis.streamIndex)) << ','
            << csvEscape(mediaKindName(analysis.mediaKind)) << ','
            << csvEscape(accessUnitKindName(analysis.accessUnitKind)) << ','
            << csvEscape(analysis.frameType.isEmpty() ? QStringLiteral("-") : analysis.frameType) << ','
            << csvEscape(QString::number(analysis.pts)) << ','
            << csvEscape(QString::number(analysis.dts)) << ','
            << csvEscape(analysis.poc >= 0 ? QString::number(analysis.poc) : QStringLiteral("-")) << ','
            << csvEscape(analysis.frameNum >= 0 ? QString::number(analysis.frameNum) : QStringLiteral("-")) << ','
            << csvEscape(QString::number(analysis.packet.streamPacketIndex)) << ','
            << csvEscape(QString::number(analysis.packet.containerPacketIndex)) << ','
            << csvEscape(QString::number(analysis.packet.position)) << ','
            << csvEscape(QString::number(analysis.packet.size)) << ','
            << csvEscape(QString::number(analysis.packet.duration)) << ','
            << csvEscape(analysis.packet.keyframe ? QStringLiteral("true") : QStringLiteral("false")) << ','
            << csvEscape(QString::number(analysis.packet.bytes.size())) << '\n';
    }

    reportExported(QFileInfo(filePath).absolutePath(),
                   tr("[Info] Exported access-unit list CSV: %1").arg(QDir::toNativeSeparators(filePath)),
                   tr("Exported access-unit list CSV"));
}

void ExportController::exportScreenshot(VideoCanvas *videoCanvas, const QString &defaultDirectory)
{
    if (videoCanvas == nullptr) {
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        m_dialogParent,
        tr("Export Screenshot"),
        QDir(defaultDirectory).filePath(QStringLiteral("zstreameye-screenshot.png")),
        tr("PNG Images (*.png);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    const QImage image = videoCanvas->grab().toImage();
    if (image.isNull() || !image.save(filePath)) {
        error(tr("Failed to export screenshot: %1").arg(QDir::toNativeSeparators(filePath)));
        return;
    }

    reportExported(QFileInfo(filePath).absolutePath(),
                   tr("[Info] Exported screenshot: %1").arg(QDir::toNativeSeparators(filePath)),
                   tr("Exported screenshot"));
}

void ExportController::warn(const QString &message)
{
    emit statusMessage(message, 5000);
    emit logMessage(tr("[Warning] %1").arg(message));
}

void ExportController::error(const QString &message)
{
    emit statusMessage(message, 5000);
    emit logMessage(tr("[Error] %1").arg(message));
}

void ExportController::reportExported(const QString &directory,
                                      const QString &logMessageText,
                                      const QString &statusMessageText)
{
    emit exportDirectoryChanged(directory);
    emit logMessage(logMessageText);
    emit statusMessage(statusMessageText, 3000);
}
