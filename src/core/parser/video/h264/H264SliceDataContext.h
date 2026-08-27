#pragma once

#include "core/parser/video/h264/H264MotionVectorParser.h"
#include "core/parser/video/h264/H264Parser.h"

#include <algorithm>
#include <array>

struct H264MacroblockCoeffState
{
    int luma16x16Dc = 0;
    std::array<int, 16> luma {};
    std::array<std::array<int, 4>, 2> chroma {};
};

struct H264MacroblockMvdState
{
    bool valid = false;
    int x = 0;
    int y = 0;
};

enum class H264MacroblockParseAction
{
    Continue,
    Stop,
    ReaderError
};

struct H264SliceDataContext
{
    H264SliceDataContext(BitReader &readerRef, SliceInfo &sliceRef, const PpsInfo &ppsRef, const SpsInfo &spsRef)
        : reader(readerRef)
        , slice(sliceRef)
        , pps(ppsRef)
        , sps(spsRef)
        , currentAddress(std::clamp(slice.firstMbInSlice, 0, totalMacroblocks - 1))
        , currentQp(slice.derivedQp)
        , normalizedSliceType(slice.sliceType % 5)
        , isISlice(normalizedSliceType == 2 || normalizedSliceType == 4)
        , isPSlice(normalizedSliceType == 0 || normalizedSliceType == 3)
        , isBSlice(normalizedSliceType == 1)
        , chromaArrayType(sps.chromaFormatIdc == 0 ? 0 : sps.chromaFormatIdc)
        , mvStatesL0(totalMacroblocks)
        , mvStatesL1(totalMacroblocks)
        , mvdStatesL0(totalMacroblocks)
        , coeffStates(totalMacroblocks)
    {
    }

    void appendDiagnostic(const QString &code, const QString &message)
    {
        if (message.isEmpty()) {
            return;
        }
        slice.diagnostics.append({code, message});
        slice.macroblockParseWarnings.append(message);
    }

    void appendEstimatedRemainder(const QString &code, const QString &message)
    {
        appendDiagnostic(code, message);
        for (int address = currentAddress; address < totalMacroblocks; ++address) {
            MacroblockInfo mb;
            mb.address = address;
            mb.mbType = QStringLiteral("Estimated");
            mb.predictionMode = QStringLiteral("unknown");
            mb.qp = currentQp;
            mb.note = message.isEmpty()
                ? QStringLiteral("QP carried forward after slice_data parsing stopped.")
                : message;
            slice.macroblocks.append(mb);
        }
    }

    void addMacroblockField(MacroblockInfo &mb,
                            const QString &name,
                            qsizetype start,
                            qsizetype end,
                            const QString &value)
    {
        mb.fields.append({name, start, end - start, value});
    }

    quint32 readUEMacroblockField(MacroblockInfo &mb, const QString &name)
    {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readUE();
        addMacroblockField(mb, name, start, reader.bitOffset(), QString::number(value));
        return value;
    }

    qint32 readSEMacroblockField(MacroblockInfo &mb, const QString &name)
    {
        const qsizetype start = reader.bitOffset();
        const qint32 value = reader.readSE();
        addMacroblockField(mb, name, start, reader.bitOffset(), QString::number(value));
        return value;
    }

    bool readBitMacroblockField(MacroblockInfo &mb, const QString &name)
    {
        const qsizetype start = reader.bitOffset();
        const bool value = reader.readBit();
        addMacroblockField(mb, name, start, reader.bitOffset(), value ? QStringLiteral("1") : QStringLiteral("0"));
        return value;
    }

    int readTEMacroblockField(MacroblockInfo &mb, const QString &name, int range)
    {
        if (range <= 0) {
            addMacroblockField(mb, name, reader.bitOffset(), reader.bitOffset(), QStringLiteral("0"));
            return 0;
        }
        if (range == 1) {
            const qsizetype start = reader.bitOffset();
            const int value = reader.readBit() ? 0 : 1;
            addMacroblockField(mb, name, start, reader.bitOffset(), QString::number(value));
            return value;
        }
        return static_cast<int>(readUEMacroblockField(mb, name));
    }

    BitReader &reader;
    SliceInfo &slice;
    const PpsInfo &pps;
    const SpsInfo &sps;
    const int totalMacroblocks = std::max(1, slice.picWidthInMbs * slice.picHeightInMbs);
    int currentAddress = 0;
    int currentQp = 26;
    const int normalizedSliceType = 0;
    const bool isISlice = false;
    const bool isPSlice = false;
    const bool isBSlice = false;
    const int chromaArrayType = 1;
    QVector<H264MacroblockMvState> mvStatesL0;
    QVector<H264MacroblockMvState> mvStatesL1;
    QVector<H264MacroblockMvdState> mvdStatesL0;
    QVector<H264MacroblockCoeffState> coeffStates;
};
