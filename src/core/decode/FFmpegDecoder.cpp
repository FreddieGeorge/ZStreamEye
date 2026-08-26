#include "core/decode/FFmpegDecoder.h"

#include "core/analysis/AccessUnitAnalyzer.h"
#include "core/decode/FFmpegStreamInfoBuilder.h"
#include "core/parser/ParserFactory.h"

#include <QFile>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

FFmpegDecoder::FFmpegDecoder()
    : m_accessUnitAnalyzer(std::make_unique<AccessUnitAnalyzer>())
{
    m_packet = av_packet_alloc();
    m_frame = av_frame_alloc();
    if (m_packet == nullptr || m_frame == nullptr) {
        setError(QStringLiteral("Unable to allocate FFmpeg packet/frame."));
    }
}

FFmpegDecoder::~FFmpegDecoder()
{
    close();
    av_packet_free(&m_packet);
    av_frame_free(&m_frame);
}

bool FFmpegDecoder::openFile(const QString &filePath)
{
    close();

    const QByteArray encodedPath = QFile::encodeName(filePath);
    int ret = avformat_open_input(&m_formatContext, encodedPath.constData(), nullptr, nullptr);
    if (ret < 0) {
        setError(QStringLiteral("avformat_open_input failed: %1").arg(ffmpegErrorString(ret)));
        return false;
    }

    ret = avformat_find_stream_info(m_formatContext, nullptr);
    if (ret < 0) {
        setError(QStringLiteral("avformat_find_stream_info failed: %1").arg(ffmpegErrorString(ret)));
        close();
        return false;
    }

    populateContainerStreamInfo(m_formatContext, filePath, &m_streamInfo);

    for (unsigned int i = 0; i < m_formatContext->nb_streams; ++i) {
        AVStream *stream = m_formatContext->streams[i];
        const AVCodecParameters *parameters = stream != nullptr ? stream->codecpar : nullptr;
        if (parameters == nullptr) {
            continue;
        }

        if (mediaKindFromAvMediaType(parameters->codec_type) == MediaKind::Audio) {
            std::unique_ptr<IBitstreamParser> parser = makeBitstreamParserForAvCodecId(parameters->codec_id);
            if (parser != nullptr) {
                const QByteArray extraData = parameters->extradata != nullptr && parameters->extradata_size > 0
                    ? QByteArray(reinterpret_cast<const char *>(parameters->extradata), parameters->extradata_size)
                    : QByteArray {};
                m_accessUnitAnalyzer->addPacketParser(static_cast<int>(i),
                                                      MediaKind::Audio,
                                                      std::move(parser),
                                                      extraData);
            }
        }
    }

    const AVCodec *decoder = nullptr;
    ret = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (ret < 0) {
        setError(QStringLiteral("No video stream found: %1").arg(ffmpegErrorString(ret)));
        close();
        return false;
    }

    m_videoStreamIndex = ret;
    m_containerPacketIndex = 0;
    AVStream *videoStream = m_formatContext->streams[m_videoStreamIndex];

    m_codecContext = avcodec_alloc_context3(decoder);
    if (m_codecContext == nullptr) {
        setError(QStringLiteral("Unable to allocate AVCodecContext."));
        close();
        return false;
    }

    ret = avcodec_parameters_to_context(m_codecContext, videoStream->codecpar);
    if (ret < 0) {
        setError(QStringLiteral("avcodec_parameters_to_context failed: %1").arg(ffmpegErrorString(ret)));
        close();
        return false;
    }

    ret = avcodec_open2(m_codecContext, decoder, nullptr);
    if (ret < 0) {
        setError(QStringLiteral("avcodec_open2 failed: %1").arg(ffmpegErrorString(ret)));
        close();
        return false;
    }

    populateSelectedVideoStreamInfo(m_formatContext,
                                    m_codecContext,
                                    m_videoStreamIndex,
                                    decoder,
                                    &m_streamInfo);
    if (m_streamInfo.width <= 0 || m_streamInfo.height <= 0) {
        setError(QStringLiteral("Video stream has invalid dimensions: %1x%2.")
                     .arg(m_streamInfo.width)
                     .arg(m_streamInfo.height));
        close();
        return false;
    }

    const QByteArray extraData = m_codecContext->extradata != nullptr && m_codecContext->extradata_size > 0
        ? QByteArray(reinterpret_cast<const char *>(m_codecContext->extradata),
                     m_codecContext->extradata_size)
        : QByteArray {};
    m_accessUnitAnalyzer->setVideoParser(m_videoStreamIndex,
                                         makeBitstreamParserForAvCodecId(m_codecContext->codec_id),
                                         extraData);
    m_pendingFrames.clear();
    m_lastFrameAnalysis = FrameAnalysis {};
    m_lastFrameSeekCheckpoint = FrameSeekCheckpoint {};
    m_draining = false;
    m_lastError.clear();

    return true;
}

