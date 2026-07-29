#include "core/analysis/BitstreamSearch.h"

#include <algorithm>

namespace
{
QByteArray hexPattern(const QString &query)
{
    QString compact = query.trimmed();
    if (compact.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        compact.remove(0, 2);
    }

    QString digits;
    digits.reserve(compact.size());
    for (const QChar character : compact) {
        if (character.isSpace()
            || character == QLatin1Char(':')
            || character == QLatin1Char('-')
            || character == QLatin1Char('_')) {
            continue;
        }
        if (!((character >= QLatin1Char('0') && character <= QLatin1Char('9'))
              || (character >= QLatin1Char('a') && character <= QLatin1Char('f'))
              || (character >= QLatin1Char('A') && character <= QLatin1Char('F')))) {
            return {};
        }
        digits += character;
    }

    if (digits.isEmpty() || (digits.size() % 2) != 0) {
        return {};
    }
    return QByteArray::fromHex(digits.toLatin1());
}
}

QVector<QByteArray> bitstreamSearchPatterns(const QString &query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    QVector<QByteArray> patterns;
    const QByteArray asHex = hexPattern(trimmed);
    if (!asHex.isEmpty()) {
        patterns.append(asHex);
    }

    const QByteArray asText = trimmed.toUtf8();
    if (!asText.isEmpty() && !patterns.contains(asText)) {
        patterns.append(asText);
    }
    return patterns;
}

QVector<BitstreamSearchMatch> findBitstreamMatches(const QByteArray &bytes,
                                                   const QString &query,
                                                   qsizetype maxMatches)
{
    if (bytes.isEmpty() || maxMatches == 0) {
        return {};
    }

    QVector<BitstreamSearchMatch> matches;
    for (const QByteArray &pattern : bitstreamSearchPatterns(query)) {
        qsizetype searchFrom = 0;
        while (searchFrom <= bytes.size() - pattern.size()) {
            const qsizetype offset = bytes.indexOf(pattern, searchFrom);
            if (offset < 0) {
                break;
            }
            matches.append({offset, pattern.size()});
            searchFrom = offset + 1;
        }
    }

    std::sort(matches.begin(), matches.end(), [](const BitstreamSearchMatch &left,
                                                  const BitstreamSearchMatch &right) {
        if (left.byteOffset != right.byteOffset) {
            return left.byteOffset < right.byteOffset;
        }
        return left.byteLength < right.byteLength;
    });
    matches.erase(std::unique(matches.begin(), matches.end(), [](const BitstreamSearchMatch &left,
                                                                 const BitstreamSearchMatch &right) {
        return left.byteOffset == right.byteOffset && left.byteLength == right.byteLength;
    }), matches.end());

    if (maxMatches > 0 && matches.size() > maxMatches) {
        matches.resize(maxMatches);
    }
    return matches;
}
