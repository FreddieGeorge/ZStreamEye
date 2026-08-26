#include "core/decode/DecodeEventSink.h"
#include "core/decode/DecodeLoop.h"
#include "core/decode/FFmpegDecoder.h"
#include "core/decode/SeekCheckpointEmitter.h"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <cstdlib>
#include <iostream>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

namespace
{
struct FrameSnapshot
{
    qint64 pts = AV_NOPTS_VALUE;
    qint64 packetPts = AV_NOPTS_VALUE;
    int streamPacketIndex = -1;
    QString frameType;
    QByteArray packetBytes;
    QByteArray pixelHash;
};

struct RebufferResult
{
    bool receivedFrame = false;
    int frameIndex = -1;
    FrameSnapshot frame;
    QVector<int> progressFrames;
    QVector<QString> logMessages;
    QString error;
};

void fail(const QString &message)
{
    std::cerr << "FAIL: " << message.toStdString() << '\n';
    std::exit(1);
}

void require(bool condition, const QString &message)
{
    if (!condition) {
        fail(message);
    }
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), QStringLiteral("Unable to open %1").arg(path));
    return file.readAll();
}

QByteArray framePixelHash(const DecodedVideoFramePtr &frame)
{
    require(frame != nullptr, QStringLiteral("Decoded frame copy is missing"));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const AVPixFmtDescriptor *descriptor = av_pix_fmt_desc_get(frame->pixelFormat);
    for (int planeIndex = 0; planeIndex < static_cast<int>(frame->planes.size()); ++planeIndex) {
        const QByteArray &plane = frame->planes[planeIndex];
        const int lineSize = frame->lineSize[planeIndex];
        if (plane.isEmpty() || lineSize <= 0) {
            continue;
        }

        int planeHeight = frame->height;
        if (descriptor != nullptr && planeIndex > 0) {
            planeHeight = AV_CEIL_RSHIFT(frame->height, descriptor->log2_chroma_h);
        }
        const int visibleLineSize = av_image_get_linesize(frame->pixelFormat,
                                                          frame->width,
                                                          planeIndex);
        require(visibleLineSize > 0 && visibleLineSize <= lineSize,
                QStringLiteral("Invalid visible plane line size"));
        require(static_cast<qsizetype>(lineSize) * planeHeight <= plane.size(),
                QStringLiteral("Decoded plane data is truncated"));
        for (int row = 0; row < planeHeight; ++row) {
            hash.addData(QByteArrayView(plane.constData() + row * lineSize, visibleLineSize));
        }
    }
    return hash.result();
}

FrameSnapshot snapshotFrame(const DecodedVideoFramePtr &frame, const FrameAnalysis &analysis)
{
    FrameSnapshot snapshot;
    snapshot.pts = frame != nullptr ? frame->pts : analysis.pts;
    snapshot.packetPts = analysis.packet.pts;
    snapshot.streamPacketIndex = analysis.packet.streamPacketIndex;
    snapshot.frameType = analysis.frameType;
    snapshot.packetBytes = analysis.packet.bytes;
    snapshot.pixelHash = framePixelHash(frame);
    return snapshot;
}

void requireMatchingFrame(const FrameSnapshot &actual,
                          const FrameSnapshot &expected,
                          const QString &fileName,
                          int targetFrameIndex)
{
    const QString context = QStringLiteral("%1 target frame %2").arg(fileName).arg(targetFrameIndex);
    require(actual.pts == expected.pts,
            QStringLiteral("Decoded PTS mismatch for %1").arg(context));
    require(actual.packetPts == expected.packetPts,
            QStringLiteral("Packet PTS mismatch for %1").arg(context));
    require(actual.streamPacketIndex == expected.streamPacketIndex,
            QStringLiteral("Stream packet index mismatch for %1").arg(context));
    require(actual.frameType == expected.frameType,
            QStringLiteral("Frame type mismatch for %1").arg(context));
    require(actual.packetBytes == expected.packetBytes,
            QStringLiteral("Packet bytes mismatch for %1").arg(context));
    require(actual.pixelHash == expected.pixelHash,
            QStringLiteral("Decoded pixel hash mismatch for %1").arg(context));
}

QVector<FrameSnapshot> decodeBaseline(const QString &path,
                                      FrameSeekCheckpoint *checkpoint,
                                      const QString &fileName)
{
    FFmpegDecoder decoder;
    require(decoder.openFile(path),
            QStringLiteral("Unable to open %1: %2").arg(fileName, decoder.lastError()));

    QVector<FrameSnapshot> frames;
    while (AVFrame *decoded = decoder.decodeNextFrame()) {
        const DecodedVideoFramePtr frame = FFmpegDecoder::copyFrame(decoded);
        const FrameAnalysis analysis = decoder.lastFrameAnalysis();
        const int frameIndex = frames.size();
        frames.append(snapshotFrame(frame, analysis));

        const std::optional<FrameSeekCheckpoint> emitted =
            seekCheckpointForDecodedFrame(decoder.lastFrameSeekCheckpoint(), frameIndex);
        if (emitted.has_value() && checkpoint->frameIndex < 0) {
            *checkpoint = *emitted;
        }
    }

    require(decoder.lastError().isEmpty(),
            QStringLiteral("Baseline decode failed for %1: %2").arg(fileName, decoder.lastError()));
    require(checkpoint->frameIndex >= 0,
            QStringLiteral("No seek checkpoint produced for %1").arg(fileName));
    require(checkpoint->keyframe || checkpoint->idr || checkpoint->frameIndex == 0,
            QStringLiteral("Checkpoint is not a sync point for %1").arg(fileName));
    return frames;
}

