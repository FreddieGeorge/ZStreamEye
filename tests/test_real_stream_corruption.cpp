#include "core/decode/FFmpegDecoder.h"
#include "core/parser/video/h264/H264Parser.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QString>
#include <QVector>

#include <cstdlib>
#include <iostream>

namespace
{
struct DecodeResult
{
    bool opened = false;
    int decodedFrames = 0;
    bool sawDiagnostic = false;
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

void writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), QStringLiteral("Unable to create %1").arg(path));
    require(file.write(bytes) == bytes.size(), QStringLiteral("Unable to write %1").arg(path));
}

bool hasDiagnostic(const FrameAnalysis &analysis, const QString &code)
{
    for (const AnalysisDiagnostic &diagnostic : analysis.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

DecodeResult decodeWithLimit(const QString &path, int maximumFrames)
{
    FFmpegDecoder decoder;
    DecodeResult result;
    result.opened = decoder.openFile(path);
    if (!result.opened) {
        result.error = decoder.lastError();
        return result;
    }

    while (decoder.decodeNextFrame() != nullptr) {
        ++result.decodedFrames;
        result.sawDiagnostic = result.sawDiagnostic || !decoder.lastFrameAnalysis().diagnostics.isEmpty();
        require(result.decodedFrames <= maximumFrames,
                QStringLiteral("Damaged stream produced more frames than the safety bound"));
    }
    result.error = decoder.lastError();
    return result;
}

QVector<FrameAnalysis> decodeAnalyses(const QString &path, const QString &fileName)
{
    FFmpegDecoder decoder;
    require(decoder.openFile(path),
            QStringLiteral("Unable to open %1: %2").arg(fileName, decoder.lastError()));

    QVector<FrameAnalysis> analyses;
    while (decoder.decodeNextFrame() != nullptr) {
        analyses.append(decoder.lastFrameAnalysis());
    }
    require(decoder.lastError().isEmpty(),
            QStringLiteral("Reference decode failed for %1: %2").arg(fileName, decoder.lastError()));
    return analyses;
}

FrameAnalysis findNonKeyPacket(const QVector<FrameAnalysis> &analyses, const QString &fileName)
{
    for (const FrameAnalysis &analysis : analyses) {
        if (!analysis.packet.keyframe
            && analysis.packet.position >= 0
            && analysis.packet.bytes.size() > 8) {
            return analysis;
        }
    }
    fail(QStringLiteral("No suitable non-key video packet in %1").arg(fileName));
    return {};
}

void setFourByteLength(QByteArray *bytes, qsizetype offset, quint32 value)
{
    require(bytes != nullptr && offset >= 0 && offset + 4 <= bytes->size(),
            QStringLiteral("Invalid AVCC length mutation offset"));
    (*bytes)[offset] = static_cast<char>((value >> 24) & 0xff);
    (*bytes)[offset + 1] = static_cast<char>((value >> 16) & 0xff);
    (*bytes)[offset + 2] = static_cast<char>((value >> 8) & 0xff);
    (*bytes)[offset + 3] = static_cast<char>(value & 0xff);
}

void verifyPacketCorruptionDiagnostics(const FrameAnalysis &packetAnalysis, const QString &fileName)
{
    H264Parser parserWithoutConfiguration;
    const FrameSyntaxInfo missingParameterSetSyntax = parserWithoutConfiguration.parsePacketSyntax(
        packetAnalysis.packet.bytes,
        packetAnalysis.packet.pts,
        packetAnalysis.packet.dts,
        packetAnalysis.packet.streamPacketIndex);
    require(!missingParameterSetSyntax.slices.isEmpty(),
            QStringLiteral("Real packet has no slice for parameter-set tests: %1").arg(fileName));
    const FrameAnalysis missingParameterSets = parserWithoutConfiguration.parsePacket(
        packetAnalysis.packet.bytes,
        packetAnalysis.packet.pts,
        packetAnalysis.packet.dts,
        packetAnalysis.packet.streamPacketIndex);
    require(hasDiagnostic(missingParameterSets, QStringLiteral("pps_missing")),
            QStringLiteral("Real packet without decoder configuration did not report pps_missing for %1")
                .arg(fileName));

    PpsInfo pps;
    pps.valid = true;
    pps.picParameterSetId = missingParameterSetSyntax.slices.first().picParameterSetId;
    pps.seqParameterSetId = 31;
    QHash<int, PpsInfo> ppsById;
    ppsById.insert(pps.picParameterSetId, pps);
    H264Parser missingSpsParser;
    missingSpsParser.setParameterSets({}, ppsById);
    const FrameAnalysis missingSps = missingSpsParser.parsePacket(
        packetAnalysis.packet.bytes,
        packetAnalysis.packet.pts,
        packetAnalysis.packet.dts,
        packetAnalysis.packet.streamPacketIndex);
    require(hasDiagnostic(missingSps, QStringLiteral("sps_missing")),
            QStringLiteral("Real packet with an unavailable SPS did not report sps_missing for %1")
                .arg(fileName));

    QByteArray invalidLength = packetAnalysis.packet.bytes;
    setFourByteLength(&invalidLength, 0, static_cast<quint32>(invalidLength.size() + 4096));
    H264Parser invalidLengthParser;
    const FrameAnalysis invalidLengthAnalysis = invalidLengthParser.parsePacket(
        invalidLength,
        packetAnalysis.packet.pts,
        packetAnalysis.packet.dts,
        packetAnalysis.packet.streamPacketIndex);
    require(hasDiagnostic(invalidLengthAnalysis, QStringLiteral("avcc_nalu_length_exceeds_packet")),
            QStringLiteral("Mutated AVCC length did not report an overrun for %1").arg(fileName));

    H264Parser truncatedPacketParser;
    const FrameAnalysis truncatedPacket = truncatedPacketParser.parsePacket(
        packetAnalysis.packet.bytes.left(5),
        packetAnalysis.packet.pts,
        packetAnalysis.packet.dts,
        packetAnalysis.packet.streamPacketIndex);
    require(hasDiagnostic(truncatedPacket, QStringLiteral("avcc_nalu_length_exceeds_packet")),
            QStringLiteral("Truncated real packet did not report an AVCC overrun for %1").arg(fileName));
}

void verifyCorpusCase(const QString &corpusDirectory,
                      const QString &temporaryDirectory,
                      const QJsonObject &entry)
{
    const QString fileName = entry.value(QStringLiteral("file")).toString();
    const QString sourcePath = corpusDirectory + QLatin1Char('/') + fileName;
    const QByteArray sourceBytes = readFile(sourcePath);
    const int expectedFrames = entry.value(QStringLiteral("decoded_frames")).toInt();
    require(sourceBytes.size() > 64, QStringLiteral("Corpus file is too small: %1").arg(fileName));

    const QString headerPath = temporaryDirectory + QStringLiteral("/header-") + fileName;
    writeFile(headerPath, sourceBytes.left(16));
    const DecodeResult headerResult = decodeWithLimit(headerPath, expectedFrames + 2);
    require(!headerResult.opened && !headerResult.error.isEmpty(),
            QStringLiteral("Header-truncated stream did not fail cleanly for %1").arg(fileName));

    const QString tailPath = temporaryDirectory + QStringLiteral("/tail-") + fileName;
    writeFile(tailPath, sourceBytes.left((sourceBytes.size() * 3) / 4));
    const DecodeResult tailResult = decodeWithLimit(tailPath, expectedFrames + 2);
    require(tailResult.opened,
            QStringLiteral("Tail-truncated stream could not be opened for %1: %2")
                .arg(fileName, tailResult.error));
    require(tailResult.decodedFrames > 0 && tailResult.decodedFrames < expectedFrames,
            QStringLiteral("Tail truncation did not produce a bounded partial decode for %1: %2 frames")
                .arg(fileName)
                .arg(tailResult.decodedFrames));

    if (entry.value(QStringLiteral("container")).toString() == QStringLiteral("annexb")) {
        return;
    }

    const QVector<FrameAnalysis> analyses = decodeAnalyses(sourcePath, fileName);
    const FrameAnalysis packetAnalysis = findNonKeyPacket(analyses, fileName);
    verifyPacketCorruptionDiagnostics(packetAnalysis, fileName);

    const qsizetype packetPosition = sourceBytes.indexOf(packetAnalysis.packet.bytes);
    require(packetPosition >= 0,
            QStringLiteral("Packet evidence was not found in container bytes for %1").arg(fileName));
    require(sourceBytes.indexOf(packetAnalysis.packet.bytes,
                                packetPosition + packetAnalysis.packet.bytes.size()) < 0,
            QStringLiteral("Packet evidence is not unique in container bytes for %1").arg(fileName));

    QByteArray invalidContainerPacket = sourceBytes;
    setFourByteLength(&invalidContainerPacket,
                      packetPosition,
                      static_cast<quint32>(packetAnalysis.packet.bytes.size() + 4096));
    const QString invalidPacketPath = temporaryDirectory + QStringLiteral("/invalid-packet-") + fileName;
    writeFile(invalidPacketPath, invalidContainerPacket);
    const DecodeResult invalidPacketResult = decodeWithLimit(invalidPacketPath, expectedFrames + 2);
    require(invalidPacketResult.opened,
            QStringLiteral("Container with a damaged video packet could not be opened for %1: %2")
                .arg(fileName, invalidPacketResult.error));
    require(!invalidPacketResult.error.isEmpty()
                || invalidPacketResult.sawDiagnostic
                || invalidPacketResult.decodedFrames < expectedFrames,
            QStringLiteral("Damaged video packet had no observable bounded outcome for %1").arg(fileName));
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

    QTemporaryDir temporaryDirectory;
    require(temporaryDirectory.isValid(), QStringLiteral("Unable to create corruption test directory"));

    const QJsonArray cases = manifest.object().value(QStringLiteral("cases")).toArray();
    require(!cases.isEmpty(), QStringLiteral("Corpus manifest has no cases"));
    for (const QJsonValue &value : cases) {
        require(value.isObject(), QStringLiteral("Corpus case must be an object"));
        verifyCorpusCase(corpusDirectory, temporaryDirectory.path(), value.toObject());
    }

    std::cout << "Real stream corruption tests passed for " << cases.size() << " cases\n";
    return 0;
}
