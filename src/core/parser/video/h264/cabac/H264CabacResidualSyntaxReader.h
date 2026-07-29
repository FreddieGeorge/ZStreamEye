#pragma once

#include "core/parser/video/h264/cabac/H264CabacContextModel.h"
#include "core/parser/video/h264/cabac/H264CabacSyntaxTypes.h"

class BitReader;
class H264CabacDecoder;

enum class H264CabacResidualBlockCategory
{
    Luma4x4,
    ChromaDc
};

int h264CabacCoeffAbsLevelMinus1Ueg0Cutoff();
bool h264CabacCoeffAbsLevelMinus1UsesUeg0Suffix(int prefixOneCount);
bool h264CabacCoeffAbsLevelMinus1IsPreUeg0RemainingInput(int prefixOneCount);
bool h264CabacCoeffAbsLevelMinus1HasPreUeg0RemainingInput(
    const H264CabacCoeffAbsLevelRemainingInput &input);
bool h264CabacCoeffAbsLevelMinus1NeedsAdditionalPreUeg0Parsing(
    const H264CabacCoeffAbsLevelRemainingInput &input);
int h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingTargetPrefixOneCount(
    const H264CabacCoeffAbsLevelRemainingInput &input);
int h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingRemainingPrefixBins(
    const H264CabacCoeffAbsLevelRemainingInput &input);
bool h264CabacCoeffAbsLevelMinus1CanContinuePreUeg0PrefixParsing(
    const H264CabacCoeffAbsLevelRemainingInput &input);
bool h264CabacCoeffAbsLevelMinus1CanComputeFromUeg0Suffix(
    const H264CabacCoeffAbsLevelRemainingInput &input);
bool h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue(
    const H264CabacCoeffAbsLevelRemainingInput &input,
    int *suffixValue);
bool h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix(
    const H264CabacCoeffAbsLevelRemainingInput &input,
    int *coeffAbsLevelMinus1Value);

H264CabacResidualBlockResult h264ReadCabacResidualCodedBlockFlagZero(
    BitReader &reader,
    H264CabacDecoder &decoder,
    H264CabacContextModelSet &contexts,
    H264CabacResidualBlockCategory category);

H264CabacResidualLuma4x4Result h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(
    BitReader &reader,
    H264CabacDecoder &decoder,
    H264CabacContextModelSet &contexts,
    int codedBlockPatternLuma);

H264CabacResidualChromaDcResult h264ReadCabacResidualChromaDcCodedBlockFlagsZero(
    BitReader &reader,
    H264CabacDecoder &decoder,
    H264CabacContextModelSet &contexts,
    int chromaArrayType,
    int codedBlockPatternChroma);