RebufferResult rebufferFrame(const QString &path,
                             int targetFrameIndex,
                             const FrameSeekCheckpoint &checkpoint)
{
    DecodeLoop loop;
    RebufferResult result;
    DecodeEventSink sink;
    sink.bufferingProgress = [&](int startFrameIndex, int currentFrameIndex, int targetIndex) {
        require(startFrameIndex == checkpoint.frameIndex,
                QStringLiteral("Unexpected rebuffer start frame"));
        require(targetIndex == targetFrameIndex,
                QStringLiteral("Unexpected rebuffer target frame"));
        result.progressFrames.append(currentFrameIndex);
    };
    sink.frameReady = [&](int frameIndex,
                          const DecodedVideoFramePtr &frame,
                          const FrameAnalysis &analysis) {
        require(!result.receivedFrame, QStringLiteral("Rebuffer emitted more than one visible frame"));
        result.receivedFrame = true;
        result.frameIndex = frameIndex;
        result.frame = snapshotFrame(frame, analysis);
        loop.stop();
    };
    sink.logMessage = [&](const QString &message) {
        result.logMessages.append(message);
    };
    sink.errorOccurred = [&](const QString &message) {
        result.error = message;
    };

    DecodeLoop::Request request;
    request.filePath = path;
    request.targetFrameIndex = targetFrameIndex;
    request.checkpoint = checkpoint;
    loop.run(request, sink);
    return result;
}

void verifySeekCase(const QString &corpusDirectory, const QJsonObject &entry)
{
    const QJsonArray seekTargets = entry.value(QStringLiteral("seek_targets")).toArray();
    if (seekTargets.isEmpty()) {
        return;
    }

    const QString fileName = entry.value(QStringLiteral("file")).toString();
    const QString path = corpusDirectory + QLatin1Char('/') + fileName;
    FrameSeekCheckpoint checkpoint;
    const QVector<FrameSnapshot> baseline = decodeBaseline(path, &checkpoint, fileName);
    require(baseline.size() == entry.value(QStringLiteral("decoded_frames")).toInt(),
            QStringLiteral("Unexpected baseline frame count for %1").arg(fileName));

    for (const QJsonValue &value : seekTargets) {
        const int targetFrameIndex = value.toInt(-1);
        require(targetFrameIndex > checkpoint.frameIndex && targetFrameIndex < baseline.size(),
                QStringLiteral("Invalid seek target %1 for %2").arg(targetFrameIndex).arg(fileName));

        const RebufferResult result = rebufferFrame(path, targetFrameIndex, checkpoint);
        require(result.error.isEmpty(),
                QStringLiteral("Rebuffer failed for %1 target %2: %3")
                    .arg(fileName)
                    .arg(targetFrameIndex)
                    .arg(result.error));
        require(result.receivedFrame,
                QStringLiteral("No rebuffered frame for %1 target %2")
                    .arg(fileName)
                    .arg(targetFrameIndex));
        require(result.frameIndex == targetFrameIndex,
                QStringLiteral("Wrong rebuffered frame index for %1: expected %2, got %3")
                    .arg(fileName)
                    .arg(targetFrameIndex)
                    .arg(result.frameIndex));
        require(result.frame.pts == result.frame.packetPts,
                QStringLiteral("Rebuffered frame and packet PTS differ for %1 target %2")
                    .arg(fileName)
                    .arg(targetFrameIndex));
        require(!result.progressFrames.isEmpty(),
                QStringLiteral("No rebuffer progress for %1 target %2")
                    .arg(fileName)
                    .arg(targetFrameIndex));
        require(result.progressFrames.first() == checkpoint.frameIndex,
                QStringLiteral("Rebuffer progress did not start at checkpoint for %1").arg(fileName));
        require(result.progressFrames.last() == targetFrameIndex - 1,
                QStringLiteral("Rebuffer progress did not reach the frame before target for %1")
                    .arg(fileName));

        bool sawSeekLog = false;
        for (const QString &message : result.logMessages) {
            sawSeekLog = sawSeekLog || message.contains(QStringLiteral("Seeking from checkpoint"));
        }
        require(sawSeekLog,
                QStringLiteral("No successful checkpoint seek log for %1 target %2")
                    .arg(fileName)
                    .arg(targetFrameIndex));
        requireMatchingFrame(result.frame, baseline.at(targetFrameIndex), fileName, targetFrameIndex);
    }
}
}

int main()
{
    const QString corpusDirectory = QStringLiteral(ZSTREAMEYE_TEST_CORPUS_DIR);
    const QByteArray manifestBytes = readFile(corpusDirectory + QStringLiteral("/manifest.json"));
    QJsonParseError parseError;
    const QJsonDocument manifest = QJsonDocument::fromJson(manifestBytes, &parseError);

    require(parseError.error == QJsonParseError::NoError,
            QStringLiteral("Invalid corpus manifest: %1").arg(parseError.errorString()));
    require(manifest.isObject(), QStringLiteral("Corpus manifest root must be an object"));

    int seekCaseCount = 0;
    const QJsonArray cases = manifest.object().value(QStringLiteral("cases")).toArray();
    for (const QJsonValue &value : cases) {
        require(value.isObject(), QStringLiteral("Corpus case must be an object"));
        const QJsonObject entry = value.toObject();
        if (!entry.value(QStringLiteral("seek_targets")).toArray().isEmpty()) {
            ++seekCaseCount;
        }
        verifySeekCase(corpusDirectory, entry);
    }

    require(seekCaseCount > 0, QStringLiteral("Corpus manifest has no seek cases"));
    std::cout << "Real stream seek tests passed for " << seekCaseCount << " cases\n";
    return 0;
}
