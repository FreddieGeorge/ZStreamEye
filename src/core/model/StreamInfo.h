#pragma once

#include "core/model/CodecKind.h"
#include "core/model/MediaTypes.h"

#include <QMetaType>
#include <QString>
#include <QtGlobal>
#include <QVector>

struct MediaStreamInfo
{
    int streamIndex = -1;
    MediaKind mediaKind = MediaKind::Unknown;
    CodecKind codecKind = CodecKind::Unknown;
    QString codecName;
    QString pixelFormatName;
    QString sampleFormatName;
    QString channelLayoutName;
    qint64 bitRate = 0;
    qint64 durationUs = 0;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    int sampleRate = 0;
    int channels = 0;
    bool selected = false;
    int timeBaseNum = 0;
    int timeBaseDen = 0;
};

struct StreamInfo
{
    QString absoluteFilePath;
    QString fileName;
    QString directory;
    MediaKind mediaKind = MediaKind::Unknown;
    int streamIndex = -1;
    CodecKind codecKind = CodecKind::Unknown;
    QString codecName;
    QString pixelFormatName;
    QString sampleFormatName;
    QString channelLayoutName;
    qint64 sizeBytes = 0;
    qint64 bitRate = 0;
    qint64 durationUs = 0;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    int sampleRate = 0;
    int channels = 0;
    QVector<MediaStreamInfo> streams;
    bool isValid = false;
    int timeBaseNum = 0;
    int timeBaseDen = 0;
};

Q_DECLARE_METATYPE(StreamInfo)