bool FFmpegDecoder::seekToCheckpoint(const FrameSeekCheckpoint &checkpoint)
{
    if (m_formatContext == nullptr || m_codecContext == nullptr) {
        setError(QStringLiteral("Decoder is not open."));
        return false;
    }

    int ret = AVERROR(EINVAL);
    if (checkpoint.packetPts != AV_NOPTS_VALUE) {
        ret = av_seek_frame(m_formatContext,
                            m_videoStreamIndex,
                            checkpoint.packetPts,
                            AVSEEK_FLAG_BACKWARD);
    } else if (checkpoint.packetDts != AV_NOPTS_VALUE) {
        ret = av_seek_frame(m_formatContext,
                            m_videoStreamIndex,
                            checkpoint.packetDts,
                            AVSEEK_FLAG_BACKWARD);
    } else if (checkpoint.packetPosition >= 0) {
        ret = av_seek_frame(m_formatContext,
                            -1,
                            checkpoint.packetPosition,
                            AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_BYTE);
    }

    if (ret < 0) {
        setError(QStringLiteral("av_seek_frame failed for checkpoint at frame %1: %2")
                     .arg(checkpoint.frameIndex)
                     .arg(ffmpegErrorString(ret)));
        return false;
    }

    avcodec_flush_buffers(m_codecContext);
    av_packet_unref(m_packet);
    av_frame_unref(m_frame);
    m_pendingFrames.clear();
    m_lastFrameAnalysis = FrameAnalysis {};
    m_lastFrameSeekCheckpoint = FrameSeekCheckpoint {};
    m_accessUnitAnalyzer->restoreVideoParserState(checkpoint);
    m_accessUnitAnalyzer->setVideoPacketIndex(checkpoint.packetIndex);
    m_containerPacketIndex = std::max(0, checkpoint.containerPacketIndex);
    m_draining = false;
    m_lastError.clear();
    return true;
}

AVFrame *FFmpegDecoder::decodeNextFrame()
{
    if (m_formatContext == nullptr || m_codecContext == nullptr) {
        setError(QStringLiteral("Decoder is not open."));
        return nullptr;
    }

    while (true) {
        int ret = avcodec_receive_frame(m_codecContext, m_frame);
        if (ret == 0) {
            if (!m_pendingFrames.isEmpty()) {
                int pendingIndex = 0;
                if (m_frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                    for (int i = 0; i < m_pendingFrames.size(); ++i) {
                        if (m_pendingFrames.at(i).analysis.pts == m_frame->best_effort_timestamp) {
                            pendingIndex = i;
                            break;
                        }
                    }
                }
                const PendingFrameInfo info = m_pendingFrames.takeAt(pendingIndex);
                m_lastFrameAnalysis = info.analysis;
                m_lastFrameSeekCheckpoint = info.checkpoint;
            } else {
                m_lastFrameAnalysis = FrameAnalysis {};
                m_lastFrameSeekCheckpoint = FrameSeekCheckpoint {};
            }
            m_lastFrameAnalysis.pts = m_frame->best_effort_timestamp;
            return m_frame;
        }

        if (ret == AVERROR_EOF) {
            return nullptr;
        }

        if (ret != AVERROR(EAGAIN)) {
            setError(QStringLiteral("avcodec_receive_frame failed: %1").arg(ffmpegErrorString(ret)));
            return nullptr;
        }

        if (m_draining) {
            return nullptr;
        }

        while (true) {
            ret = av_read_frame(m_formatContext, m_packet);
            if (ret < 0) {
                avcodec_send_packet(m_codecContext, nullptr);
                m_draining = true;
                break;
            }
            const int containerPacketIndex = m_containerPacketIndex++;

            if (m_packet->stream_index != m_videoStreamIndex) {
                const QVector<FrameAnalysis> analyses =
                    m_accessUnitAnalyzer->parseNonVideoPacket(m_packet, containerPacketIndex);
                for (const FrameAnalysis &analysis : analyses) {
                    m_pendingAccessUnitAnalyses.append(analysis);
                }
                av_packet_unref(m_packet);
                continue;
            }

            const std::optional<AccessUnitAnalyzer::ParsedVideoPacket> parsed =
                m_accessUnitAnalyzer->parseVideoPacket(m_packet, containerPacketIndex);
            if (parsed.has_value() && parsed->hasFrame) {
                m_pendingFrames.append({parsed->analysis, parsed->checkpoint});
            }

            ret = avcodec_send_packet(m_codecContext, m_packet);
            av_packet_unref(m_packet);
            if (ret < 0) {
                setError(QStringLiteral("avcodec_send_packet failed: %1").arg(ffmpegErrorString(ret)));
                return nullptr;
            }
            break;
        }
    }
}

