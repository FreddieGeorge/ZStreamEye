#include "core/parser/video/h264/cabac/H264CabacSubMacroblockSyntaxReader.h"

#include "core/parser/video/h264/H264SliceDataContext.h"
#include "core/parser/video/h264/cabac/H264CabacDecoder.h"

#include <cstdlib>
#include <limits>

namespace
{
H264CabacSubMbTypeResult failedSubMbTypeResult(const QString &code, const QString &message, int ctxIdx = -1)
{
    H264CabacSubMbTypeResult result;
    result.ctxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

H264CabacRefIdxResult failedRefIdxResult(const QString &code, const QString &message, int ctxIdx = -1)
{
    H264CabacRefIdxResult result;
    result.ctxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

H264CabacMvdResult failedMvdResult(const QString &code, const QString &message, int ctxIdx = -1)
{
    H264CabacMvdResult result;
    result.ctxIdx = ctxIdx;
    result.diagnosticCode = code;
    result.diagnosticMessage = message;
    return result;
}

int pSubMbPartitionCount(int subMbType)
{
    switch (subMbType) {
    case 0:
        return 1;
    case 1:
    case 2:
        return 2;
    case 3:
        return 4;
    default:
        return 0;
    }
}

int mvdComponentValue(const QVector<H264CabacMvdPair> &subMacroblockMvd,
                      const QVector<bool> &subMacroblockMvdValid,
                      int subIndex,
                      int component)
{
    if (subIndex < 0 || subIndex >= subMacroblockMvd.size()
        || subIndex >= subMacroblockMvdValid.size() || !subMacroblockMvdValid.at(subIndex)) {
        return 0;
    }

    const H264CabacMvdPair pair = subMacroblockMvd.at(subIndex);
    return component == 0 ? pair.x : pair.y;
}

int mvdCtxIdxIncFromNeighborAbsMvd(int neighborAbsMvd)
{
    if (neighborAbsMvd < 3) {
        return 0;
    }
    if (neighborAbsMvd <= 32) {
        return 1;
    }
    return 2;
}

int p8x8MvdCtxIdxInc(const QVector<H264CabacMvdPair> &subMacroblockMvd,
                     const QVector<bool> &subMacroblockMvdValid,
                     int subIndex,
                     int component)
{
    const int leftSubIndex = (subIndex % 2) == 1 ? subIndex - 1 : -1;
    const int topSubIndex = subIndex >= 2 ? subIndex - 2 : -1;
    const int neighborAbsMvd =
        std::abs(mvdComponentValue(subMacroblockMvd, subMacroblockMvdValid, leftSubIndex, component))
        + std::abs(mvdComponentValue(subMacroblockMvd, subMacroblockMvdValid, topSubIndex, component));
    return mvdCtxIdxIncFromNeighborAbsMvd(neighborAbsMvd);
}

bool decodeMvdUeg3Suffix(BitReader &reader,
                         H264CabacDecoder &decoder,
                         int *value)
{
    constexpr int Ueg3Order = 3;
    constexpr int MaxPrefixOneCount = 27;

    qint64 decodedValue = 0;
    int suffixBitCount = Ueg3Order;
    for (int prefixOneCount = 0;; ++prefixOneCount) {
        int bin = 0;
        if (!decoder.decodeBypassBin(reader, &bin)) {
            return false;
        }
        if (bin == 0) {
            break;
        }
        if (prefixOneCount >= MaxPrefixOneCount) {
            return false;
        }
        decodedValue += qint64{1} << suffixBitCount;
        ++suffixBitCount;
    }

    for (int bit = suffixBitCount - 1; bit >= 0; --bit) {
        int bin = 0;
        if (!decoder.decodeBypassBin(reader, &bin)) {
            return false;
        }
        decodedValue += qint64{bin} << bit;
    }

    if (decodedValue > std::numeric_limits<int>::max() - 9) {
        return false;
    }
    *value = static_cast<int>(decodedValue);
    return true;
}
}

H264CabacSubMbTypeResult h264ReadCabacPSubMbType(BitReader &reader,
                                                 H264CabacDecoder &decoder,
                                                 H264CabacContextModelSet &contexts,
                                                 const H264SliceDataContext &sliceContext)
{
    if (!sliceContext.isPSlice) {
        return failedSubMbTypeResult(
            QStringLiteral("cabac_slice_type_unsupported"),
            QStringLiteral("CABAC P sub_mb_type reader was used for a non-P slice."));
    }

    int bin = 0;
    const int ctxIdx = 21;
    if (!contexts.isInitialized(ctxIdx)) {
        return failedSubMbTypeResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context 21 for P sub_mb_type is not initialized in the covered context table."),
            ctxIdx);
    }
    if (!decoder.decodeBin(reader, contexts, ctxIdx, &bin)) {
        return failedSubMbTypeResult(
            QStringLiteral("cabac_bin_decode_failed"),
            QStringLiteral("CABAC bin decoding failed while reading P sub_mb_type."),
            ctxIdx);
    }

    H264CabacSubMbTypeResult result;
    result.ok = true;
    result.ctxIdx = ctxIdx;
    if (bin != 0) {
        result.complete = true;
        result.subMbType = 0;
        return result;
    }

    if (!contexts.isInitialized(22)) {
        return failedSubMbTypeResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context 22 for P sub_mb_type is not initialized in the covered context table."),
            22);
    }
    if (!decoder.decodeBin(reader, contexts, 22, &bin)) {
        result.ok = false;
        result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
        result.diagnosticMessage =
            QStringLiteral("CABAC bin decoding failed while reading P sub_mb_type partition branch.");
        return result;
    }
    if (bin == 0) {
        result.complete = true;
        result.subMbType = 1;
        return result;
    }

