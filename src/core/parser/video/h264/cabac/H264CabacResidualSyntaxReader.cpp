#include "core/parser/video/h264/cabac/H264CabacResidualSyntaxReader.h"

#include "core/parser/video/h264/cabac/H264CabacDecoder.h"

namespace
{
constexpr int Luma4x4SignificantCoeffFlagCtxIdxBase = 134;
constexpr int Luma4x4SignificantCoeffFlagSkeletonCount = 15;
constexpr int Luma4x4LastSignificantCoeffFlagCtxIdxBase = 166;
constexpr int Luma4x4CoeffAbsLevelMinus1CtxIdxBase = 247;
constexpr int Luma4x4CoeffAbsLevelMinus1RemainingCtxIdxBase = 252;
constexpr int ChromaDcSignificantCoeffFlagCtxIdxBase = 149;
constexpr int ChromaDcLastSignificantCoeffFlagCtxIdxBase = 181;
constexpr int ChromaDcExplicitSignificantCoeffFlagCount = 3;
constexpr int ChromaDcCoeffAbsLevelMinus1CtxIdxBase = 257;
constexpr int ChromaDcCoeffAbsLevelMinus1RemainingCtxIdxBase = 262;
constexpr int CoeffAbsLevelMinus1Ueg0Cutoff = 14;
constexpr int CoeffAbsLevelMinus1Ueg0MaxPrefixOneCount = 29;

struct Luma4x4CoeffAbsLevelContextState
{
    int numDecodAbsLevelEq1 = 0;
    int numDecodAbsLevelGt1 = 0;
};

struct Luma4x4CoeffAbsLevelPrefixStep
{
    const char *name = "";
};

constexpr Luma4x4CoeffAbsLevelPrefixStep Luma4x4CoeffAbsLevelAdditionalPrefixSteps[] = {
    {"next"}, {"third"}, {"fourth"}, {"fifth"}, {"sixth"},
    {"seventh"}, {"eighth"}, {"ninth"}, {"tenth"}, {"eleventh"},
    {"twelfth"}, {"thirteenth"}, {"fourteenth"},
};

int luma4x4CoeffAbsLevelMinus1FirstCtxIdx(const Luma4x4CoeffAbsLevelContextState &state)
{
    const int ctxIdxInc = state.numDecodAbsLevelGt1 != 0
        ? 0
        : (state.numDecodAbsLevelEq1 < 3 ? state.numDecodAbsLevelEq1 + 1 : 4);
    return Luma4x4CoeffAbsLevelMinus1CtxIdxBase + ctxIdxInc;
}

int luma4x4CoeffAbsLevelMinus1RemainingCtxIdx(const Luma4x4CoeffAbsLevelContextState &state)
{
    const int ctxIdxInc = state.numDecodAbsLevelGt1 < 4 ? state.numDecodAbsLevelGt1 : 4;
    return Luma4x4CoeffAbsLevelMinus1RemainingCtxIdxBase + ctxIdxInc;
}

int codedBlockFlagCtxIdx(H264CabacResidualBlockCategory category)
{
    switch (category) {
    case H264CabacResidualBlockCategory::Luma4x4:
        return 85;
    case H264CabacResidualBlockCategory::ChromaDc:
        return 97;
    }
    return -1;
}

QString residualBlockCategoryName(H264CabacResidualBlockCategory category)
{
    switch (category) {
    case H264CabacResidualBlockCategory::Luma4x4:
        return QStringLiteral("luma4x4");
    case H264CabacResidualBlockCategory::ChromaDc:
        return QStringLiteral("chroma_dc");
    }
    return QStringLiteral("unknown");
}

H264CabacResidualBlockResult failedResidualBlockResult(const QString &code,
                                                       const QString &message,
                                                       int ctxIdx = -1)
{
    H264CabacResidualBlockResult result;
    result.ctxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

H264CabacResidualLuma4x4Result failedResidualLuma4x4Result(const QString &code,
                                                           const QString &message,
                                                           int ctxIdx = -1)
{
    H264CabacResidualLuma4x4Result result;
    result.firstCtxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

H264CabacResidualChromaDcResult failedResidualChromaDcResult(const QString &code,
                                                             const QString &message,
                                                             int ctxIdx = -1)
{
    H264CabacResidualChromaDcResult result;
    result.firstCtxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

void appendLuma4x4CoeffReverseScanOrder(H264CabacResidualLuma4x4Result &result,
                                        int terminalScanIndex,
                                        int significantStartIndex)
{
    result.coeffReverseScanIndices.append(terminalScanIndex);
    for (int i = result.significantScanIndices.size() - 1; i >= 0; --i) {
        if (i < significantStartIndex) break;
        const int scanIndex = result.significantScanIndices.at(i);
        if (scanIndex == terminalScanIndex || result.significantCoeffFlags.at(i) == 0) {
            continue;
        }
        result.coeffReverseScanIndices.append(scanIndex);
    }
}

bool readLuma4x4CoeffSignFlagSkeleton(BitReader &reader,
                                       H264CabacDecoder &decoder,
                                       int blockIndex,
                                       int scanIndex,
                                       int coeffAbsLevelMinus1,
                                       H264CabacResidualLuma4x4Result &result)
{
    int sign = 0;
    if (!decoder.decodeBypassBin(reader, &sign)) {
        result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
        result.diagnosticMessage =
            QStringLiteral("CABAC bypass decoding failed while reading luma4x4 coeff_sign_flag[%1][%2].")
                .arg(blockIndex)
                .arg(scanIndex);
        return false;
    }

    result.coeffSignFlags.append(sign);
    result.coeffAbsLevelBlockIndices.append(blockIndex);
    result.coeffAbsLevelMinus1Values.append(coeffAbsLevelMinus1);
    const int magnitude = coeffAbsLevelMinus1 + 1;
    result.coefficientLevels.append(sign != 0 ? -magnitude : magnitude);
    result.incompleteStage = QStringLiteral("residual_coefficients");
    result.diagnosticMessage =
        QStringLiteral("CABAC luma4x4 coeff_sign_flag[%1][%2] was decoded and coefficient level %3 was reconstructed; completing the residual block is not implemented.")
            .arg(blockIndex)
            .arg(scanIndex)
            .arg(result.coefficientLevels.last());
    return true;
}

bool readCoeffAbsLevelMinus1Ueg0Value(const QVector<int> &bins, int *value)
{
    if (bins.isEmpty()) {
        return false;
    }

    int prefixOneCount = 0;
    while (prefixOneCount < bins.size() && bins.at(prefixOneCount) == 1) {
        ++prefixOneCount;
    }
    if (prefixOneCount >= bins.size() || bins.at(prefixOneCount) != 0
        || prefixOneCount > CoeffAbsLevelMinus1Ueg0MaxPrefixOneCount
        || bins.size() != prefixOneCount * 2 + 1) {
        return false;
    }

    int suffixValue = 0;
    for (int i = prefixOneCount + 1; i < bins.size(); ++i) {
        const int bin = bins.at(i);
        if (bin != 0 && bin != 1) {
            return false;
        }
        suffixValue = (suffixValue << 1) | bin;
    }

    if (value != nullptr) {
        *value = ((1 << prefixOneCount) - 1) + suffixValue;
    }
    return true;
}

void appendLuma4x4CoeffAbsLevelPrefixState(H264CabacResidualLuma4x4Result &result,
                                           bool terminated,
                                           int oneCount)
{
    result.coeffAbsLevelPrefixTerminatedFlags.append(terminated ? 1 : 0);
    result.coeffAbsLevelPrefixOneCounts.append(oneCount);
}

bool readLuma4x4CoeffAbsLevelMinus1SuffixBypassBinSkeleton(BitReader &reader,
                                                           H264CabacDecoder &decoder,
                                                           int blockIndex,
                                                           int scanIndex,
                                                           const QString &suffixBinName,
                                                           QVector<int> &bins,
                                                           H264CabacResidualLuma4x4Result &result)
{
    int suffixBin = 0;
    if (!decoder.decodeBypassBin(reader, &suffixBin)) {
        result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
        result.diagnosticMessage =
            QStringLiteral("CABAC bypass decoding failed while reading luma4x4 coeff_abs_level_minus1[%1][%2] %3 suffix bypass bin.")
                .arg(blockIndex)
                .arg(scanIndex)
                .arg(suffixBinName);
        return false;
    }

    bins.append(suffixBin);
    return true;
}

bool readLuma4x4CoeffAbsLevelMinus1Ueg0Skeleton(
    BitReader &reader,
    H264CabacDecoder &decoder,
    int blockIndex,
    int scanIndex,
    int prefixOneCount,
    H264CabacResidualLuma4x4Result &result)
{
    QVector<int> ueg0Bins;
    int ueg0PrefixOneCount = 0;
    while (true) {
        if (!readLuma4x4CoeffAbsLevelMinus1SuffixBypassBinSkeleton(
                reader,
                decoder,
                blockIndex,
                scanIndex,
                QStringLiteral("UEG0 prefix"),
                result.coeffAbsLevelSuffixBins,
                result)) {
            return false;
        }

        const int bin = result.coeffAbsLevelSuffixBins.last();
        ueg0Bins.append(bin);
        result.coeffAbsLevelRemainingInputBins.append(bin);
        if (bin == 0) {
            break;
        }
        ++ueg0PrefixOneCount;
        if (ueg0PrefixOneCount > CoeffAbsLevelMinus1Ueg0MaxPrefixOneCount) {
            result.incompleteStage = QStringLiteral("coeff_abs_level_minus1");
            result.diagnosticCode = QStringLiteral("cabac_residual_incomplete");
            result.diagnosticMessage =
                QStringLiteral("CABAC luma4x4 coeff_abs_level_minus1[%1][%2] UEG0 prefix exceeds the supported safety limit.")
                    .arg(blockIndex)
                    .arg(scanIndex);
            return true;
        }
    }

    for (int i = 0; i < ueg0PrefixOneCount; ++i) {
        if (!readLuma4x4CoeffAbsLevelMinus1SuffixBypassBinSkeleton(
                reader,
                decoder,
                blockIndex,
                scanIndex,
                QStringLiteral("UEG0 information"),
                result.coeffAbsLevelSuffixBins,
                result)) {
            return false;
        }
        const int bin = result.coeffAbsLevelSuffixBins.last();
        ueg0Bins.append(bin);
        result.coeffAbsLevelRemainingInputBins.append(bin);
    }

    const H264CabacCoeffAbsLevelRemainingInput input{prefixOneCount, ueg0Bins};
    int coeffAbsLevelMinus1 = 0;
    if (!h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix(input, &coeffAbsLevelMinus1)) {
        result.diagnosticCode = QStringLiteral("cabac_residual_invalid");
        result.diagnosticMessage =
            QStringLiteral("CABAC luma4x4 coeff_abs_level_minus1[%1][%2] has an invalid UEG0 suffix.")
                .arg(blockIndex)
                .arg(scanIndex);
        return false;
    }

    result.coeffAbsLevelSuffixBinCounts.append(ueg0Bins.size());
    result.coeffAbsLevelRemainingInputBinCounts.append(ueg0Bins.size());
    result.coeffAbsLevelReadyForValueFlags.last() = 1;
    result.coeffAbsLevelReadyPrefixOneCounts.append(prefixOneCount);
    result.coeffAbsLevelReadySuffixBins.append(ueg0Bins);
    result.coeffAbsLevelReadyRemainingInputBins.append(ueg0Bins);
    result.coeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags.append(0);
    result.coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.append(0);
    result.coeffAbsLevelValueInputCompleteFlags.append(1);
    result.coeffAbsLevelFixedInputRecognizedFlags.append(0);
    result.coeffAbsLevelPreUeg0RemainingInputFlags.append(0);
    result.coeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags.append(0);
    return readLuma4x4CoeffSignFlagSkeleton(
        reader,
        decoder,
        blockIndex,
        scanIndex,
        coeffAbsLevelMinus1,
        result);
}

bool readLuma4x4CoeffAbsLevelMinus1PrefixBinSkeleton(BitReader &reader,
                                                     H264CabacDecoder &decoder,
                                                     H264CabacContextModelSet &contexts,
                                                     int ctxIdx,
                                                     const QString &prefixBinName,
                                                     int blockIndex,
                                                     int scanIndex,
                                                     int *bin,
                                                     H264CabacResidualLuma4x4Result &result)
{
    if (!contexts.isInitialized(ctxIdx)) {
        result.diagnosticCode = QStringLiteral("cabac_context_uninitialized");
        result.diagnosticMessage =
            QStringLiteral("CABAC context %1 for luma4x4 coeff_abs_level_minus1[%2][%3] %4 prefix bin is not initialized in the covered context table.")
                .arg(ctxIdx)
                .arg(blockIndex)
                .arg(scanIndex)
                .arg(prefixBinName);
        return false;
    }

    if (!decoder.decodeBin(reader, contexts, ctxIdx, bin)) {
        result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
        result.diagnosticMessage =
            QStringLiteral("CABAC bin decoding failed while reading luma4x4 coeff_abs_level_minus1[%1][%2] %3 prefix bin.")
                .arg(blockIndex)
                .arg(scanIndex)
                .arg(prefixBinName);
        return false;
    }

    return true;
}

bool readLuma4x4CoeffAbsLevelMinus1FirstBinSkeleton(BitReader &reader,
                                                    H264CabacDecoder &decoder,
                                                    H264CabacContextModelSet &contexts,
                                                    int blockIndex,
                                                    int scanIndex,
                                                    bool inferredFinalScan,
                                                    const Luma4x4CoeffAbsLevelContextState &state,
                                                    H264CabacResidualLuma4x4Result &result)
{
    const int firstCtxIdx = luma4x4CoeffAbsLevelMinus1FirstCtxIdx(state);
    int bin = 0;
    if (!readLuma4x4CoeffAbsLevelMinus1PrefixBinSkeleton(
            reader,
            decoder,
            contexts,
            firstCtxIdx,
            QStringLiteral("first"),
            blockIndex,
            scanIndex,
            &bin,
            result)) {
        return false;
    }

    result.coeffAbsLevelScanIndices.append(scanIndex);
    result.coeffAbsLevelInferredFinalFlags.append(inferredFinalScan ? 1 : 0);
    result.coeffAbsLevelPrefixFirstBins.append(bin);
    result.coeffAbsLevelPrefixFirstCtxIndices.append(firstCtxIdx);
    result.coeffAbsLevelReadyForValueFlags.append(0);
    result.incompleteBlockIndex = blockIndex;
    result.incompleteScanIndex = scanIndex;
    result.diagnosticCode = QStringLiteral("cabac_residual_incomplete");
    if (bin == 0) {
        appendLuma4x4CoeffAbsLevelPrefixState(result, true, 0);
        return readLuma4x4CoeffSignFlagSkeleton(
            reader,
            decoder,
            blockIndex,
            scanIndex,
            0,
            result);
    }

    int prefixOneCount = 1;
    const int remainingCtxIdx = luma4x4CoeffAbsLevelMinus1RemainingCtxIdx(state);
    for (const Luma4x4CoeffAbsLevelPrefixStep &prefixStep :
         Luma4x4CoeffAbsLevelAdditionalPrefixSteps) {
        int prefixBin = 0;
        if (!readLuma4x4CoeffAbsLevelMinus1PrefixBinSkeleton(
                reader,
                decoder,
                contexts,
                remainingCtxIdx,
                QString::fromLatin1(prefixStep.name),
                blockIndex,
                scanIndex,
                &prefixBin,
                result)) {
            return false;
        }

        result.coeffAbsLevelPrefixNextBins.append(prefixBin);
        result.coeffAbsLevelPrefixNextCtxIndices.append(remainingCtxIdx);
        if (prefixBin == 0) {
            appendLuma4x4CoeffAbsLevelPrefixState(result, true, prefixOneCount);
            return readLuma4x4CoeffSignFlagSkeleton(
                reader,
                decoder,
                blockIndex,
                scanIndex,
                prefixOneCount,
                result);
        }
        ++prefixOneCount;
        if (prefixOneCount == CoeffAbsLevelMinus1Ueg0Cutoff) {
            appendLuma4x4CoeffAbsLevelPrefixState(result, true, prefixOneCount);
            return readLuma4x4CoeffAbsLevelMinus1Ueg0Skeleton(
                reader,
                decoder,
                blockIndex,
                scanIndex,
                prefixOneCount,
                result);
        }
    }

    appendLuma4x4CoeffAbsLevelPrefixState(result, false, prefixOneCount);
    result.incompleteStage = QStringLiteral("coeff_abs_level_minus1");
    result.diagnosticMessage =
        QStringLiteral("CABAC luma4x4 coeff_abs_level_minus1[%1][%2] covered prefix bins did not terminate; remaining coefficient level prefix parsing is not implemented.")
            .arg(blockIndex)
            .arg(scanIndex);
    return true;
}

bool readLuma4x4CoeffReverseOrderSkeleton(BitReader &reader,
                                          H264CabacDecoder &decoder,
                                          H264CabacContextModelSet &contexts,
                                          int blockIndex,
                                          int reverseScanStartIndex,
                                          H264CabacResidualLuma4x4Result &result)
{
    Luma4x4CoeffAbsLevelContextState state;
    for (int i = reverseScanStartIndex; i < result.coeffReverseScanIndices.size(); ++i) {
        const int scanIndex = result.coeffReverseScanIndices.at(i);
        const int signCountBefore = result.coeffSignFlags.size();
        const int valueCountBefore = result.coeffAbsLevelMinus1Values.size();
        if (!readLuma4x4CoeffAbsLevelMinus1FirstBinSkeleton(
                reader,
                decoder,
                contexts,
                blockIndex,
                scanIndex,
                scanIndex == Luma4x4SignificantCoeffFlagSkeletonCount,
                state,
                result)) {
            return false;
        }
        if (result.coeffAbsLevelMinus1Values.size() > valueCountBefore) {
            if (result.coeffAbsLevelMinus1Values.last() == 0) {
                ++state.numDecodAbsLevelEq1;
            } else {
                ++state.numDecodAbsLevelGt1;
            }
        }
        if (result.incompleteStage != QStringLiteral("residual_coefficients")
            || result.coeffSignFlags.size() == signCountBefore) {
            return true;
        }
    }
    result.incompleteBlockIndex = -1;
    result.incompleteScanIndex = -1;
    result.incompleteStage.clear();
    result.diagnosticCode.clear();
    result.diagnosticMessage.clear();
    return true;
}

bool readLuma4x4SignificantCoeffFlagsSkeleton(BitReader &reader,
                                              H264CabacDecoder &decoder,
                                              H264CabacContextModelSet &contexts,
                                              int blockIndex,
                                              H264CabacResidualLuma4x4Result &result)
{
    const int significantStartIndex = result.significantScanIndices.size();
    const int reverseScanStartIndex = result.coeffReverseScanIndices.size();
    for (int scanIndex = 0; scanIndex < Luma4x4SignificantCoeffFlagSkeletonCount; ++scanIndex) {
        const int ctxIdx = Luma4x4SignificantCoeffFlagCtxIdxBase + scanIndex;
        if (!contexts.isInitialized(ctxIdx)) {
            result.diagnosticCode = QStringLiteral("cabac_context_uninitialized");
            result.diagnosticMessage =
                QStringLiteral("CABAC context %1 for luma4x4 significant_coeff_flag[%2][%3] is not initialized in the covered context table.")
                    .arg(ctxIdx)
                    .arg(blockIndex)
                    .arg(scanIndex);
            return false;
        }

        int bin = 0;
        if (!decoder.decodeBin(reader, contexts, ctxIdx, &bin)) {
            result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
            result.diagnosticMessage =
                QStringLiteral("CABAC bin decoding failed while reading luma4x4 significant_coeff_flag[%1][%2].")
                    .arg(blockIndex)
                    .arg(scanIndex);
            return false;
        }

        result.significantScanIndices.append(scanIndex);
        result.significantCoeffFlags.append(bin);
        if (bin != 0) {
            const int lastCtxIdx = Luma4x4LastSignificantCoeffFlagCtxIdxBase + scanIndex;
            if (!contexts.isInitialized(lastCtxIdx)) {
                result.diagnosticCode = QStringLiteral("cabac_context_uninitialized");
                result.diagnosticMessage =
                    QStringLiteral("CABAC context %1 for luma4x4 last_significant_coeff_flag[%2][%3] is not initialized in the covered context table.")
                        .arg(lastCtxIdx)
                        .arg(blockIndex)
                        .arg(scanIndex);
                return false;
            }

            int lastBin = 0;
            if (!decoder.decodeBin(reader, contexts, lastCtxIdx, &lastBin)) {
                result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
                result.diagnosticMessage =
                    QStringLiteral("CABAC bin decoding failed while reading luma4x4 last_significant_coeff_flag[%1][%2].")
                        .arg(blockIndex)
                        .arg(scanIndex);
                return false;
            }

            result.lastSignificantScanIndices.append(scanIndex);
            result.lastSignificantCoeffFlags.append(lastBin);
            if (lastBin != 0) {
                appendLuma4x4CoeffReverseScanOrder(result, scanIndex, significantStartIndex);
                return readLuma4x4CoeffReverseOrderSkeleton(
                    reader, decoder, contexts, blockIndex, reverseScanStartIndex, result);
            }

            continue;
        }
    }

    appendLuma4x4CoeffReverseScanOrder(
        result, Luma4x4SignificantCoeffFlagSkeletonCount, significantStartIndex);
    return readLuma4x4CoeffReverseOrderSkeleton(
        reader, decoder, contexts, blockIndex, reverseScanStartIndex, result);
}

bool readChromaDcCoeffLevel(BitReader &reader,
                            H264CabacDecoder &decoder,
                            H264CabacContextModelSet &contexts,
                            int component,
                            int scanIndex,
                            Luma4x4CoeffAbsLevelContextState &state,
                            H264CabacResidualChromaDcResult &result)
{
    const int firstCtxIdx = ChromaDcCoeffAbsLevelMinus1CtxIdxBase
        + (state.numDecodAbsLevelGt1 != 0
               ? 0
               : (state.numDecodAbsLevelEq1 < 3 ? state.numDecodAbsLevelEq1 + 1 : 4));
    if (!contexts.isInitialized(firstCtxIdx)) {
        result.diagnosticCode = QStringLiteral("cabac_context_uninitialized");
        result.diagnosticMessage = QStringLiteral("CABAC context %1 for chroma_dc coeff_abs_level_minus1[%2][%3] is not initialized.")
                                       .arg(firstCtxIdx).arg(component).arg(scanIndex);
        return false;
    }

    int bin = 0;
    if (!decoder.decodeBin(reader, contexts, firstCtxIdx, &bin)) {
        result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
        result.diagnosticMessage = QStringLiteral("CABAC bin decoding failed while reading chroma_dc coeff_abs_level_minus1[%1][%2].")
                                       .arg(component).arg(scanIndex);
        return false;
    }
    result.coeffAbsLevelPrefixFirstCtxIndices.append(firstCtxIdx);

    int coeffAbsLevelMinus1 = 0;
    if (bin != 0) {
        int prefixOneCount = 1;
        const int remainingCtxIdx = ChromaDcCoeffAbsLevelMinus1RemainingCtxIdxBase
            + (state.numDecodAbsLevelGt1 < 4 ? state.numDecodAbsLevelGt1 : 4);
        while (prefixOneCount < CoeffAbsLevelMinus1Ueg0Cutoff) {
            if (!contexts.isInitialized(remainingCtxIdx)) {
                result.diagnosticCode = QStringLiteral("cabac_context_uninitialized");
                result.diagnosticMessage = QStringLiteral("CABAC context %1 for chroma_dc coeff_abs_level_minus1[%2][%3] continuation is not initialized.")
                                               .arg(remainingCtxIdx).arg(component).arg(scanIndex);
                return false;
            }
            if (!decoder.decodeBin(reader, contexts, remainingCtxIdx, &bin)) {
                result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
                result.diagnosticMessage = QStringLiteral("CABAC bin decoding failed while reading chroma_dc coeff_abs_level_minus1[%1][%2] continuation.")
                                               .arg(component).arg(scanIndex);
                return false;
            }
            result.coeffAbsLevelPrefixNextCtxIndices.append(remainingCtxIdx);
            if (bin == 0) {
                coeffAbsLevelMinus1 = prefixOneCount;
                break;
            }
            ++prefixOneCount;
        }

        if (prefixOneCount == CoeffAbsLevelMinus1Ueg0Cutoff) {
            QVector<int> ueg0Bins;
            int ueg0PrefixOneCount = 0;
            while (true) {
                if (!decoder.decodeBypassBin(reader, &bin)) {
                    result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
                    result.diagnosticMessage = QStringLiteral("CABAC bypass decoding failed while reading chroma_dc coeff_abs_level_minus1[%1][%2] UEG0 prefix.")
                                                   .arg(component).arg(scanIndex);
                    return false;
                }
                ueg0Bins.append(bin);
                if (bin == 0) break;
                if (++ueg0PrefixOneCount > CoeffAbsLevelMinus1Ueg0MaxPrefixOneCount) {
                    result.diagnosticCode = QStringLiteral("cabac_residual_invalid");
                    result.diagnosticMessage = QStringLiteral("CABAC chroma_dc coeff_abs_level_minus1[%1][%2] UEG0 prefix exceeds the safety limit.")
                                                   .arg(component).arg(scanIndex);
                    return false;
                }
            }
            for (int i = 0; i < ueg0PrefixOneCount; ++i) {
                if (!decoder.decodeBypassBin(reader, &bin)) {
                    result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
                    result.diagnosticMessage = QStringLiteral("CABAC bypass decoding failed while reading chroma_dc coeff_abs_level_minus1[%1][%2] UEG0 information.")
                                                   .arg(component).arg(scanIndex);
                    return false;
                }
                ueg0Bins.append(bin);
            }
            const H264CabacCoeffAbsLevelRemainingInput input{prefixOneCount, ueg0Bins};
            if (!h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix(input, &coeffAbsLevelMinus1)) {
                result.diagnosticCode = QStringLiteral("cabac_residual_invalid");
                result.diagnosticMessage = QStringLiteral("CABAC chroma_dc coeff_abs_level_minus1[%1][%2] has an invalid UEG0 suffix.")
                                               .arg(component).arg(scanIndex);
                return false;
            }
        }
    }

    int sign = 0;
    if (!decoder.decodeBypassBin(reader, &sign)) {
        result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
        result.diagnosticMessage = QStringLiteral("CABAC bypass decoding failed while reading chroma_dc coeff_sign_flag[%1][%2].")
                                       .arg(component).arg(scanIndex);
        return false;
    }
    const int magnitude = coeffAbsLevelMinus1 + 1;
    result.coeffComponentIndices.append(component);
    result.coeffScanIndices.append(scanIndex);
    result.coeffAbsLevelMinus1Values.append(coeffAbsLevelMinus1);
    result.coeffSignFlags.append(sign);
    result.coefficientLevels.append(sign != 0 ? -magnitude : magnitude);
    if (coeffAbsLevelMinus1 == 0) ++state.numDecodAbsLevelEq1;
    else ++state.numDecodAbsLevelGt1;
    return true;
}

bool readChromaDcCoefficients(BitReader &reader,
                              H264CabacDecoder &decoder,
                              H264CabacContextModelSet &contexts,
                              int component,
                              H264CabacResidualChromaDcResult &result)
{
    QVector<int> significantScanIndices;
    int terminalScanIndex = ChromaDcExplicitSignificantCoeffFlagCount;
    for (int scanIndex = 0; scanIndex < ChromaDcExplicitSignificantCoeffFlagCount; ++scanIndex) {
        const int ctxIdx = ChromaDcSignificantCoeffFlagCtxIdxBase + scanIndex;
        if (!contexts.isInitialized(ctxIdx)) {
            result.diagnosticCode = QStringLiteral("cabac_context_uninitialized");
            result.diagnosticMessage = QStringLiteral("CABAC context %1 for chroma_dc significant_coeff_flag[%2][%3] is not initialized.")
                                           .arg(ctxIdx).arg(component).arg(scanIndex);
            return false;
        }
        int significant = 0;
        if (!decoder.decodeBin(reader, contexts, ctxIdx, &significant)) {
            result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
            result.diagnosticMessage = QStringLiteral("CABAC bin decoding failed while reading chroma_dc significant_coeff_flag[%1][%2].")
                                           .arg(component).arg(scanIndex);
            return false;
        }
        result.significantComponentIndices.append(component);
        result.significantScanIndices.append(scanIndex);
        result.significantCoeffFlags.append(significant);
        if (significant == 0) continue;
        significantScanIndices.append(scanIndex);

        const int lastCtxIdx = ChromaDcLastSignificantCoeffFlagCtxIdxBase + scanIndex;
        if (!contexts.isInitialized(lastCtxIdx)) {
            result.diagnosticCode = QStringLiteral("cabac_context_uninitialized");
            result.diagnosticMessage = QStringLiteral("CABAC context %1 for chroma_dc last_significant_coeff_flag[%2][%3] is not initialized.")
                                           .arg(lastCtxIdx).arg(component).arg(scanIndex);
            return false;
        }
        int last = 0;
        if (!decoder.decodeBin(reader, contexts, lastCtxIdx, &last)) {
            result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
            result.diagnosticMessage = QStringLiteral("CABAC bin decoding failed while reading chroma_dc last_significant_coeff_flag[%1][%2].")
                                           .arg(component).arg(scanIndex);
            return false;
        }
        result.lastSignificantComponentIndices.append(component);
        result.lastSignificantScanIndices.append(scanIndex);
        result.lastSignificantCoeffFlags.append(last);
        if (last != 0) {
            terminalScanIndex = scanIndex;
            break;
        }
    }

    QVector<int> reverseScanIndices{terminalScanIndex};
    for (int i = significantScanIndices.size() - 1; i >= 0; --i) {
        if (significantScanIndices.at(i) != terminalScanIndex)
            reverseScanIndices.append(significantScanIndices.at(i));
    }
    Luma4x4CoeffAbsLevelContextState state;
    for (int scanIndex : reverseScanIndices) {
        if (!readChromaDcCoeffLevel(reader, decoder, contexts, component, scanIndex, state, result))
            return false;
    }
    return true;
}
}

int h264CabacCoeffAbsLevelMinus1Ueg0Cutoff()
{
    return CoeffAbsLevelMinus1Ueg0Cutoff;
}

bool h264CabacCoeffAbsLevelMinus1UsesUeg0Suffix(int prefixOneCount)
{
    return prefixOneCount >= CoeffAbsLevelMinus1Ueg0Cutoff;
}

bool h264CabacCoeffAbsLevelMinus1IsPreUeg0RemainingInput(int prefixOneCount)
{
    return prefixOneCount >= 0 && prefixOneCount < CoeffAbsLevelMinus1Ueg0Cutoff;
}

bool h264CabacCoeffAbsLevelMinus1HasPreUeg0RemainingInput(
    const H264CabacCoeffAbsLevelRemainingInput &input)
{
    return h264CabacCoeffAbsLevelMinus1IsPreUeg0RemainingInput(input.prefixOneCount)
        && input.bins.size() == 4;
}

bool h264CabacCoeffAbsLevelMinus1NeedsAdditionalPreUeg0Parsing(
    const H264CabacCoeffAbsLevelRemainingInput &input)
{
    return h264CabacCoeffAbsLevelMinus1HasPreUeg0RemainingInput(input);
}

int h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingTargetPrefixOneCount(
    const H264CabacCoeffAbsLevelRemainingInput &input)
{
    return h264CabacCoeffAbsLevelMinus1NeedsAdditionalPreUeg0Parsing(input)
        ? CoeffAbsLevelMinus1Ueg0Cutoff
        : -1;
}

int h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingRemainingPrefixBins(
    const H264CabacCoeffAbsLevelRemainingInput &input)
{
    const int targetPrefixOneCount =
        h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingTargetPrefixOneCount(input);
    return targetPrefixOneCount >= 0 ? targetPrefixOneCount - input.prefixOneCount : -1;
}

bool h264CabacCoeffAbsLevelMinus1CanContinuePreUeg0PrefixParsing(
    const H264CabacCoeffAbsLevelRemainingInput &input)
{
    return h264CabacCoeffAbsLevelMinus1NeedsAdditionalPreUeg0Parsing(input)
        && h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingTargetPrefixOneCount(input) >= 0
        && h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingRemainingPrefixBins(input) > 0;
}

bool h264CabacCoeffAbsLevelMinus1CanComputeFromUeg0Suffix(
    const H264CabacCoeffAbsLevelRemainingInput &input)
{
    return h264CabacCoeffAbsLevelMinus1UsesUeg0Suffix(input.prefixOneCount)
        && readCoeffAbsLevelMinus1Ueg0Value(input.bins, nullptr);
}

bool h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue(
    const H264CabacCoeffAbsLevelRemainingInput &input,
    int *suffixValue)
{
    if (suffixValue == nullptr || !h264CabacCoeffAbsLevelMinus1CanComputeFromUeg0Suffix(input)) {
        return false;
    }

    return readCoeffAbsLevelMinus1Ueg0Value(input.bins, suffixValue);
}

bool h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix(
    const H264CabacCoeffAbsLevelRemainingInput &input,
    int *coeffAbsLevelMinus1Value)
{
    if (coeffAbsLevelMinus1Value == nullptr) {
        return false;
    }

    int suffixValue = 0;
    if (!h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue(input, &suffixValue)) {
        return false;
    }

    *coeffAbsLevelMinus1Value = input.prefixOneCount + suffixValue;
    return true;
}

H264CabacResidualBlockResult h264ReadCabacResidualCodedBlockFlagZero(
    BitReader &reader,
    H264CabacDecoder &decoder,
    H264CabacContextModelSet &contexts,
    H264CabacResidualBlockCategory category)
{
    const int ctxIdx = codedBlockFlagCtxIdx(category);
    if (ctxIdx < 0) {
        return failedResidualBlockResult(
            QStringLiteral("cabac_residual_block_category_unsupported"),
            QStringLiteral("CABAC residual block category is not supported by the coded_block_flag zero reader."),
            ctxIdx);
    }
    if (!contexts.isInitialized(ctxIdx)) {
        return failedResidualBlockResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context %1 for residual coded_block_flag is not initialized in the covered context table.")
                .arg(ctxIdx),
            ctxIdx);
    }

    int bin = 0;
    if (!decoder.decodeBin(reader, contexts, ctxIdx, &bin)) {
        return failedResidualBlockResult(
            QStringLiteral("cabac_bin_decode_failed"),
            QStringLiteral("CABAC bin decoding failed while reading residual coded_block_flag."),
            ctxIdx);
    }

    H264CabacResidualBlockResult result;
    result.ok = true;
    result.ctxIdx = ctxIdx;
    result.codedBlockFlag = bin;
    if (bin != 0) {
        result.diagnosticCode = QStringLiteral("cabac_residual_incomplete");
        result.diagnosticMessage =
            QStringLiteral("CABAC %1 coded_block_flag is 1; significant_coeff_flag parsing is not implemented.")
                .arg(residualBlockCategoryName(category));
        return result;
    }

    result.complete = true;
    return result;
}

H264CabacResidualChromaDcResult h264ReadCabacResidualChromaDcCodedBlockFlagsZero(
    BitReader &reader,
    H264CabacDecoder &decoder,
    H264CabacContextModelSet &contexts,
    int chromaArrayType,
    int codedBlockPatternChroma)
{
    H264CabacResidualChromaDcResult result;
    result.firstCtxIdx = 97;

    if (codedBlockPatternChroma == 0) {
        result.ok = true;
        result.complete = true;
        return result;
    }
    if (chromaArrayType != 1) {
        return failedResidualChromaDcResult(
            QStringLiteral("cabac_residual_incomplete"),
            QStringLiteral("CABAC chroma DC coded_block_flag zero reader currently only supports 4:2:0 chroma."),
            result.firstCtxIdx);
    }
    if (codedBlockPatternChroma != 1 && codedBlockPatternChroma != 2) {
        return failedResidualChromaDcResult(
            QStringLiteral("cabac_residual_incomplete"),
            QStringLiteral("CABAC chroma DC coded_block_flag zero reader only supports coded_block_pattern_chroma 1 or 2."),
            result.firstCtxIdx);
    }

    for (int component = 0; component < 2; ++component) {
        const H264CabacResidualBlockResult block =
            h264ReadCabacResidualCodedBlockFlagZero(
                reader,
                decoder,
                contexts,
                H264CabacResidualBlockCategory::ChromaDc);
        if (!block.ok) {
            result.diagnosticCode = block.diagnosticCode;
            result.diagnosticMessage = QStringLiteral("CABAC chroma_dc coded_block_flag[%1] failed: %2")
                                           .arg(component)
                                           .arg(block.diagnosticMessage);
            return result;
        }
        result.ok = true;
        result.components.append(component);
        result.codedBlockFlags.append(block.codedBlockFlag);
        if (block.codedBlockFlag != 0) {
            result.incompleteComponent = component;
            result.incompleteStage = QStringLiteral("significant_coeff_flag");
            if (!readChromaDcCoefficients(reader, decoder, contexts, component, result)) {
                result.ok = false;
                return result;
            }
            result.incompleteComponent = -1;
            result.incompleteStage.clear();
            result.diagnosticCode.clear();
            result.diagnosticMessage.clear();
        }
    }

    result.complete = true;
    return result;
}

H264CabacResidualLuma4x4Result h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(
    BitReader &reader,
    H264CabacDecoder &decoder,
    H264CabacContextModelSet &contexts,
    int codedBlockPatternLuma)
{
    H264CabacResidualLuma4x4Result result;
    result.firstCtxIdx = 85;

    if (codedBlockPatternLuma <= 0 || codedBlockPatternLuma > 0x0f) {
        return failedResidualLuma4x4Result(
            QStringLiteral("cabac_residual_incomplete"),
            QStringLiteral("CABAC narrow residual path only supports luma coded_block_pattern bits."),
            result.firstCtxIdx);
    }

    for (int luma8x8 = 0; luma8x8 < 4; ++luma8x8) {
        if (((codedBlockPatternLuma >> luma8x8) & 0x01) == 0) {
            continue;
        }
        for (int i4x4 = 0; i4x4 < 4; ++i4x4) {
            const int blockIndex = luma8x8 * 4 + i4x4;
            const H264CabacResidualBlockResult block =
                h264ReadCabacResidualCodedBlockFlagZero(
                    reader,
                    decoder,
                    contexts,
                    H264CabacResidualBlockCategory::Luma4x4);
            if (!block.ok) {
                result.diagnosticCode = block.diagnosticCode;
                result.diagnosticMessage = QStringLiteral("CABAC luma4x4 coded_block_flag[%1] failed: %2")
                                               .arg(blockIndex)
                                               .arg(block.diagnosticMessage);
                return result;
            }
            result.ok = true;
            result.blockIndices.append(blockIndex);
            result.codedBlockFlags.append(block.codedBlockFlag);
            if (!block.complete) {
                result.incompleteBlockIndex = blockIndex;
                result.incompleteStage = QStringLiteral("significant_coeff_flag");
                if (!readLuma4x4SignificantCoeffFlagsSkeleton(
                        reader,
                        decoder,
                        contexts,
                        blockIndex,
                        result)) {
                    result.ok = false;
                    return result;
                }
                if (!result.incompleteStage.isEmpty()) {
                    return result;
                }
            }
        }
    }

    result.complete = true;
    return result;
}