StreamInfo FFmpegDecoder::getStreamInfo() const
{
    return m_streamInfo;
}

FrameAnalysis FFmpegDecoder::lastFrameAnalysis() const
{
    return m_lastFrameAnalysis;
}

QVector<FrameAnalysis> FFmpegDecoder::takePendingAccessUnitAnalyses()
{
    QVector<FrameAnalysis> analyses = m_pendingAccessUnitAnalyses;
    m_pendingAccessUnitAnalyses.clear();
    return analyses;
}

FrameSeekCheckpoint FFmpegDecoder::lastFrameSeekCheckpoint() const
{
    return m_lastFrameSeekCheckpoint;
}

QString FFmpegDecoder::lastError() const
{
    return m_lastError;
}

void FFmpegDecoder::close()
{
    m_draining = false;
    m_videoStreamIndex = -1;
    m_containerPacketIndex = 0;
    m_streamInfo = StreamInfo {};
    m_pendingFrames.clear();
    m_pendingAccessUnitAnalyses.clear();
    m_lastFrameAnalysis = FrameAnalysis {};
    m_lastFrameSeekCheckpoint = FrameSeekCheckpoint {};
    m_accessUnitAnalyzer->clear();

    if (m_codecContext != nullptr) {
        avcodec_free_context(&m_codecContext);
    }

    if (m_formatContext != nullptr) {
        avformat_close_input(&m_formatContext);
    }

    if (m_packet != nullptr) {
        av_packet_unref(m_packet);
    }

    if (m_frame != nullptr) {
        av_frame_unref(m_frame);
    }
}

DecodedVideoFramePtr FFmpegDecoder::copyFrame(const AVFrame *frame)
{
    if (frame == nullptr) {
        return {};
    }

    auto result = std::make_shared<DecodedVideoFrame>();
    result->width = frame->width;
    result->height = frame->height;
    result->pixelFormat = static_cast<AVPixelFormat>(frame->format);
    result->pts = frame->best_effort_timestamp;

    const AVPixFmtDescriptor *descriptor = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format));

    for (int plane = 0; plane < 4; ++plane) {
        result->lineSize[plane] = frame->linesize[plane];
        if (frame->data[plane] == nullptr || frame->linesize[plane] <= 0) {
            continue;
        }

        int planeHeight = frame->height;
        if (descriptor != nullptr && plane > 0) {
            planeHeight = AV_CEIL_RSHIFT(frame->height, descriptor->log2_chroma_h);
        }

        const int bytes = frame->linesize[plane] * planeHeight;
        result->planes[plane] = QByteArray(reinterpret_cast<const char *>(frame->data[plane]), bytes);
    }

    return result;
}

void FFmpegDecoder::setError(const QString &message)
{
    m_lastError = message;
}

QString FFmpegDecoder::ffmpegErrorString(int errorCode)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}