    if (!contexts.isInitialized(23)) {
        return failedSubMbTypeResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context 23 for P sub_mb_type is not initialized in the covered context table."),
            23);
    }
    if (!decoder.decodeBin(reader, contexts, 23, &bin)) {
        result.ok = false;
        result.diagnosticCode = QStringLiteral("cabac_bin_decode_failed");
        result.diagnosticMessage =
            QStringLiteral("CABAC bin decoding failed while reading P sub_mb_type 4x8/4x4 selector.");
        return result;
    }
    result.complete = true;
    result.subMbType = bin == 0 ? 3 : 2;
    return result;
}

H264CabacSubMbTypesResult h264ReadCabacPSubMbTypes(BitReader &reader,
                                                   H264CabacDecoder &decoder,
                                                   H264CabacContextModelSet &contexts,
                                                   const H264SliceDataContext &sliceContext,
                                                   int subMacroblockCount)
{
    H264CabacSubMbTypesResult result;
    if (subMacroblockCount <= 0) {
        result.ok = true;
        result.complete = true;
        return result;
    }

    for (int subIndex = 0; subIndex < subMacroblockCount; ++subIndex) {
        const H264CabacSubMbTypeResult subMbType =
            h264ReadCabacPSubMbType(reader, decoder, contexts, sliceContext);
        if (!subMbType.ok) {
            result.diagnosticCode = subMbType.diagnosticCode;
            result.diagnosticMessage = QStringLiteral("CABAC P sub_mb_type[%1] failed: %2")
                                           .arg(subIndex)
                                           .arg(subMbType.diagnosticMessage);
            return result;
        }
        if (!subMbType.complete) {
            result.ok = true;
            result.diagnosticCode = subMbType.diagnosticCode;
            result.diagnosticMessage = QStringLiteral("CABAC P sub_mb_type[%1] is incomplete: %2")
                                           .arg(subIndex)
                                           .arg(subMbType.diagnosticMessage);
            return result;
        }
        result.subMbTypes.append(subMbType.subMbType);
    }

    result.ok = true;
    result.complete = true;
    return result;
}

H264CabacRefIdxResult h264ReadCabacRefIdxL0(BitReader &reader,
                                            H264CabacDecoder &decoder,
                                            H264CabacContextModelSet &contexts,
                                            const H264SliceDataContext &sliceContext)
{
    H264CabacRefIdxResult result;
    if (sliceContext.slice.numRefIdxL0ActiveMinus1 <= 0) {
        result.ok = true;
        result.present = false;
        result.refIdx = 0;
        return result;
    }

    int bin = 0;
    const int firstCtxIdx = 54;
    if (!contexts.isInitialized(firstCtxIdx)) {
        return failedRefIdxResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context 54 for ref_idx_l0 is not initialized in the covered context table."),
            firstCtxIdx);
    }
    if (!decoder.decodeBin(reader, contexts, firstCtxIdx, &bin)) {
        return failedRefIdxResult(
            QStringLiteral("cabac_bin_decode_failed"),
            QStringLiteral("CABAC bin decoding failed while reading ref_idx_l0."),
            firstCtxIdx);
    }

    result.ok = true;
    result.present = true;
    result.ctxIdx = firstCtxIdx;
    if (bin == 0) {
        result.refIdx = 0;
        return result;
    }

    result.diagnosticCode = QStringLiteral("cabac_ref_idx_incomplete");
    result.diagnosticMessage =
        QStringLiteral("CABAC ref_idx_l0 greater than zero is not implemented.");
    return result;
}

