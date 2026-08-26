#include "core/decode/FFmpegDecoder.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <cstdlib>
#include <iostream>

namespace
{
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

int countStreams(const StreamInfo &streamInfo, MediaKind mediaKind)
{
    int count = 0;
    for (const MediaStreamInfo &stream : streamInfo.streams) {
        if (stream.mediaKind == mediaKind) {
            ++count;
        }
    }
    return count;
}

void verifyCorpusCase(const QString &corpusDirectory, const QJsonObject &entry)
{
    const QString fileName = entry.value(QStringLiteral("file")).toString();
    const QString path = corpusDirectory + QLatin1Char('/') + fileName;
    const QString expectedHash = entry.value(QStringLiteral("sha256")).toString();
    const QByteArray bytes = readFile(path);
    const QString actualHash = QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());

    require(!bytes.isEmpty(), QStringLiteral("Corpus file is empty: %1").arg(fileName));
    require(actualHash == expectedHash,
            QStringLiteral("SHA-256 mismatch for %1: expected %2, got %3")
                .arg(fileName, expectedHash, actualHash));

    FFmpegDecoder decoder;
    require(decoder.openFile(path),
            QStringLiteral("Unable to open %1: %2").arg(fileName, decoder.lastError()));

    const StreamInfo streamInfo = decoder.getStreamInfo();
    require(streamInfo.isValid, QStringLiteral("Invalid stream info for %1").arg(fileName));
    require(streamInfo.codecKind == CodecKind::H264,
            QStringLiteral("Selected codec is not H.264 for %1").arg(fileName));
    require(streamInfo.width == entry.value(QStringLiteral("width")).toInt(),
            QStringLiteral("Unexpected width for %1").arg(fileName));
    require(streamInfo.height == entry.value(QStringLiteral("height")).toInt(),
            QStringLiteral("Unexpected height for %1").arg(fileName));
    require(streamInfo.streams.size() == entry.value(QStringLiteral("stream_count")).toInt(),
            QStringLiteral("Unexpected stream count for %1").arg(fileName));
    require(countStreams(streamInfo, MediaKind::Audio)
                == entry.value(QStringLiteral("audio_stream_count")).toInt(),
            QStringLiteral("Unexpected audio stream count for %1").arg(fileName));

    int decodedFrames = 0;
    int analyzedFrames = 0;
    bool sawIFrame = false;
    bool sawPFrame = false;
    bool sawBFrame = false;
    bool sawPacketBytes = false;

    while (decoder.decodeNextFrame() != nullptr) {
        ++decodedFrames;
        const FrameAnalysis analysis = decoder.lastFrameAnalysis();
        if (analysis.pts != AV_NOPTS_VALUE && analysis.packet.pts != AV_NOPTS_VALUE) {
            require(analysis.pts == analysis.packet.pts,
                    QStringLiteral("Decoded frame PTS does not match analysis packet PTS for %1: frame %2, packet %3")
                        .arg(fileName)
                        .arg(analysis.pts)
                        .arg(analysis.packet.pts));
        }
        if (analysis.codecKind == CodecKind::H264 && analysis.hasFrame) {
            ++analyzedFrames;
        }
        sawIFrame = sawIFrame || analysis.frameType == QStringLiteral("I");
        sawPFrame = sawPFrame || analysis.frameType == QStringLiteral("P");
        sawBFrame = sawBFrame || analysis.frameType == QStringLiteral("B");
        sawPacketBytes = sawPacketBytes || !analysis.packet.bytes.isEmpty();
    }

    require(decoder.lastError().isEmpty(),
            QStringLiteral("Decode failed for %1: %2").arg(fileName, decoder.lastError()));

    const int expectedFrames = entry.value(QStringLiteral("decoded_frames")).toInt();
    require(decodedFrames == expectedFrames,
            QStringLiteral("Unexpected decoded frame count for %1: expected %2, got %3")
                .arg(fileName)
                .arg(expectedFrames)
                .arg(decodedFrames));
    const int minimumAnalyzedFrames = entry.value(QStringLiteral("minimum_analyzed_frames")).toInt();
    require(analyzedFrames >= minimumAnalyzedFrames,
            QStringLiteral("Too few H.264 frame analyses for %1: expected at least %2, got %3")
                .arg(fileName)
                .arg(minimumAnalyzedFrames)
                .arg(analyzedFrames));
    require(sawIFrame, QStringLiteral("No I-frame analysis observed for %1").arg(fileName));
    require(sawPFrame, QStringLiteral("No P-frame analysis observed for %1").arg(fileName));
    require(sawPacketBytes, QStringLiteral("No packet evidence retained for %1").arg(fileName));
    require(sawBFrame == entry.value(QStringLiteral("has_b_frames")).toBool(),
            QStringLiteral("Unexpected B-frame presence for %1").arg(fileName));

    const QVector<FrameAnalysis> nonVideoAnalyses = decoder.takePendingAccessUnitAnalyses();
    if (entry.value(QStringLiteral("audio_stream_count")).toInt() > 0) {
        bool sawAudioAnalysis = false;
        for (const FrameAnalysis &analysis : nonVideoAnalyses) {
            sawAudioAnalysis = sawAudioAnalysis || analysis.mediaKind == MediaKind::Audio;
        }
        require(sawAudioAnalysis,
                QStringLiteral("No audio packet analysis observed for %1").arg(fileName));
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
    require(manifest.object().value(QStringLiteral("schema_version")).toInt() == 1,
            QStringLiteral("Unsupported corpus manifest schema"));

    const QJsonArray cases = manifest.object().value(QStringLiteral("cases")).toArray();
    require(!cases.isEmpty(), QStringLiteral("Corpus manifest has no cases"));
    for (const QJsonValue &value : cases) {
        require(value.isObject(), QStringLiteral("Corpus case must be an object"));
        verifyCorpusCase(corpusDirectory, value.toObject());
    }

    std::cout << "Real stream corpus tests passed for " << cases.size() << " cases\n";
    return 0;
}
