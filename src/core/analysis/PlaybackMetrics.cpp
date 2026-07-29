#include "core/analysis/PlaybackMetrics.h"

#include <algorithm>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

namespace
{
bool hasValidTimeBase(const StreamInfo &streamInfo)
{
    return streamInfo.timeBaseNum > 0 && streamInfo.timeBaseDen > 0;
}

qint64 timestampToUs(qint64 timestamp, const StreamInfo &streamInfo)
{
    if (!hasValidTimeBase(streamInfo) || timestamp == AV_NOPTS_VALUE) {
        return -1;
    }
    return std::max<qint64>(0,
                            av_rescale_q(timestamp,
                                         AVRational {streamInfo.timeBaseNum, streamInfo.timeBaseDen},
                                         AVRational {1, AV_TIME_BASE}));
}
}

PlaybackMetrics calculatePlaybackMetrics(const StreamInfo &streamInfo,
                                         const FrameAnalysis &analysis)
{
    PlaybackMetrics metrics;
    metrics.timestampUs = timestampToUs(analysis.pts, streamInfo);

    const qint64 packetSize = analysis.packet.size > 0
        ? static_cast<qint64>(analysis.packet.size)
        : static_cast<qint64>(analysis.packet.bytes.size());
    if (packetSize <= 0) {
        return metrics;
    }

    if (analysis.packet.duration > 0 && hasValidTimeBase(streamInfo)) {
        const qint64 durationUs = av_rescale_q(
            analysis.packet.duration,
            AVRational {streamInfo.timeBaseNum, streamInfo.timeBaseDen},
            AVRational {1, AV_TIME_BASE});
        if (durationUs > 0) {
            metrics.bitRateBitsPerSecond = static_cast<double>(
                static_cast<long double>(packetSize) * 8.0L * AV_TIME_BASE / durationUs);
            return metrics;
        }
    }

    if (streamInfo.frameRate > 0.0) {
        metrics.bitRateBitsPerSecond = static_cast<double>(packetSize) * 8.0 * streamInfo.frameRate;
        metrics.bitRateEstimated = true;
    }
    return metrics;
}

QString formatPlaybackTimestamp(qint64 timestampUs)
{
    if (timestampUs < 0) {
        return QStringLiteral("--:--.---");
    }

    const qint64 totalMilliseconds = timestampUs / 1000;
    const qint64 milliseconds = totalMilliseconds % 1000;
    const qint64 totalSeconds = totalMilliseconds / 1000;
    const qint64 seconds = totalSeconds % 60;
    const qint64 totalMinutes = totalSeconds / 60;
    if (totalMinutes < 60) {
        return QStringLiteral("%1:%2.%3")
            .arg(totalMinutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'))
            .arg(milliseconds, 3, 10, QLatin1Char('0'));
    }

    const qint64 hours = totalMinutes / 60;
    const qint64 minutes = totalMinutes % 60;
    return QStringLiteral("%1:%2:%3.%4")
        .arg(hours)
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(milliseconds, 3, 10, QLatin1Char('0'));
}

QString formatPlaybackBitRate(double bitsPerSecond, bool estimated)
{
    if (bitsPerSecond <= 0.0) {
        return QStringLiteral("-");
    }

    const QString prefix = estimated ? QStringLiteral("~") : QString {};
    if (bitsPerSecond < 1000000.0) {
        return QStringLiteral("%1%2 kbps")
            .arg(prefix, QString::number(bitsPerSecond / 1000.0, 'f', 0));
    }
    return QStringLiteral("%1%2 Mbps")
        .arg(prefix, QString::number(bitsPerSecond / 1000000.0, 'f', 2));
}
