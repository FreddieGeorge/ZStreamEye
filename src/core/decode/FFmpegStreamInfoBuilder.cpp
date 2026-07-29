#include "core/decode/FFmpegStreamInfoBuilder.h"

#include "core/parser/ParserFactory.h"

#include <QFileInfo>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
}

namespace
{
QString channelLayoutName(const AVCodecParameters *parameters)
{
    if (parameters == nullptr || parameters->codec_type != AVMEDIA_TYPE_AUDIO) {
        return QString {};
    }

    char buffer[128] = {};
    if (parameters->ch_layout.nb_channels > 0
        && av_channel_layout_describe(&parameters->ch_layout, buffer, sizeof(buffer)) >= 0) {
        return QString::fromUtf8(buffer);
    }
    return QString {};
}

MediaStreamInfo mediaStreamInfoFromAvStream(const AVStream *stream, int streamIndex)
{
    MediaStreamInfo streamInfo;
    if (stream == nullptr || stream->codecpar == nullptr) {
        return streamInfo;
    }

    const AVCodecParameters *parameters = stream->codecpar;
    streamInfo.streamIndex = streamIndex;
    streamInfo.mediaKind = mediaKindFromAvMediaType(parameters->codec_type);
    streamInfo.codecKind = codecKindFromAvCodecId(parameters->codec_id);
    const AVCodecDescriptor *descriptor = avcodec_descriptor_get(parameters->codec_id);
    streamInfo.codecName = descriptor != nullptr && descriptor->name != nullptr
        ? QString::fromUtf8(descriptor->name)
        : QString::fromUtf8(avcodec_get_name(parameters->codec_id));
    streamInfo.bitRate = parameters->bit_rate;
    streamInfo.timeBaseNum = stream->time_base.num;
    streamInfo.timeBaseDen = stream->time_base.den;
    streamInfo.durationUs = stream->duration != AV_NOPTS_VALUE
        ? av_rescale_q(stream->duration, stream->time_base, AVRational {1, AV_TIME_BASE})
        : 0;

    if (parameters->codec_type == AVMEDIA_TYPE_VIDEO) {
        const AVRational rate = stream->avg_frame_rate.num != 0
            ? stream->avg_frame_rate
            : stream->r_frame_rate;
        streamInfo.width = parameters->width;
        streamInfo.height = parameters->height;
        streamInfo.frameRate = rationalToDouble(rate);
        const char *pixelFormatName = av_get_pix_fmt_name(static_cast<AVPixelFormat>(parameters->format));
        streamInfo.pixelFormatName = pixelFormatName != nullptr
            ? QString::fromUtf8(pixelFormatName)
            : QString {};
    } else if (parameters->codec_type == AVMEDIA_TYPE_AUDIO) {
        streamInfo.sampleRate = parameters->sample_rate;
        streamInfo.channels = parameters->ch_layout.nb_channels;
        const char *sampleFormatName = av_get_sample_fmt_name(static_cast<AVSampleFormat>(parameters->format));
        streamInfo.sampleFormatName = sampleFormatName != nullptr
            ? QString::fromUtf8(sampleFormatName)
            : QString {};
        streamInfo.channelLayoutName = channelLayoutName(parameters);
    }

    return streamInfo;
}
}

MediaKind mediaKindFromAvMediaType(AVMediaType mediaType)
{
    switch (mediaType) {
    case AVMEDIA_TYPE_VIDEO:
        return MediaKind::Video;
    case AVMEDIA_TYPE_AUDIO:
        return MediaKind::Audio;
    case AVMEDIA_TYPE_SUBTITLE:
        return MediaKind::Subtitle;
    case AVMEDIA_TYPE_DATA:
        return MediaKind::Data;
    default:
        return MediaKind::Unknown;
    }
}

double rationalToDouble(AVRational value)
{
    if (value.num == 0 || value.den == 0) {
        return 0.0;
    }
    return static_cast<double>(value.num) / static_cast<double>(value.den);
}

void populateContainerStreamInfo(AVFormatContext *formatContext,
                                 const QString &filePath,
                                 StreamInfo *streamInfo)
{
    if (streamInfo == nullptr) {
        return;
    }

    *streamInfo = StreamInfo {};

    const QFileInfo info(filePath);
    streamInfo->absoluteFilePath = info.absoluteFilePath();
    streamInfo->fileName = info.fileName();
    streamInfo->directory = info.absolutePath();
    streamInfo->sizeBytes = info.size();

    if (formatContext == nullptr) {
        return;
    }

    streamInfo->bitRate = formatContext->bit_rate;
    streamInfo->durationUs = formatContext->duration;

    for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
        AVStream *stream = formatContext->streams[i];
        const AVCodecParameters *parameters = stream != nullptr ? stream->codecpar : nullptr;
        if (parameters == nullptr) {
            continue;
        }
        streamInfo->streams.append(mediaStreamInfoFromAvStream(stream, static_cast<int>(i)));
    }
}

void populateSelectedVideoStreamInfo(AVFormatContext *formatContext,
                                     AVCodecContext *codecContext,
                                     int videoStreamIndex,
                                     const AVCodec *decoder,
                                     StreamInfo *streamInfo)
{
    if (formatContext == nullptr
        || codecContext == nullptr
        || decoder == nullptr
        || streamInfo == nullptr
        || videoStreamIndex < 0
        || videoStreamIndex >= static_cast<int>(formatContext->nb_streams)) {
        return;
    }

    AVStream *videoStream = formatContext->streams[videoStreamIndex];
    const AVRational rate = videoStream->avg_frame_rate.num != 0
        ? videoStream->avg_frame_rate
        : videoStream->r_frame_rate;

    streamInfo->mediaKind = MediaKind::Video;
    streamInfo->streamIndex = videoStreamIndex;
    streamInfo->codecKind = codecKindFromAvCodecId(codecContext->codec_id);
    streamInfo->codecName = QString::fromUtf8(decoder->name);
    const char *pixelFormatName = av_get_pix_fmt_name(codecContext->pix_fmt);
    streamInfo->pixelFormatName = pixelFormatName != nullptr
        ? QString::fromUtf8(pixelFormatName)
        : QStringLiteral("unknown");
    streamInfo->bitRate = codecContext->bit_rate > 0
        ? codecContext->bit_rate
        : formatContext->bit_rate;
    streamInfo->timeBaseNum = videoStream->time_base.num;
    streamInfo->timeBaseDen = videoStream->time_base.den;
    streamInfo->width = codecContext->width;
    streamInfo->height = codecContext->height;
    streamInfo->frameRate = rationalToDouble(rate);
    for (MediaStreamInfo &mediaStreamInfo : streamInfo->streams) {
        mediaStreamInfo.selected = mediaStreamInfo.streamIndex == videoStreamIndex;
    }
    streamInfo->isValid = true;
}
