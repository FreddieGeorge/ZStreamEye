#include "core/analysis/BitstreamSearch.h"

#include <QByteArray>

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

void findsSpacedHexBytes()
{
    const QByteArray bytes = QByteArray::fromHex("1200000165");
    const QVector<BitstreamSearchMatch> matches = findBitstreamMatches(bytes, QStringLiteral("00 00 01"));

    require(matches.size() == 1, "spaced hex match count");
    require(matches[0].byteOffset == 1 && matches[0].byteLength == 3,
            "spaced hex match range");
}

void findsTextBytes()
{
    const QByteArray bytes("prefix NAL suffix");
    const QVector<BitstreamSearchMatch> matches = findBitstreamMatches(bytes, QStringLiteral("NAL"));

    require(matches.size() == 1, "text match count");
    require(matches[0].byteOffset == 7 && matches[0].byteLength == 3,
            "text match range");
}

void searchesHexAndTextInterpretationsTogether()
{
    QByteArray bytes = QByteArray::fromHex("abcd");
    bytes += QByteArray("ABCD");

    const QVector<BitstreamSearchMatch> matches = findBitstreamMatches(bytes, QStringLiteral("ABCD"));

    require(matches.size() == 2, "combined hex and text match count");
    require(matches[0].byteOffset == 0 && matches[0].byteLength == 2,
            "combined hex match range");
    require(matches[1].byteOffset == 2 && matches[1].byteLength == 4,
            "combined text match range");
}

void keepsOverlappingMatches()
{
    const QVector<BitstreamSearchMatch> matches =
        findBitstreamMatches(QByteArray("AAAA"), QStringLiteral("AAA"));

    require(matches.size() == 2, "overlapping match count");
    require(matches[0].byteOffset == 0 && matches[1].byteOffset == 1,
            "overlapping match offsets");
}

void respectsMatchLimitAndEmptyQueries()
{
    require(findBitstreamMatches(QByteArray("AAAA"), QString()).isEmpty(),
            "empty query has no matches");
    const QVector<BitstreamSearchMatch> matches =
        findBitstreamMatches(QByteArray("AAAA"), QStringLiteral("A"), 2);
    require(matches.size() == 2, "match limit");
    require(matches[0].byteOffset == 0 && matches[1].byteOffset == 1,
            "limited match offsets");
}
}

int main()
{
    findsSpacedHexBytes();
    findsTextBytes();
    searchesHexAndTextInterpretationsTogether();
    keepsOverlappingMatches();
    respectsMatchLimitAndEmptyQueries();
    std::cout << "BitstreamSearch tests passed\n";
    return 0;
}