H264CabacRefIdxListResult h264ReadCabacPSubMbRefIdxL0(BitReader &reader,
                                                      H264CabacDecoder &decoder,
                                                      H264CabacContextModelSet &contexts,
                                                      const H264SliceDataContext &sliceContext,
                                                      int subMacroblockCount)
{
    H264CabacRefIdxListResult result;
    if (subMacroblockCount <= 0) {
        result.ok = true;
        return result;
    }
    if (sliceContext.slice.numRefIdxL0ActiveMinus1 <= 0) {
        result.ok = true;
        result.present = false;
        result.refIdx.fill(0, subMacroblockCount);
        return result;
    }

    result.present = true;
    for (int subIndex = 0; subIndex < subMacroblockCount; ++subIndex) {
        const H264CabacRefIdxResult refIdx =
            h264ReadCabacRefIdxL0(reader, decoder, contexts, sliceContext);
        if (!refIdx.ok) {
            result.diagnosticCode = refIdx.diagnosticCode;
            result.diagnosticMessage = QStringLiteral("CABAC ref_idx_l0[%1] failed: %2")
                                           .arg(subIndex)
                                           .arg(refIdx.diagnosticMessage);
            return result;
        }
        if (!refIdx.diagnosticCode.isEmpty()) {
            result.ok = true;
            result.diagnosticCode = refIdx.diagnosticCode;
            result.diagnosticMessage = QStringLiteral("CABAC ref_idx_l0[%1] is incomplete: %2")
                                           .arg(subIndex)
                                           .arg(refIdx.diagnosticMessage);
            return result;
        }
        result.refIdx.append(refIdx.refIdx);
    }

    result.ok = true;
    return result;
}

H264CabacMvdResult h264ReadCabacMvdL0Component(BitReader &reader,
                                               H264CabacDecoder &decoder,
                                               H264CabacContextModelSet &contexts,
                                               int component,
                                               int ctxIdxInc)
{
    if (component < 0 || component > 1 || ctxIdxInc < 0 || ctxIdxInc > 2) {
        return failedMvdResult(
            QStringLiteral("cabac_mvd_context_invalid"),
            QStringLiteral("CABAC mvd_l0 context derivation produced an invalid component or ctxIdxInc."));
    }

    const int ctxIdx = (component == 0 ? 40 : 47) + ctxIdxInc;
    if (!contexts.isInitialized(ctxIdx)) {
        return failedMvdResult(
            QStringLiteral("cabac_context_uninitialized"),
            QStringLiteral("CABAC context %1 for mvd_l0 is not initialized in the covered context table.")
                .arg(ctxIdx),
            ctxIdx);
    }

    int bin = 0;
    if (!decoder.decodeBin(reader, contexts, ctxIdx, &bin)) {
        return failedMvdResult(
            QStringLiteral("cabac_bin_decode_failed"),
            QStringLiteral("CABAC bin decoding failed while reading mvd_l0."),
            ctxIdx);
    }

    H264CabacMvdResult result;
    result.ok = true;
    result.ctxIdx = ctxIdx;
    if (bin == 0) {
        result.complete = true;
        result.value = 0;
        return result;
    }

    constexpr int suffixCtxOffsets[8] = {3, 4, 5, 6, 6, 6, 6, 6};
    const int ctxBase = component == 0 ? 40 : 47;
    int absMvd = 0;
    for (int candidateAbsMvd = 1; candidateAbsMvd <= 8; ++candidateAbsMvd) {
        const int suffixCtxIdx = ctxBase + suffixCtxOffsets[candidateAbsMvd - 1];
        if (!contexts.isInitialized(suffixCtxIdx)) {
            return failedMvdResult(
                QStringLiteral("cabac_context_uninitialized"),
                QStringLiteral("CABAC context %1 for non-zero mvd_l0 is not initialized in the covered context table.")
                    .arg(suffixCtxIdx),
                suffixCtxIdx);
        }
        if (!decoder.decodeBin(reader, contexts, suffixCtxIdx, &bin)) {
            return failedMvdResult(
                QStringLiteral("cabac_bin_decode_failed"),
                QStringLiteral("CABAC bin decoding failed while reading non-zero mvd_l0 prefix."),
                suffixCtxIdx);
        }
        if (bin == 0) {
            absMvd = candidateAbsMvd;
            break;
        }
    }
    if (absMvd == 0) {
        int ueg3Value = 0;
        if (!decodeMvdUeg3Suffix(reader, decoder, &ueg3Value)) {
            return failedMvdResult(
                QStringLiteral("cabac_bin_decode_failed"),
                QStringLiteral("CABAC bypass UEG3 decoding failed while reading mvd_l0."),
                ctxBase + 6);
        }
        absMvd = 9 + ueg3Value;
    }

    int sign = 0;
    if (!decoder.decodeBypassBin(reader, &sign)) {
        return failedMvdResult(
            QStringLiteral("cabac_bin_decode_failed"),
            QStringLiteral("CABAC bypass sign decoding failed while reading non-zero mvd_l0."),
            ctxIdx);
    }
    result.complete = true;
    result.value = sign == 0 ? absMvd : -absMvd;
    return result;
}

