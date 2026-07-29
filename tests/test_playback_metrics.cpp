#include "core/analysis/PlaybackMetrics.h"

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void calculatesTimestampAndPacketBitRate()
{
    StreamInfo stream;
    stream.timeBaseNum = 1;
    stream.timeBaseDen = 90000;
    stream.frameRate = 25.0;

    FrameAnalysis analysis;
    analysis.pts = 90000;
    analysis.packet.size = 125000;
    analysis.packet.duration = 4500;

    const PlaybackMetrics metrics = calculatePlaybackMetrics(stream, analysis);
    require(metrics.timestampUs == 1000000, "playback timestamp converted from stream time base");
    require(metrics.bitRateBitsPerSecond == 20000000.0, "packet bitrate uses packet duration");
    require(!metrics.bitRateEstimated, "packet duration bitrate is not estimated");
}

void estimatesBitRateFromFrameRateWhenDurationIsMissing()
{
    StreamInfo stream;
    stream.timeBaseNum = 1;
    stream.timeBaseDen = 1000;
    stream.frameRate = 25.0;

    FrameAnalysis analysis;
    analysis.pts = 2500;
    analysis.packet.size = 50000;

    const PlaybackMetrics metrics = calculatePlaybackMetrics(stream, analysis);
    require(metrics.timestampUs == 2500000, "playback millisecond time base conversion");
    require(metrics.bitRateBitsPerSecond == 10000000.0, "frame-rate bitrate estimate");
    require(metrics.bitRateEstimated, "frame-rate bitrate is marked estimated");
}

void handlesUnavailablePlaybackValues()
{
    const PlaybackMetrics metrics = calculatePlaybackMetrics(StreamInfo {}, FrameAnalysis {});
    require(metrics.timestampUs < 0, "missing time base has no timestamp");
    require(metrics.bitRateBitsPerSecond == 0.0, "missing packet has no bitrate");
    require(!metrics.bitRateEstimated, "missing bitrate is not estimated");
}

void formatsReadablePlaybackValues()
{
    require(formatPlaybackTimestamp(125456000) == QStringLiteral("02:05.456"),
            "minute-second timestamp format");
    require(formatPlaybackTimestamp(3723004000LL) == QStringLiteral("1:02:03.004"),
            "hour timestamp format");
    require(formatPlaybackTimestamp(-1) == QStringLiteral("--:--.---"),
            "unknown timestamp format");
    require(formatPlaybackBitRate(842000.0, false) == QStringLiteral("842 kbps"),
            "kilobit bitrate format");
    require(formatPlaybackBitRate(4820000.0, false) == QStringLiteral("4.82 Mbps"),
            "megabit bitrate format");
    require(formatPlaybackBitRate(10000000.0, true) == QStringLiteral("~10.00 Mbps"),
            "estimated bitrate format");
    require(formatPlaybackBitRate(0.0, false) == QStringLiteral("-"),
            "unknown bitrate format");
}
}

int main()
{
    calculatesTimestampAndPacketBitRate();
    estimatesBitRateFromFrameRateWhenDurationIsMissing();
    handlesUnavailablePlaybackValues();
    formatsReadablePlaybackValues();
    std::cout << "PlaybackMetrics tests passed\n";
    return 0;
}
