#pragma once

#include <QString>
#include <QVector>

struct H264CabacSyntaxResult
{
    bool ok = false;
    int value = 0;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacCoeffAbsLevelRemainingInput
{
    int prefixOneCount = 0;
    QVector<int> bins;
};

struct H264CabacMbTypeResult
{
    bool ok = false;
    bool complete = false;
    bool needsSubMacroblockTypes = false;
    int mbType = -1;
    int prefixBin = -1;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacCodedBlockPatternResult
{
    bool ok = false;
    bool complete = false;
    int codedBlockPattern = -1;
    int codedBlockPatternLuma = -1;
    int codedBlockPatternChroma = -1;
    int firstCtxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacMbQpDeltaResult
{
    bool ok = false;
    bool complete = false;
    int mbQpDelta = 0;
    int firstCtxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacResidualBlockResult
{
    bool ok = false;
    bool complete = false;
    int codedBlockFlag = -1;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacResidualLuma4x4Result
{
    bool ok = false;
    bool complete = false;
    QVector<int> blockIndices;
    QVector<int> codedBlockFlags;
    QVector<int> significantScanIndices;
    QVector<int> significantCoeffFlags;
    QVector<int> lastSignificantScanIndices;
    QVector<int> lastSignificantCoeffFlags;
    QVector<int> coeffReverseScanIndices;
    QVector<int> coeffAbsLevelBlockIndices;
    QVector<int> coeffAbsLevelScanIndices;
    QVector<int> coeffAbsLevelInferredFinalFlags;
    QVector<int> coeffAbsLevelPrefixFirstBins;
    QVector<int> coeffAbsLevelPrefixFirstCtxIndices;
    QVector<int> coeffAbsLevelPrefixNextBins;
    QVector<int> coeffAbsLevelPrefixNextCtxIndices;
    QVector<int> coeffAbsLevelPrefixTerminatedFlags;
    QVector<int> coeffAbsLevelPrefixOneCounts;
    QVector<int> coeffAbsLevelSuffixBins;
    QVector<int> coeffAbsLevelSuffixBinCounts;
    QVector<int> coeffAbsLevelRemainingInputBins;
    QVector<int> coeffAbsLevelRemainingInputBinCounts;
    QVector<int> coeffAbsLevelReadyForValueFlags;
    QVector<int> coeffAbsLevelReadyPrefixOneCounts;
    QVector<QVector<int>> coeffAbsLevelReadySuffixBins;
    QVector<QVector<int>> coeffAbsLevelReadyRemainingInputBins;
    QVector<int> coeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags;
    QVector<int> coeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts;
    QVector<int> coeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts;
    QVector<int> coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags;
    QVector<int> coeffAbsLevelValueInputCompleteFlags;
    QVector<int> coeffAbsLevelFixedInputRecognizedFlags;
    QVector<int> coeffAbsLevelPreUeg0RemainingInputFlags;
    QVector<int> coeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags;
    QVector<int> coeffSignFlags;
    QVector<int> coeffAbsLevelMinus1Values;
    QVector<int> coefficientLevels;
    int firstCtxIdx = -1;
    int incompleteBlockIndex = -1;
    int incompleteScanIndex = -1;
    QString incompleteStage;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacResidualChromaDcResult
{
    bool ok = false;
    bool complete = false;
    QVector<int> components;
    QVector<int> codedBlockFlags;
    QVector<int> significantComponentIndices;
    QVector<int> significantScanIndices;
    QVector<int> significantCoeffFlags;
    QVector<int> lastSignificantComponentIndices;
    QVector<int> lastSignificantScanIndices;
    QVector<int> lastSignificantCoeffFlags;
    QVector<int> coeffComponentIndices;
    QVector<int> coeffScanIndices;
    QVector<int> coeffAbsLevelPrefixFirstCtxIndices;
    QVector<int> coeffAbsLevelPrefixNextCtxIndices;
    QVector<int> coeffAbsLevelMinus1Values;
    QVector<int> coeffSignFlags;
    QVector<int> coefficientLevels;
    int firstCtxIdx = -1;
    int incompleteComponent = -1;
    QString incompleteStage;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacSubMbTypeResult
{
    bool ok = false;
    bool complete = false;
    int subMbType = -1;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacSubMbTypesResult
{
    bool ok = false;
    bool complete = false;
    QVector<int> subMbTypes;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacRefIdxResult
{
    bool ok = false;
    bool present = false;
    int refIdx = 0;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacRefIdxListResult
{
    bool ok = false;
    bool present = false;
    QVector<int> refIdx;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacMvdResult
{
    bool ok = false;
    bool complete = false;
    int value = 0;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacMvdPair
{
    int x = 0;
    int y = 0;
};

struct H264CabacMvdListResult
{
    bool ok = false;
    bool complete = false;
    QVector<H264CabacMvdPair> mvd;
    QString diagnosticCode;
    QString diagnosticMessage;
};