H264CabacMvdListResult h264ReadCabacPSubMbMvdL0(BitReader &reader,
                                                H264CabacDecoder &decoder,
                                                H264CabacContextModelSet &contexts,
                                                const QVector<int> &subMbTypes)
{
    H264CabacMvdListResult result;
    QVector<H264CabacMvdPair> subMacroblockMvd(4);
    QVector<bool> subMacroblockMvdValid(4, false);
    for (int subIndex = 0; subIndex < subMbTypes.size(); ++subIndex) {
        const int partitionCount = pSubMbPartitionCount(subMbTypes.at(subIndex));
        if (partitionCount <= 0) {
            result.diagnosticCode = QStringLiteral("cabac_sub_mb_type_invalid");
            result.diagnosticMessage =
                QStringLiteral("CABAC sub_mb_type[%1] has unsupported value %2.")
                    .arg(subIndex)
                    .arg(subMbTypes.at(subIndex));
            return result;
        }

        for (int partition = 0; partition < partitionCount; ++partition) {
            const int mvdXCtxIdxInc =
                subMbTypes.at(subIndex) == 0
                    ? p8x8MvdCtxIdxInc(subMacroblockMvd, subMacroblockMvdValid, subIndex, 0)
                    : 0;
            const H264CabacMvdResult mvdX =
                h264ReadCabacMvdL0Component(reader, decoder, contexts, 0, mvdXCtxIdxInc);
            if (!mvdX.ok) {
                result.diagnosticCode = mvdX.diagnosticCode;
                result.diagnosticMessage = QStringLiteral("CABAC mvd_l0[%1][%2][0] failed: %3")
                                               .arg(subIndex)
                                               .arg(partition)
                                               .arg(mvdX.diagnosticMessage);
                return result;
            }
            if (!mvdX.complete) {
                result.ok = true;
                result.diagnosticCode = mvdX.diagnosticCode;
                result.diagnosticMessage = QStringLiteral("CABAC mvd_l0[%1][%2][0] is incomplete: %3")
                                               .arg(subIndex)
                                               .arg(partition)
                                               .arg(mvdX.diagnosticMessage);
                return result;
            }

            const int mvdYCtxIdxInc =
                subMbTypes.at(subIndex) == 0
                    ? p8x8MvdCtxIdxInc(subMacroblockMvd, subMacroblockMvdValid, subIndex, 1)
                    : 0;
            const H264CabacMvdResult mvdY =
                h264ReadCabacMvdL0Component(reader, decoder, contexts, 1, mvdYCtxIdxInc);
            if (!mvdY.ok) {
                result.diagnosticCode = mvdY.diagnosticCode;
                result.diagnosticMessage = QStringLiteral("CABAC mvd_l0[%1][%2][1] failed: %3")
                                               .arg(subIndex)
                                               .arg(partition)
                                               .arg(mvdY.diagnosticMessage);
                return result;
            }
            if (!mvdY.complete) {
                result.ok = true;
                result.diagnosticCode = mvdY.diagnosticCode;
                result.diagnosticMessage = QStringLiteral("CABAC mvd_l0[%1][%2][1] is incomplete: %3")
                                               .arg(subIndex)
                                               .arg(partition)
                                               .arg(mvdY.diagnosticMessage);
                return result;
            }

            const H264CabacMvdPair pair {mvdX.value, mvdY.value};
            result.mvd.append(pair);
            if (subIndex < subMacroblockMvd.size()) {
                subMacroblockMvd[subIndex] = pair;
                subMacroblockMvdValid[subIndex] = true;
            }
        }
    }

    result.ok = true;
    result.complete = true;
    return result;
}
