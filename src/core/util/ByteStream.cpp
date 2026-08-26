#include "core/util/ByteStream.h"

#include <algorithm>

namespace
{
bool isCompleteFourByteLengthPrefixedPacket(const QByteArray &data)
{
    qsizetype offset = 0;
    int naluCount = 0;
    while (offset + 4 <= data.size()) {
        const int naluLength = readBigEndianLength(
            reinterpret_cast<const uint8_t *>(data.constData() + offset),
            4);
        offset += 4;
        if (naluLength <= 0 || offset + naluLength > data.size()) {
            return false;
        }

        const uint8_t header = static_cast<uint8_t>(data.at(offset));
        const int nalUnitType = header & 0x1f;
        if ((header & 0x80) != 0 || nalUnitType == 0) {
            return false;
        }

        offset += naluLength;
        ++naluCount;
    }
    return naluCount > 0 && offset == data.size();
}
}

bool hasAnnexBStartCode(const QByteArray &data)
{
    // A four-byte AVCC length such as 0x000001ef contains a valid-looking
    // three-byte Annex B prefix. Prefer AVCC when the complete packet validates
    // as length-prefixed NAL units.
    if (isCompleteFourByteLengthPrefixedPacket(data)) {
        return false;
    }

    const qsizetype scanLimit = std::min<qsizetype>(data.size(), 64);
    for (qsizetype i = 0; i < scanLimit; ++i) {
        if (annexBStartCodeSizeAt(data, i) != 0) {
            return true;
        }
    }
    return false;
}

qsizetype annexBStartCodeSizeAt(const QByteArray &data, qsizetype offset)
{
    if (offset + 3 <= data.size()
        && data[offset] == 0
        && data[offset + 1] == 0
        && data[offset + 2] == 1) {
        return 3;
    }

    if (offset + 4 <= data.size()
        && data[offset] == 0
        && data[offset + 1] == 0
        && data[offset + 2] == 0
        && data[offset + 3] == 1) {
        return 4;
    }

    return 0;
}

int readBigEndianLength(const uint8_t *data, int size)
{
    int value = 0;
    for (int i = 0; i < size; ++i) {
        value = (value << 8) | data[i];
    }
    return value;
}
