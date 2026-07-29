#pragma once

#include "core/model/FrameAnalysis.h"
#include "core/model/StreamInfo.h"

#include <QString>

struct PlaybackMetrics
{
    qint64 timestampUs = -1;
    double bitRateBitsPerSecond = 0.0;
    bool bitRateEstimated = false;
};

PlaybackMetrics calculatePlaybackMetrics(const StreamInfo &streamInfo,
                                         const FrameAnalysis &analysis);
QString formatPlaybackTimestamp(qint64 timestampUs);
QString formatPlaybackBitRate(double bitsPerSecond, bool estimated);
