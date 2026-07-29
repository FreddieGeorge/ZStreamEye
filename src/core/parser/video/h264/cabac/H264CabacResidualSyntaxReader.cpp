#include "core/parser/video/h264/cabac/H264CabacResidualSyntaxReader.h"

#include "core/parser/video/h264/cabac/H264CabacDecoder.h"

namespace
{
constexpr int Luma4x4SignificantCoeffFlagCtxIdxBase = 134;
constexpr int Luma4x4SignificantCoeffFlagSkeletonCount = 15;
constexpr int Luma4x4LastSignificantCoeffFlagCtxIdxBase = 166;
constexpr int Luma4x4CoeffAbsLevelMinus1FirstCtxIdx = 248;
constexpr int Luma4x4CoeffAbsLevelMinus1NextCtxIdx = 252;
constexpr int Luma4x4CoeffAbsLevelMinus1ThirdCtxIdx = 253;
constexpr int Luma4x4CoeffAbsLevelMinus1FourthCtxIdx = 254;
constexpr int Luma4x4CoeffAbsLevelMinus1FifthCtxIdx = 255;
constexpr int Luma4x4CoeffAbsLevelMinus1DirectSignMaxOneCount = 3;
constexpr int CoeffAbsLevelMinus1Ueg0Cutoff = 14;

struct Luma4x4CoeffAbsLevelPrefixContext
{
    int ctxIdx = -1;
    const char *name = "";
    bool required = false;
};

constexpr Luma4x4CoeffAbsLevelPrefixContext Luma4x4CoeffAbsLevelAdditionalPrefixContexts[] = {
    {Luma4x4CoeffAbsLevelMinus1NextCtxIdx, "next", true},
    {Luma4x4CoeffAbsLevelMinus1ThirdCtxIdx, "third", false},
    {Luma4x4CoeffAbsLevelMinus1FourthCtxIdx, "fourth", false},
    {Luma4x4CoeffAbsLevelMinus1FifthCtxIdx, "fifth", false},
};

constexpr const char *Luma4x4CoeffAbsLevelSuffixBinNames[] = {
    "first",
    "second",
    "third",
    "fourth",
};

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
                                        int terminalScanIndex)
{
    result.coeffReverseScanIndices.append(terminalScanIndex);
    for (int i = result.significantScanIndices.size() - 1; i >= 0; --i) {
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
    result.incompleteStage = QStringLiteral("residual_coefficients");
    result.diagnosticMessage =
        QStringLiteral("CABAC luma4x4 coeff_sign_flag[%1][%2] was decoded; completing non-zero residual coefficients is not implemented.")
            .arg(blockIndex)
            .arg(scanIndex);
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

bool isLuma4x4CoeffAbsLevelMinus1FixedReadyInput(int prefixOneCount,
                                                 const QVector<int> &suffixBins)
{
    if (prefixOneCount != 4 || suffixBins.size() != 4) {
        return false;
    }

    for (int suffixBin : suffixBins) {
        if (suffixBin != 0) {
            return false;
        }
    }
    return true;
}

bool readLuma4x4CoeffAbsLevelMinus1RemainingSkeleton(BitReader &reader,
                                                     H264CabacDecoder &decoder,
                                                     int blockIndex,
                                                     int scanIndex,
                                                     int prefixOneCount,
                                                     H264CabacResidualLuma4x4Result &result)
{
    const char *lastSuffixBinName = "";
    int suffixBinCount = 0;
    QVector<int> suffixBinsForCoeff;
    QVector<int> remainingInputBinsForCoeff;
    for (const char *suffixBinName : Luma4x4CoeffAbsLevelSuffixBinNames) {
        lastSuffixBinName = suffixBinName;
        if (!readLuma4x4CoeffAbsLevelMinus1SuffixBypassBinSkeleton(
                reader,
                decoder,
                blockIndex,
                scanIndex,
                QString::fromLatin1(suffixBinName),
                result.coeffAbsLevelSuffixBins,
                result)) {
            return false;
        }
        suffixBinsForCoeff.append(result.coeffAbsLevelSuffixBins.last());
        remainingInputBinsForCoeff.append(result.coeffAbsLevelSuffixBins.last());
        result.coeffAbsLevelRemainingInputBins.append(result.coeffAbsLevelSuffixBins.last());
        ++suffixBinCount;
    }

    result.coeffAbsLevelSuffixBinCounts.append(suffixBinCount);
    result.coeffAbsLevelRemainingInputBinCounts.append(suffixBinCount);
    if (!result.coeffAbsLevelReadyForValueFlags.isEmpty()) {
        result.coeffAbsLevelReadyForValueFlags.last() = 1;
    }
    result.coeffAbsLevelReadyPrefixOneCounts.append(prefixOneCount);
    result.coeffAbsLevelReadySuffixBins.append(suffixBinsForCoeff);
    result.coeffAbsLevelReadyRemainingInputBins.append(remainingInputBinsForCoeff);
    const bool fixedInputRecognized =
        isLuma4x4CoeffAbsLevelMinus1FixedReadyInput(prefixOneCount, suffixBinsForCoeff);
    const H264CabacCoeffAbsLevelRemainingInput remainingInput{
        prefixOneCount,
        remainingInputBinsForCoeff,
    };
    const bool needsAdditionalPreUeg0Parsing =
        h264CabacCoeffAbsLevelMinus1NeedsAdditionalPreUeg0Parsing(remainingInput);
    const int additionalPreUeg0ParsingTargetPrefixOneCount =
        h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingTargetPrefixOneCount(remainingInput);
    const int additionalPreUeg0ParsingRemainingPrefixBins =
        h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingRemainingPrefixBins(remainingInput);
    const bool canContinuePreUeg0PrefixParsing =
        h264CabacCoeffAbsLevelMinus1CanContinuePreUeg0PrefixParsing(remainingInput);
    result.coeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags.append(
        needsAdditionalPreUeg0Parsing ? 1 : 0);
    if (additionalPreUeg0ParsingTargetPrefixOneCount >= 0) {
        result.coeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts.append(
            additionalPreUeg0ParsingTargetPrefixOneCount);
    }
    if (additionalPreUeg0ParsingRemainingPrefixBins >= 0) {
        result.coeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts.append(
            additionalPreUeg0ParsingRemainingPrefixBins);
    }
    result.coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.append(
        canContinuePreUeg0PrefixParsing ? 1 : 0);
    result.coeffAbsLevelValueInputCompleteFlags.append(1);
    result.coeffAbsLevelFixedInputRecognizedFlags.append(fixedInputRecognized ? 1 : 0);
    result.coeffAbsLevelPreUeg0RemainingInputFlags.append(
        fixedInputRecognized && h264CabacCoeffAbsLevelMinus1HasPreUeg0RemainingInput(remainingInput) ? 1 : 0);
    result.coeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags.append(
        needsAdditionalPreUeg0Parsing ? 1 : 0);
    result.incompleteStage = QStringLiteral("coeff_abs_level_minus1");
    result.diagnosticMessage =
        QStringLiteral("CABAC luma4x4 coeff_abs_level_minus1[%1][%2] %3 suffix bypass bin was decoded after prefix one-count %4; computing coeff_abs_level_minus1 is not implemented.")
            .arg(blockIndex)
            .arg(scanIndex)
            .arg(QString::fromLatin1(lastSuffixBinName))
            .arg(prefixOneCount);
    return true;
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
                                                    H264CabacResidualLuma4x4Result &result)
{
    int bin = 0;
    if (!readLuma4x4CoeffAbsLevelMinus1PrefixBinSkeleton(
            reader,
            decoder,
            contexts,
            Luma4x4CoeffAbsLevelMinus1FirstCtxIdx,
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
    result.coeffAbsLevelReadyForValueFlags.append(0);
    result.incompleteBlockIndex = blockIndex;
    result.incompleteScanIndex = scanIndex;
    result.diagnosticCode = QStringLiteral("cabac_residual_incomplete");
    if (bin == 0) {
        appendLuma4x4CoeffAbsLevelPrefixState(result, true, 0);
        return readLuma4x4CoeffSignFlagSkeleton(reader, decoder, blockIndex, scanIndex, result);
    }

    int prefixOneCount = 1;
    for (const Luma4x4CoeffAbsLevelPrefixContext &prefixContext :
         Luma4x4CoeffAbsLevelAdditionalPrefixContexts) {
        if (!contexts.isInitialized(prefixContext.ctxIdx) && !prefixContext.required) {
            break;
        }

        int prefixBin = 0;
        if (!readLuma4x4CoeffAbsLevelMinus1PrefixBinSkeleton(
                reader,
                decoder,
                contexts,
                prefixContext.ctxIdx,
                QString::fromLatin1(prefixContext.name),
                blockIndex,
                scanIndex,
                &prefixBin,
                result)) {
            return false;
        }

        result.coeffAbsLevelPrefixNextBins.append(prefixBin);
        if (prefixBin == 0) {
            appendLuma4x4CoeffAbsLevelPrefixState(result, true, prefixOneCount);
            if (prefixOneCount > Luma4x4CoeffAbsLevelMinus1DirectSignMaxOneCount) {
                return readLuma4x4CoeffAbsLevelMinus1RemainingSkeleton(
                    reader,
                    decoder,
                    blockIndex,
                    scanIndex,
                    prefixOneCount,
                    result);
            }
            return readLuma4x4CoeffSignFlagSkeleton(reader, decoder, blockIndex, scanIndex, result);
        }
        ++prefixOneCount;
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
                                          H264CabacResidualLuma4x4Result &result)
{
    const int coefficientLimit = result.coeffReverseScanIndices.size() < 2
        ? result.coeffReverseScanIndices.size()
        : 2;
    for (int i = 0; i < coefficientLimit; ++i) {
        const int scanIndex = result.coeffReverseScanIndices.at(i);
        const int signCountBefore = result.coeffSignFlags.size();
        if (!readLuma4x4CoeffAbsLevelMinus1FirstBinSkeleton(
                reader,
                decoder,
                contexts,
                blockIndex,
                scanIndex,
                scanIndex == Luma4x4SignificantCoeffFlagSkeletonCount,
                result)) {
            return false;
        }
        if (result.incompleteStage != QStringLiteral("residual_coefficients")
            || result.coeffSignFlags.size() == signCountBefore) {
            return true;
        }
    }
    return true;
}

bool readLuma4x4SignificantCoeffFlagsSkeleton(BitReader &reader,
                                              H264CabacDecoder &decoder,
                                              H264CabacContextModelSet &contexts,
                                              int blockIndex,
                                              H264CabacResidualLuma4x4Result &result)
{
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
                appendLuma4x4CoeffReverseScanOrder(result, scanIndex);
                return readLuma4x4CoeffReverseOrderSkeleton(reader, decoder, contexts, blockIndex, result);
            }

            continue;
        }
    }

    appendLuma4x4CoeffReverseScanOrder(result, Luma4x4SignificantCoeffFlagSkeletonCount);
    return readLuma4x4CoeffReverseOrderSkeleton(reader, decoder, contexts, blockIndex, result);
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
        && (input.bins == QVector<int>{0} || input.bins.size() == 4);
}

bool h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue(
    const H264CabacCoeffAbsLevelRemainingInput &input,
    int *suffixValue)
{
    if (suffixValue == nullptr || !h264CabacCoeffAbsLevelMinus1CanComputeFromUeg0Suffix(input)) {
        return false;
    }

    int value = 0;
    for (int bin : input.bins) {
        if (bin != 0 && bin != 1) {
            return false;
        }
        value = (value << 1) | bin;
    }

    *suffixValue = value;
    return true;
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
        if (!block.complete) {
            result.incompleteComponent = component;
            result.incompleteStage = QStringLiteral("significant_coeff_flag");
            result.diagnosticCode = block.diagnosticCode;
            result.diagnosticMessage =
                QStringLiteral("CABAC chroma_dc coded_block_flag[%1] is 1; significant_coeff_flag parsing is not implemented.")
                    .arg(component);
            return result;
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
                }
                return result;
            }
        }
    }

    result.complete = true;
    return result;
}
