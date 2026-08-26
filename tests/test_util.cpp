#include "core/util/BitReader.h"
#include "core/util/ByteStream.h"
#include "core/util/Rbsp.h"

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

void testSequentialBitReader()
{
    BitReader reader(QByteArray::fromHex("ac40"));

    require(reader.bitOffset() == 0, "initial bit offset");
    require(reader.readBit(), "first bit");
    require(!reader.readBit(), "second bit");
    require(reader.readBits(3) == 5, "next three bits");
    require(reader.bitOffset() == 5, "bit offset after sequential reads");
    require(reader.readBits(3) == 4, "finish first byte");
    require(reader.bitsRemaining() == 8, "bits remaining after first byte");
    require(!reader.hasError(), "reader has no error after valid reads");
}

void testExpGolombReader()
{
    BitReader reader(QByteArray::fromHex("a6"));

    require(reader.readUE() == 0, "UE code 1 -> 0");
    require(reader.readUE() == 1, "UE code 010 -> 1");
    require(reader.readSE() == -1, "SE codeNum 2 -> -1");
    require(!reader.hasError(), "Exp-Golomb reader has no error");
}

void testReadBitsAt()
{
    bool ok = true;
    require(BitReader::readBitsAt(QByteArray::fromHex("f150"), 0, 12, &ok) == 0x0f15,
            "readBitsAt first 12 bits");
    require(ok, "readBitsAt valid range ok");

    ok = true;
    require(BitReader::readBitsAt(QByteArray::fromHex("f1"), 4, 8, &ok) == 0x01,
            "readBitsAt returns partial accumulated value on truncation");
    require(!ok, "readBitsAt truncated range clears ok");
}

void testAnnexBAndBigEndianHelpers()
{
    const QByteArray packet = QByteArray::fromHex("12000001650000000141");

    require(hasAnnexBStartCode(packet), "Annex B start code detected within scan window");
    require(annexBStartCodeSizeAt(packet, 1) == 3, "3-byte Annex B start code");
    require(annexBStartCodeSizeAt(packet, 5) == 4, "4-byte Annex B start code");
    require(annexBStartCodeSizeAt(packet, 0) == 0, "no start code at non-prefix byte");

    const unsigned char lengthBytes[] = {0x00, 0x01, 0x02, 0x03};
    require(readBigEndianLength(lengthBytes, 4) == 0x00010203, "4-byte big-endian length");
    require(readBigEndianLength(lengthBytes + 1, 2) == 0x0102, "2-byte big-endian length");

    QByteArray ambiguousAvcc = QByteArray::fromHex("00000100");
    ambiguousAvcc.append(char(0x41));
    ambiguousAvcc.append(QByteArray(255, char(0x55)));
    require(!hasAnnexBStartCode(ambiguousAvcc),
            "complete AVCC packet is not mistaken for a 3-byte Annex B prefix");

    const QByteArray incompleteAvcc = QByteArray::fromHex("000001004155");
    require(hasAnnexBStartCode(incompleteAvcc),
            "incomplete length prefix does not hide a real Annex B signature");
}

void testRbspConversionAndPacketRanges()
{
    const QByteArray ebsp = QByteArray::fromHex("000003010200000303");
    const QByteArray rbsp = rbspFromEbsp(reinterpret_cast<const uint8_t *>(ebsp.constData()), ebsp.size());
    require(rbsp == QByteArray::fromHex("00000102000003"), "EBSP to RBSP removes only emulation-prevention bytes");

    const QByteArray nalu = QByteArray::fromHex("670000030102");
    const QVector<qsizetype> offsets = rbspByteToPacketByteOffsets(nalu, 11);
    require(offsets.size() == 4, "RBSP to packet offset count");
    require(offsets.at(0) == 11, "first RBSP byte packet offset");
    require(offsets.at(1) == 12, "second RBSP byte packet offset");
    require(offsets.at(2) == 14, "third RBSP byte skips emulation-prevention byte");
    require(offsets.at(3) == 15, "fourth RBSP byte packet offset");

    const QVector<AnalysisBitRange> ranges = packetBitRangesForRbspField(12, 12, offsets);
    require(ranges.size() == 2, "RBSP field crossing emulation-prevention byte splits ranges");
    require(ranges.at(0).bitOffset == 100, "first split range bit offset");
    require(ranges.at(0).bitLength == 4, "first split range bit length");
    require(ranges.at(1).bitOffset == 112, "second split range bit offset");
    require(ranges.at(1).bitLength == 8, "second split range bit length");
    require(ranges.at(0).offsetBasis == QStringLiteral("packet"), "range offset basis");
}
}

int main()
{
    testSequentialBitReader();
    testExpGolombReader();
    testReadBitsAt();
    testAnnexBAndBigEndianHelpers();
    testRbspConversionAndPacketRanges();

    std::cout << "Util tests passed\n";
    return 0;
}
