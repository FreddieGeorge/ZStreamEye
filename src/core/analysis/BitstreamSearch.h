#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

struct BitstreamSearchMatch
{
    qsizetype byteOffset = -1;
    qsizetype byteLength = 0;
};

QVector<QByteArray> bitstreamSearchPatterns(const QString &query);
QVector<BitstreamSearchMatch> findBitstreamMatches(const QByteArray &bytes,
                                                   const QString &query,
                                                   qsizetype maxMatches = -1);
