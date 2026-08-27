#include "core/parser/video/h264/cabac/H264CabacDecoder.h"
#include "core/parser/video/h264/cabac/H264CabacMacroblockParser.h"
#include "core/parser/video/h264/cabac/H264CabacSyntaxReader.h"
#include "core/parser/video/h264/H264SliceDataContext.h"

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

void initializeBasicSlice(SliceInfo &slice, int sliceType, int firstMbInSlice)
{
    slice.sliceType = sliceType;
    slice.firstMbInSlice = firstMbInSlice;
    slice.picWidthInMbs = 2;
    slice.picHeightInMbs = 2;
    slice.derivedQp = 26;
    slice.cabacInitIdc = 0;
}

void initializeBasicSps(SpsInfo &sps)
{
    sps.frameMbsOnlyFlag = true;
    sps.chromaFormatIdc = 1;
}

void testCoeffAbsLevelMinus1Ueg0CutoffHelper()
{
    require(h264CabacCoeffAbsLevelMinus1Ueg0Cutoff() == 14,
            "CABAC coeff_abs_level_minus1 UEG0 cutoff");
    require(!h264CabacCoeffAbsLevelMinus1UsesUeg0Suffix(-1),
            "CABAC coeff_abs_level_minus1 UEG0 negative prefix does not use suffix");
    require(!h264CabacCoeffAbsLevelMinus1IsPreUeg0RemainingInput(-1),
            "CABAC coeff_abs_level_minus1 negative prefix is not pre-UEG0 remaining input");
    require(!h264CabacCoeffAbsLevelMinus1UsesUeg0Suffix(4),
            "CABAC coeff_abs_level_minus1 UEG0 current narrow input does not use suffix");
    require(h264CabacCoeffAbsLevelMinus1IsPreUeg0RemainingInput(4),
            "CABAC coeff_abs_level_minus1 current narrow input is pre-UEG0 remaining input");
    require(!h264CabacCoeffAbsLevelMinus1UsesUeg0Suffix(13),
            "CABAC coeff_abs_level_minus1 UEG0 prefix before cutoff does not use suffix");
    require(h264CabacCoeffAbsLevelMinus1IsPreUeg0RemainingInput(13),
            "CABAC coeff_abs_level_minus1 prefix before cutoff is pre-UEG0 remaining input");
    require(h264CabacCoeffAbsLevelMinus1UsesUeg0Suffix(14),
            "CABAC coeff_abs_level_minus1 UEG0 cutoff uses suffix");
    require(!h264CabacCoeffAbsLevelMinus1IsPreUeg0RemainingInput(14),
            "CABAC coeff_abs_level_minus1 cutoff is not pre-UEG0 remaining input");
    require(h264CabacCoeffAbsLevelMinus1UsesUeg0Suffix(15),
            "CABAC coeff_abs_level_minus1 UEG0 prefix after cutoff uses suffix");
    require(!h264CabacCoeffAbsLevelMinus1IsPreUeg0RemainingInput(15),
            "CABAC coeff_abs_level_minus1 prefix after cutoff is not pre-UEG0 remaining input");
}

void testCoeffAbsLevelMinus1PreUeg0RemainingInputHelper()
{
    require(h264CabacCoeffAbsLevelMinus1HasPreUeg0RemainingInput({4, {0, 0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 current narrow input has pre-UEG0 remaining input");
    require(h264CabacCoeffAbsLevelMinus1HasPreUeg0RemainingInput({13, {0, 0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 prefix before cutoff has pre-UEG0 remaining input");
    require(h264CabacCoeffAbsLevelMinus1NeedsAdditionalPreUeg0Parsing({4, {0, 0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 current narrow input needs additional pre-UEG0 parsing");
    require(h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingTargetPrefixOneCount(
                {4, {0, 0, 0, 0}}) == h264CabacCoeffAbsLevelMinus1Ueg0Cutoff(),
            "CABAC coeff_abs_level_minus1 current narrow input targets UEG0 cutoff");
    require(h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingRemainingPrefixBins(
                {4, {0, 0, 0, 0}}) == 10,
            "CABAC coeff_abs_level_minus1 current narrow input remaining pre-UEG0 prefix bins");
    require(h264CabacCoeffAbsLevelMinus1CanContinuePreUeg0PrefixParsing({4, {0, 0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 current narrow input can continue pre-UEG0 prefix parsing");
    require(h264CabacCoeffAbsLevelMinus1NeedsAdditionalPreUeg0Parsing({13, {0, 0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 prefix before cutoff needs additional pre-UEG0 parsing");
    require(h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingTargetPrefixOneCount(
                {13, {0, 0, 0, 0}}) == h264CabacCoeffAbsLevelMinus1Ueg0Cutoff(),
            "CABAC coeff_abs_level_minus1 prefix before cutoff targets UEG0 cutoff");
    require(h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingRemainingPrefixBins(
                {13, {0, 0, 0, 0}}) == 1,
            "CABAC coeff_abs_level_minus1 prefix before cutoff remaining pre-UEG0 prefix bins");
    require(h264CabacCoeffAbsLevelMinus1CanContinuePreUeg0PrefixParsing({13, {0, 0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 prefix before cutoff can continue pre-UEG0 prefix parsing");
    require(!h264CabacCoeffAbsLevelMinus1HasPreUeg0RemainingInput({14, {0, 0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 cutoff does not have pre-UEG0 remaining input");
    require(!h264CabacCoeffAbsLevelMinus1NeedsAdditionalPreUeg0Parsing({14, {0, 0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 cutoff does not need pre-UEG0 parsing");
    require(h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingTargetPrefixOneCount(
                {14, {0, 0, 0, 0}}) == -1,
            "CABAC coeff_abs_level_minus1 cutoff has no pre-UEG0 target");
    require(h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingRemainingPrefixBins(
                {14, {0, 0, 0, 0}}) == -1,
            "CABAC coeff_abs_level_minus1 cutoff has no remaining pre-UEG0 prefix bins");
    require(!h264CabacCoeffAbsLevelMinus1CanContinuePreUeg0PrefixParsing({14, {0, 0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 cutoff cannot continue pre-UEG0 prefix parsing");
    require(!h264CabacCoeffAbsLevelMinus1CanComputeFromUeg0Suffix({4, {0, 0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 current narrow input cannot compute from UEG0 suffix");
    require(!h264CabacCoeffAbsLevelMinus1CanComputeFromUeg0Suffix({13, {0, 0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 pre-UEG0 input cannot compute from UEG0 suffix");
    require(!h264CabacCoeffAbsLevelMinus1CanComputeFromUeg0Suffix({14, {0, 0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 rejects a non-codeword UEG0 input shape");
    require(h264CabacCoeffAbsLevelMinus1CanComputeFromUeg0Suffix({14, {0}}),
            "CABAC coeff_abs_level_minus1 cutoff input accepts zero UEG0 suffix codeword");
    require(h264CabacCoeffAbsLevelMinus1CanComputeFromUeg0Suffix({14, {1, 0, 0}}),
            "CABAC coeff_abs_level_minus1 cutoff input accepts UEG0 value-one codeword");
    require(!h264CabacCoeffAbsLevelMinus1CanComputeFromUeg0Suffix({14, {0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 cutoff short input cannot compute from UEG0 suffix");
    require(!h264CabacCoeffAbsLevelMinus1HasPreUeg0RemainingInput({4, {}}),
            "CABAC coeff_abs_level_minus1 empty bins are not complete pre-UEG0 remaining input");
    require(!h264CabacCoeffAbsLevelMinus1NeedsAdditionalPreUeg0Parsing({4, {}}),
            "CABAC coeff_abs_level_minus1 empty bins do not trigger additional pre-UEG0 parsing");
    require(h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingTargetPrefixOneCount({4, {}}) == -1,
            "CABAC coeff_abs_level_minus1 empty bins have no pre-UEG0 target");
    require(h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingRemainingPrefixBins({4, {}}) == -1,
            "CABAC coeff_abs_level_minus1 empty bins have no remaining pre-UEG0 prefix bins");
    require(!h264CabacCoeffAbsLevelMinus1CanContinuePreUeg0PrefixParsing({4, {}}),
            "CABAC coeff_abs_level_minus1 empty bins cannot continue pre-UEG0 prefix parsing");
    require(!h264CabacCoeffAbsLevelMinus1HasPreUeg0RemainingInput({4, {0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 short bins are not complete pre-UEG0 remaining input");
    require(!h264CabacCoeffAbsLevelMinus1NeedsAdditionalPreUeg0Parsing({4, {0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 short bins do not trigger additional pre-UEG0 parsing");
    require(h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingTargetPrefixOneCount({4, {0, 0, 0}}) == -1,
            "CABAC coeff_abs_level_minus1 short bins have no pre-UEG0 target");
    require(h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingRemainingPrefixBins({4, {0, 0, 0}}) == -1,
            "CABAC coeff_abs_level_minus1 short bins have no remaining pre-UEG0 prefix bins");
    require(!h264CabacCoeffAbsLevelMinus1CanContinuePreUeg0PrefixParsing({4, {0, 0, 0}}),
            "CABAC coeff_abs_level_minus1 short bins cannot continue pre-UEG0 prefix parsing");
}

void testCoeffAbsLevelMinus1Ueg0SuffixValueHelper()
{
    int suffixValue = -1;
    require(!h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue({4, {0, 0, 0, 0}}, &suffixValue),
            "CABAC coeff_abs_level_minus1 pre-UEG0 input has no UEG0 suffix value");
    require(suffixValue == -1,
            "CABAC coeff_abs_level_minus1 pre-UEG0 suffix value output remains unchanged");
    require(!h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue({14, {0, 0, 0, 0}}, nullptr),
            "CABAC coeff_abs_level_minus1 suffix value rejects null output");
    require(!h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue({14, {0, 0, 0, 0}}, &suffixValue),
            "CABAC coeff_abs_level_minus1 rejects non-codeword suffix input");
    require(suffixValue == -1,
            "CABAC coeff_abs_level_minus1 rejected suffix leaves output unchanged");
    require(h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue({14, {0}}, &suffixValue),
            "CABAC coeff_abs_level_minus1 zero UEG0 suffix codeword result");
    require(suffixValue == 0, "CABAC coeff_abs_level_minus1 zero UEG0 suffix codeword value");
    require(h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue({14, {1, 0, 1}}, &suffixValue),
            "CABAC coeff_abs_level_minus1 UEG0 value-two result");
    require(suffixValue == 2, "CABAC coeff_abs_level_minus1 UEG0 value-two suffix");
    require(h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue({14, {1, 1, 0, 0, 0}}, &suffixValue),
            "CABAC coeff_abs_level_minus1 UEG0 value-three result");
    require(suffixValue == 3, "CABAC coeff_abs_level_minus1 UEG0 value-three suffix");
    require(!h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue({14, {1, 0, 2}}, &suffixValue),
            "CABAC coeff_abs_level_minus1 suffix value rejects non-binary bin");
    require(suffixValue == 3,
            "CABAC coeff_abs_level_minus1 non-binary suffix value output remains unchanged");
}

void testCoeffAbsLevelMinus1Ueg0SuffixComputeHelper()
{
    int coeffAbsLevelMinus1Value = -1;
    require(!h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix({4, {0, 0, 0, 0}},
                                                               &coeffAbsLevelMinus1Value),
            "CABAC coeff_abs_level_minus1 pre-UEG0 input does not compute coeff_abs_level_minus1");
    require(coeffAbsLevelMinus1Value == -1,
            "CABAC coeff_abs_level_minus1 pre-UEG0 compute output remains unchanged");
    require(!h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix({14, {0, 0, 0}},
                                                               &coeffAbsLevelMinus1Value),
            "CABAC coeff_abs_level_minus1 short UEG0 input does not compute coeff_abs_level_minus1");
    require(coeffAbsLevelMinus1Value == -1,
            "CABAC coeff_abs_level_minus1 short UEG0 compute output remains unchanged");
    require(!h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix({14, {0, 0, 2, 0}},
                                                               &coeffAbsLevelMinus1Value),
            "CABAC coeff_abs_level_minus1 non-binary UEG0 input does not compute coeff_abs_level_minus1");
    require(coeffAbsLevelMinus1Value == -1,
            "CABAC coeff_abs_level_minus1 non-binary UEG0 compute output remains unchanged");
    require(!h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix({14, {0, 0, 0, 0}}, nullptr),
            "CABAC coeff_abs_level_minus1 compute rejects null output");
    require(!h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix({14, {0, 0, 0, 0}},
                                                               &coeffAbsLevelMinus1Value),
            "CABAC coeff_abs_level_minus1 non-codeword UEG0 input is rejected");
    require(coeffAbsLevelMinus1Value == -1,
            "CABAC coeff_abs_level_minus1 rejected codeword leaves output unchanged");
    require(h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix({14, {0}},
                                                              &coeffAbsLevelMinus1Value),
            "CABAC coeff_abs_level_minus1 zero UEG0 suffix codeword computes value");
    require(coeffAbsLevelMinus1Value == 14,
            "CABAC coeff_abs_level_minus1 zero UEG0 suffix codeword value");
    require(h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix({14, {1, 0, 1}},
                                                              &coeffAbsLevelMinus1Value),
            "CABAC coeff_abs_level_minus1 UEG0 value-two input computes coeff_abs_level_minus1");
    require(coeffAbsLevelMinus1Value == 16, "CABAC coeff_abs_level_minus1 UEG0 value-two result");
}

H264CabacDecoder initializedDecoder(BitReader &reader)
{
    H264CabacDecoder decoder;
    require(decoder.initialize(reader), "CABAC syntax reader decoder initialization");
    return decoder;
}

void setCbpZeroContexts(H264CabacContextModelSet &contexts)
{
    for (int ctxIdx = 73; ctxIdx <= 77; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
}

void setLuma4x4SignificantZeroContexts(H264CabacContextModelSet &contexts)
{
    for (int ctxIdx = 134; ctxIdx <= 148; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
}

void initializeP8x8LumaResidualSlice(SliceInfo &slice, SpsInfo &sps)
{
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    sps.chromaFormatIdc = 0;
    slice.numRefIdxL0ActiveMinus1 = 0;
}

H264CabacContextModelSet initializedP8x8LumaResidualCoeffLevelContexts(int currentQp,
                                                                       int firstPrefixBin,
                                                                       int nextPrefixBin,
                                                                       int thirdPrefixBin = -1)
{
    const int maxCtxIdx = thirdPrefixBin >= 0 ? 253 : 252;
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, currentQp, maxCtxIdx);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(73, {0, 0});
    contexts.setModel(74, {0, 0});
    contexts.setModel(75, {0, 0});
    contexts.setModel(76, {0, 1});
    contexts.setModel(85, {0, 1});
    contexts.setModel(134, {0, 1});
    contexts.setModel(166, {0, 1});
    contexts.setModel(248, {0, firstPrefixBin});
    contexts.setModel(252, {0, nextPrefixBin});
    if (thirdPrefixBin >= 0) {
        contexts.setModel(253, {0, thirdPrefixBin});
    }
    return contexts;
}

H264CabacContextModelSet initializedP8x8LumaResidualCoeffLevelContextsWithFourthContext(
    int currentQp,
    int firstPrefixBin,
    int nextPrefixBin,
    int thirdPrefixBin,
    int fourthPrefixBin)
{
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, currentQp, 254);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(73, {0, 0});
    contexts.setModel(74, {0, 0});
    contexts.setModel(75, {0, 0});
    contexts.setModel(76, {0, 1});
    contexts.setModel(85, {0, 1});
    contexts.setModel(134, {0, 1});
    contexts.setModel(166, {0, 1});
    contexts.setModel(248, {0, firstPrefixBin});
    contexts.setModel(252, {0, nextPrefixBin});
    contexts.setModel(253, {0, thirdPrefixBin});
    contexts.setModel(254, {0, fourthPrefixBin});
    return contexts;
}

H264CabacContextModelSet initializedP8x8LumaResidualCoeffLevelContextsWithFifthContext(
    int currentQp,
    int firstPrefixBin,
    int nextPrefixBin,
    int thirdPrefixBin,
    int fourthPrefixBin,
    int fifthPrefixBin)
{
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, currentQp, 256);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(73, {0, 0});
    contexts.setModel(74, {0, 0});
    contexts.setModel(75, {0, 0});
    contexts.setModel(76, {0, 1});
    contexts.setModel(85, {0, 1});
    contexts.setModel(134, {0, 1});
    contexts.setModel(166, {0, 1});
    contexts.setModel(248, {0, firstPrefixBin});
    contexts.setModel(252, {0, nextPrefixBin});
    contexts.setModel(253, {0, thirdPrefixBin});
    contexts.setModel(254, {0, fourthPrefixBin});
    contexts.setModel(255, {0, fifthPrefixBin});
    return contexts;
}

H264CabacContextModelSet initializedLuma4x4CoeffLevelContexts(int firstPrefixBin,
                                                              int nextPrefixBin,
                                                              int thirdPrefixBin = -1)
{
    const int maxCtxIdx = thirdPrefixBin >= 0 ? 253 : 252;
    H264CabacContextModelSet contexts(maxCtxIdx + 1);
    contexts.setModel(85, {0, 1});
    contexts.setModel(134, {0, 1});
    contexts.setModel(166, {0, 1});
    contexts.setModel(248, {0, firstPrefixBin});
    contexts.setModel(252, {0, nextPrefixBin});
    if (thirdPrefixBin >= 0) {
        contexts.setModel(253, {0, thirdPrefixBin});
    }
    return contexts;
}

H264CabacContextModelSet initializedLuma4x4CoeffLevelContextsWithFourthContext(int firstPrefixBin,
                                                                               int nextPrefixBin,
                                                                               int thirdPrefixBin,
                                                                               int fourthPrefixBin)
{
    H264CabacContextModelSet contexts(255);
    contexts.setModel(85, {0, 1});
    contexts.setModel(134, {0, 1});
    contexts.setModel(166, {0, 1});
    contexts.setModel(248, {0, firstPrefixBin});
    contexts.setModel(252, {0, nextPrefixBin});
    contexts.setModel(253, {0, thirdPrefixBin});
    contexts.setModel(254, {0, fourthPrefixBin});
    return contexts;
}

H264CabacContextModelSet initializedLuma4x4CoeffLevelContextsWithFifthContext(int firstPrefixBin,
                                                                              int nextPrefixBin,
                                                                              int thirdPrefixBin,
                                                                              int fourthPrefixBin,
                                                                              int fifthPrefixBin)
{
    H264CabacContextModelSet contexts(256);
    contexts.setModel(85, {0, 1});
    contexts.setModel(134, {0, 1});
    contexts.setModel(166, {0, 1});
    contexts.setModel(248, {0, firstPrefixBin});
    contexts.setModel(252, {0, nextPrefixBin});
    contexts.setModel(253, {0, thirdPrefixBin});
    contexts.setModel(254, {0, fourthPrefixBin});
    contexts.setModel(255, {0, fifthPrefixBin});
    return contexts;
}

void testReadPSliceMbSkipFlag()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 23);

    const H264CabacSyntaxResult result = h264ReadCabacMbSkipFlag(reader, decoder, contexts, context);
    require(result.ok, "CABAC P mb_skip_flag result");
    require(result.value == 0 || result.value == 1, "CABAC P mb_skip_flag bin value is binary");
    require(result.ctxIdx == 11, "CABAC P mb_skip_flag first macroblock ctxIdx");
    require(contexts.model(11).stateIndex !=
                H264CabacContextModelInitializer::initializeSliceContexts(false, 0, 26, 23).model(11).stateIndex,
            "CABAC P mb_skip_flag updates context state");
}

void testReadPSliceMbSkipFlagUsesLeftNeighbor()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 1);
    initializeBasicSps(sps);
    MacroblockInfo left;
    left.address = 0;
    left.skipped = false;
    slice.macroblocks.append(left);

    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 23);

    const H264CabacSyntaxResult result = h264ReadCabacMbSkipFlag(reader, decoder, contexts, context);
    require(result.ok, "CABAC P mb_skip_flag with left neighbor");
    require(result.ctxIdx == 12, "CABAC P mb_skip_flag left neighbor ctxIdx increment");
}

void testReadPSliceMbTypeP16x16()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 23);

    const H264CabacMbTypeResult result = h264ReadCabacMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P mb_type P_L0_16x16 result");
    require(result.complete, "CABAC P mb_type P_L0_16x16 is complete");
    require(result.mbType == 0, "CABAC P mb_type P_L0_16x16 value");
    require(result.prefixBin == 0, "CABAC P mb_type P_L0_16x16 prefix bin");
    require(result.ctxIdx == 14, "CABAC P mb_type P_L0_16x16 ctxIdx");
}

void testReadPSliceMbTypeP16x8()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(18);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 1});
    contexts.setModel(17, {0, 1});

    const H264CabacMbTypeResult result = h264ReadCabacMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P mb_type P_L0_L0_16x8 result");
    require(result.complete, "CABAC P mb_type P_L0_L0_16x8 is complete");
    require(result.mbType == 1, "CABAC P mb_type P_L0_L0_16x8 value");
    require(result.prefixBin == 0, "CABAC P mb_type P_L0_L0_16x8 prefix bin");
    require(result.ctxIdx == 14, "CABAC P mb_type P_L0_L0_16x8 ctxIdx");
}

void testReadPSliceMbTypeP8x16()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(18);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 1});
    contexts.setModel(17, {0, 0});

    const H264CabacMbTypeResult result = h264ReadCabacMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P mb_type P_L0_L0_8x16 result");
    require(result.complete, "CABAC P mb_type P_L0_L0_8x16 is complete");
    require(result.mbType == 2, "CABAC P mb_type P_L0_L0_8x16 value");
    require(result.prefixBin == 0, "CABAC P mb_type P_L0_L0_8x16 prefix bin");
    require(result.ctxIdx == 14, "CABAC P mb_type P_L0_L0_8x16 ctxIdx");
}

void testReadPSliceMbTypeP8x8RequiresSubMbType()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(17);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});

    const H264CabacMbTypeResult result = h264ReadCabacMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P mb_type P_8x8 branch result");
    require(result.complete, "CABAC P mb_type P_8x8 branch completes mb_type");
    require(result.needsSubMacroblockTypes, "CABAC P mb_type P_8x8 requires sub_mb_type");
    require(result.mbType == 3, "CABAC P mb_type P_8x8 value");
    require(result.prefixBin == 0, "CABAC P mb_type P_8x8 prefix bin");
    require(result.ctxIdx == 14, "CABAC P mb_type P_8x8 ctxIdx");
}

void testReadPSubMbTypeP8x8()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(22);
    contexts.setModel(21, {0, 1});

    const H264CabacSubMbTypeResult result = h264ReadCabacPSubMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P sub_mb_type P_L0_8x8 result");
    require(result.complete, "CABAC P sub_mb_type P_L0_8x8 is complete");
    require(result.subMbType == 0, "CABAC P sub_mb_type P_L0_8x8 value");
    require(result.ctxIdx == 21, "CABAC P sub_mb_type P_L0_8x8 ctxIdx");
}

void testReadPSubMbTypeReportsMissingContext()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(22);
    contexts.setModel(21, {0, 0});

    const H264CabacSubMbTypeResult result = h264ReadCabacPSubMbType(reader, decoder, contexts, context);
    require(!result.ok, "CABAC P sub_mb_type missing context fails cleanly");
    require(!result.complete, "CABAC P sub_mb_type missing context remains incomplete");
    require(result.ctxIdx == 22, "CABAC P sub_mb_type missing context ctxIdx");
    require(result.diagnosticCode == QStringLiteral("cabac_context_uninitialized"),
            "CABAC P sub_mb_type missing context diagnostic");
}

void testReadPSubMbTypeP8x4()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(24);
    contexts.setModel(21, {0, 0});
    contexts.setModel(22, {0, 0});

    const H264CabacSubMbTypeResult result = h264ReadCabacPSubMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P sub_mb_type P_L0_8x4 result");
    require(result.complete, "CABAC P sub_mb_type P_L0_8x4 is complete");
    require(result.subMbType == 1, "CABAC P sub_mb_type P_L0_8x4 value");
    require(result.ctxIdx == 21, "CABAC P sub_mb_type P_L0_8x4 ctxIdx");
}

void testReadPSubMbTypeP4x8()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(24);
    contexts.setModel(21, {0, 0});
    contexts.setModel(22, {0, 1});
    contexts.setModel(23, {0, 1});

    const H264CabacSubMbTypeResult result = h264ReadCabacPSubMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P sub_mb_type P_L0_4x8 result");
    require(result.complete, "CABAC P sub_mb_type P_L0_4x8 is complete");
    require(result.subMbType == 2, "CABAC P sub_mb_type P_L0_4x8 value");
    require(result.ctxIdx == 21, "CABAC P sub_mb_type P_L0_4x8 ctxIdx");
}

void testReadPSubMbTypeP4x4()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(24);
    contexts.setModel(21, {0, 0});
    contexts.setModel(22, {0, 1});
    contexts.setModel(23, {0, 0});

    const H264CabacSubMbTypeResult result = h264ReadCabacPSubMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC P sub_mb_type P_L0_4x4 result");
    require(result.complete, "CABAC P sub_mb_type P_L0_4x4 is complete");
    require(result.subMbType == 3, "CABAC P sub_mb_type P_L0_4x4 value");
    require(result.ctxIdx == 21, "CABAC P sub_mb_type P_L0_4x4 ctxIdx");
}

void testReadFourPSubMbTypes()
{
    BitReader reader(QByteArray::fromHex("0000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(24);
    contexts.setModel(21, {0, 1});
    contexts.setModel(22, {0, 0});
    contexts.setModel(23, {0, 0});

    const H264CabacSubMbTypesResult result =
        h264ReadCabacPSubMbTypes(reader, decoder, contexts, context, 4);
    require(result.ok, "CABAC P reads four sub_mb_type values");
    require(result.complete, "CABAC P four sub_mb_type values complete");
    require(result.subMbTypes.size() == 4, "CABAC P four sub_mb_type count");
    require(result.subMbTypes[0] == 0, "CABAC P sub_mb_type[0]");
    require(result.subMbTypes[1] == 0, "CABAC P sub_mb_type[1]");
    require(result.subMbTypes[2] == 0, "CABAC P sub_mb_type[2]");
    require(result.subMbTypes[3] == 0, "CABAC P sub_mb_type[3]");
}

void testReadFourPSubMbTypesReportsIndexedFailure()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(22);
    contexts.setModel(21, {0, 0});

    const H264CabacSubMbTypesResult result =
        h264ReadCabacPSubMbTypes(reader, decoder, contexts, context, 4);
    require(!result.ok, "CABAC P four sub_mb_type reports indexed failure");
    require(!result.complete, "CABAC P four sub_mb_type indexed failure remains incomplete");
    require(result.diagnosticCode == QStringLiteral("cabac_context_uninitialized"),
            "CABAC P four sub_mb_type indexed failure diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("sub_mb_type[0]")),
            "CABAC P four sub_mb_type indexed failure message");
}

void testReadPSubMbRefIdxNotPresent()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 59);

    const H264CabacRefIdxListResult result =
        h264ReadCabacPSubMbRefIdxL0(reader, decoder, contexts, context, 4);
    require(result.ok, "CABAC P sub_mb ref_idx not present result");
    require(!result.present, "CABAC P sub_mb ref_idx not present flag");
    require(result.refIdx.size() == 4, "CABAC P sub_mb ref_idx not present count");
    require(result.refIdx[0] == 0 && result.refIdx[3] == 0,
            "CABAC P sub_mb ref_idx not present defaults to zero");
}

void testReadPSubMbRefIdxZero()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 1;
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 59);

    const H264CabacRefIdxListResult result =
        h264ReadCabacPSubMbRefIdxL0(reader, decoder, contexts, context, 4);
    require(result.ok, "CABAC P sub_mb ref_idx zero result");
    require(result.present, "CABAC P sub_mb ref_idx present flag");
    require(result.refIdx.size() == 4, "CABAC P sub_mb ref_idx zero count");
    require(result.refIdx[0] == 0 && result.refIdx[1] == 0 && result.refIdx[2] == 0 && result.refIdx[3] == 0,
            "CABAC P sub_mb ref_idx zero values");
}

void testReadPSubMbRefIdxIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 1;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(54, {0, 1});

    const H264CabacRefIdxListResult result =
        h264ReadCabacPSubMbRefIdxL0(reader, decoder, contexts, context, 4);
    require(result.ok, "CABAC P sub_mb ref_idx incomplete result");
    require(!result.refIdx.size(), "CABAC P sub_mb ref_idx incomplete stops before append");
    require(result.diagnosticCode == QStringLiteral("cabac_ref_idx_incomplete"),
            "CABAC P sub_mb ref_idx incomplete diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("ref_idx_l0[0]")),
            "CABAC P sub_mb ref_idx incomplete indexed message");
}

void testReadMvdComponentZero()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(40, {0, 0});

    const H264CabacMvdResult result = h264ReadCabacMvdL0Component(reader, decoder, contexts, 0);
    require(result.ok, "CABAC mvd_l0 component zero result");
    require(result.complete, "CABAC mvd_l0 component zero complete");
    require(result.value == 0, "CABAC mvd_l0 component zero value");
    require(result.ctxIdx == 40, "CABAC mvd_l0 x context");
}

void testReadMvdComponentAbs2Positive()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(41, {0, 1});
    contexts.setModel(43, {0, 1});
    contexts.setModel(44, {1, 0});

    const H264CabacMvdResult result = h264ReadCabacMvdL0Component(reader, decoder, contexts, 0, 1);
    require(result.ok, "CABAC mvd_l0 component abs2 positive result");
    require(result.complete, "CABAC mvd_l0 component abs2 positive complete");
    require(result.value == 2, "CABAC mvd_l0 component abs2 positive value");
    require(result.ctxIdx == 41, "CABAC mvd_l0 abs2 positive derived context");
}

void testReadMvdComponentAbs3Positive()
{
    BitReader reader(QByteArray::fromHex("270000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(41, {0, 1});
    contexts.setModel(43, {0, 1});
    contexts.setModel(44, {1, 0});

    const H264CabacMvdResult result = h264ReadCabacMvdL0Component(reader, decoder, contexts, 0, 1);
    require(result.ok, "CABAC mvd_l0 component abs3 positive result");
    require(result.complete, "CABAC mvd_l0 component abs3 positive complete");
    require(result.value == 3, "CABAC mvd_l0 component abs3 positive value");
    require(result.ctxIdx == 41, "CABAC mvd_l0 abs3 positive derived context");
}

void testReadMvdComponentGreaterThanThreeIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(47, {0, 1});
    contexts.setModel(50, {0, 1});
    contexts.setModel(51, {0, 1});

    const H264CabacMvdResult result = h264ReadCabacMvdL0Component(reader, decoder, contexts, 1);
    require(result.ok, "CABAC mvd_l0 component greater-than-three prefix result");
    require(!result.complete, "CABAC mvd_l0 component greater-than-three incomplete");
    require(result.ctxIdx == 47, "CABAC mvd_l0 y context");
    require(result.diagnosticCode == QStringLiteral("cabac_mvd_incomplete"),
            "CABAC mvd_l0 component greater-than-three diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("greater than three")),
            "CABAC mvd_l0 component greater-than-three message");
}

void testReadPSubMbMvdZero8x8()
{
    BitReader reader(QByteArray::fromHex("00000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});

    const H264CabacMvdListResult result =
        h264ReadCabacPSubMbMvdL0(reader, decoder, contexts, QVector<int> {0, 0, 0, 0});
    require(result.ok, "CABAC P sub_mb mvd zero 8x8 result");
    require(result.complete, "CABAC P sub_mb mvd zero 8x8 complete");
    require(result.mvd.size() == 4, "CABAC P sub_mb mvd zero 8x8 pair count");
    require(result.mvd[0].x == 0 && result.mvd[0].y == 0, "CABAC P sub_mb mvd zero first pair");
}

void testReadPSubMbMvdAbs2_8x8()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(40, {0, 1});
    contexts.setModel(43, {0, 1});
    contexts.setModel(44, {1, 0});
    contexts.setModel(47, {0, 0});

    const H264CabacMvdListResult result =
        h264ReadCabacPSubMbMvdL0(reader, decoder, contexts, QVector<int> {0});
    require(result.ok, "CABAC P sub_mb mvd abs2 8x8 result");
    require(result.complete, "CABAC P sub_mb mvd abs2 8x8 complete");
    require(result.mvd.size() == 1, "CABAC P sub_mb mvd abs2 8x8 pair count");
    require(result.mvd[0].x == 2 && result.mvd[0].y == 0,
            "CABAC P sub_mb mvd abs2 8x8 pair value");
}

void testReadPSubMbMvdZero4x4()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});

    const H264CabacMvdListResult result =
        h264ReadCabacPSubMbMvdL0(reader, decoder, contexts, QVector<int> {3, 3, 3, 3});
    require(result.ok, "CABAC P sub_mb mvd zero 4x4 result");
    require(result.complete, "CABAC P sub_mb mvd zero 4x4 complete");
    require(result.mvd.size() == 16, "CABAC P sub_mb mvd zero 4x4 pair count");
    require(result.mvd[15].x == 0 && result.mvd[15].y == 0, "CABAC P sub_mb mvd zero last pair");
}

void testReadCabacMacroblockSyntaxP8x8RefAbsent()
{
    BitReader reader(QByteArray::fromHex("000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(60, {0, 1});
    setCbpZeroContexts(contexts);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 ref absent result");
    require(result.complete, "CABAC macroblock syntax P_8x8 ref absent complete");
    require(result.parsedSubMacroblockSyntax, "CABAC macroblock syntax P_8x8 sub syntax flag");
    require(result.parsedCodedBlockPatternZero, "CABAC macroblock syntax P_8x8 CBP zero flag");
    require(result.mbType == 3, "CABAC macroblock syntax P_8x8 mb_type");
    require(result.subMbTypes.size() == 4, "CABAC macroblock syntax P_8x8 sub_mb_type count");
    require(!result.refIdxL0Present, "CABAC macroblock syntax P_8x8 ref_idx_l0 absent");
    require(result.refIdxL0.size() == 4, "CABAC macroblock syntax P_8x8 default ref_idx count");
    require(result.mvdL0.size() == 4, "CABAC macroblock syntax P_8x8 mvd pair count");
    require(result.codedBlockPattern == 0, "CABAC macroblock syntax P_8x8 CBP value");
    require(contexts.model(60).stateIndex == 0 && contexts.model(60).valueMps == 1,
            "CABAC macroblock syntax P_8x8 CBP-zero does not consume absent mb_qp_delta");
}

void testReadCabacMacroblockSyntaxP8x8RefZero()
{
    BitReader reader(QByteArray::fromHex("00000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 1;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(54, {0, 0});
    setCbpZeroContexts(contexts);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 ref zero result");
    require(result.complete, "CABAC macroblock syntax P_8x8 ref zero complete");
    require(result.parsedCodedBlockPatternZero, "CABAC macroblock syntax P_8x8 ref zero CBP zero flag");
    require(result.refIdxL0Present, "CABAC macroblock syntax P_8x8 ref_idx_l0 present");
    require(result.refIdxL0.size() == 4, "CABAC macroblock syntax P_8x8 ref_idx_l0 count");
    require(result.refIdxL0[0] == 0 && result.refIdxL0[3] == 0,
            "CABAC macroblock syntax P_8x8 ref_idx_l0 zero values");
    require(result.mvdL0.size() == 4, "CABAC macroblock syntax P_8x8 ref zero mvd pair count");
}

void testReadCabacMacroblockSyntaxP8x8ChromaAcResidualIncomplete()
{
    BitReader reader(QByteArray::fromHex("00000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(73, {0, 0});
    contexts.setModel(74, {0, 0});
    contexts.setModel(75, {0, 0});
    contexts.setModel(76, {0, 0});
    contexts.setModel(77, {0, 1});
    contexts.setModel(81, {0, 1});
    contexts.setModel(97, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 chroma AC residual prefix result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 chroma AC residual incomplete");
    require(result.parsedSubMacroblockSyntax, "CABAC macroblock syntax P_8x8 chroma AC residual keeps motion flag");
    require(result.parsedCodedBlockPattern, "CABAC macroblock syntax P_8x8 chroma AC residual keeps CBP");
    require(result.codedBlockPatternChroma == 2,
            "CABAC macroblock syntax P_8x8 chroma AC residual CBP value");
    require(result.residualChromaDcComponents.size() == 2,
            "CABAC macroblock syntax P_8x8 chroma AC residual preserves DC components");
    require(result.residualChromaDcCodedBlockFlags[0] == 0
                && result.residualChromaDcCodedBlockFlags[1] == 0,
            "CABAC macroblock syntax P_8x8 chroma AC residual preserves DC CBF values");
    require(result.residualIncompleteCategory == QStringLiteral("chroma_ac"),
            "CABAC macroblock syntax P_8x8 chroma AC residual incomplete category");
    require(result.residualIncompleteStage == QStringLiteral("coded_block_flag"),
            "CABAC macroblock syntax P_8x8 chroma AC residual incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 chroma AC residual diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("chroma AC")),
            "CABAC macroblock syntax P_8x8 chroma AC residual message");
}

void testReadCabacMacroblockSyntaxP8x8SingleLumaResidualCbfZero()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    sps.chromaFormatIdc = 0;
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(73, {0, 0});
    contexts.setModel(74, {0, 0});
    contexts.setModel(75, {0, 0});
    contexts.setModel(76, {0, 1});
    contexts.setModel(85, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 single-luma residual result");
    require(result.complete, "CABAC macroblock syntax P_8x8 single-luma residual complete");
    require(result.parsedCodedBlockPattern, "CABAC macroblock syntax P_8x8 single-luma CBP parsed");
    require(!result.parsedCodedBlockPatternZero,
            "CABAC macroblock syntax P_8x8 single-luma CBP non-zero flag");
    require(result.parsedResidualCodedBlockFlagsZero,
            "CABAC macroblock syntax P_8x8 single-luma residual CBF-zero flag");
    require(result.codedBlockPatternLuma == 8,
            "CABAC macroblock syntax P_8x8 single-luma CBP bit");
    require(result.mbQpDelta == 0, "CABAC macroblock syntax P_8x8 single-luma mb_qp_delta");
    require(result.residualLuma4x4BlockIndices.size() == 4,
            "CABAC macroblock syntax P_8x8 single-luma residual block count");
    require(result.residualLuma4x4BlockIndices[0] == 12
                && result.residualLuma4x4BlockIndices[3] == 15,
            "CABAC macroblock syntax P_8x8 single-luma residual block indices");
    require(result.residualCodedBlockFlags[0] == 0 && result.residualCodedBlockFlags[3] == 0,
            "CABAC macroblock syntax P_8x8 single-luma residual CBF values");
}

void testReadCabacMacroblockSyntaxP8x8MultiLumaResidualCbfZero()
{
    BitReader reader(QByteArray::fromHex("000000000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    sps.chromaFormatIdc = 0;
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(73, {0, 1});
    contexts.setModel(85, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 multi-luma residual result");
    require(result.complete, "CABAC macroblock syntax P_8x8 multi-luma residual complete");
    require(result.parsedCodedBlockPattern, "CABAC macroblock syntax P_8x8 multi-luma CBP parsed");
    require(result.parsedResidualCodedBlockFlagsZero,
            "CABAC macroblock syntax P_8x8 multi-luma residual CBF-zero flag");
    require(result.codedBlockPatternLuma == 15,
            "CABAC macroblock syntax P_8x8 multi-luma CBP bits");
    require(result.residualLuma4x4BlockIndices.size() == 16,
            "CABAC macroblock syntax P_8x8 multi-luma residual block count");
    require(result.residualLuma4x4BlockIndices[0] == 0
                && result.residualLuma4x4BlockIndices[15] == 15,
            "CABAC macroblock syntax P_8x8 multi-luma residual block indices");
    require(result.residualCodedBlockFlags[0] == 0 && result.residualCodedBlockFlags[15] == 0,
            "CABAC macroblock syntax P_8x8 multi-luma residual CBF values");
}

void testReadCabacMacroblockSyntaxP8x8ChromaDcResidualCbfZero()
{
    BitReader reader(QByteArray::fromHex("000000000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    for (int ctxIdx = 73; ctxIdx <= 76; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
    contexts.setModel(77, {0, 1});
    contexts.setModel(81, {0, 0});
    contexts.setModel(97, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 chroma DC residual result");
    require(result.complete, "CABAC macroblock syntax P_8x8 chroma DC residual complete");
    require(result.parsedCodedBlockPattern, "CABAC macroblock syntax P_8x8 chroma DC CBP parsed");
    require(result.parsedResidualCodedBlockFlagsZero,
            "CABAC macroblock syntax P_8x8 chroma DC residual CBF-zero flag");
    require(result.codedBlockPatternLuma == 0,
            "CABAC macroblock syntax P_8x8 chroma DC luma CBP zero");
    require(result.codedBlockPatternChroma == 1,
            "CABAC macroblock syntax P_8x8 chroma DC CBP value");
    require(result.codedBlockPattern == 16,
            "CABAC macroblock syntax P_8x8 chroma DC combined CBP value");
    require(result.residualChromaDcComponents.size() == 2,
            "CABAC macroblock syntax P_8x8 chroma DC residual component count");
    require(result.residualChromaDcComponents[0] == 0
                && result.residualChromaDcComponents[1] == 1,
            "CABAC macroblock syntax P_8x8 chroma DC residual components");
    require(result.residualChromaDcCodedBlockFlags[0] == 0
                && result.residualChromaDcCodedBlockFlags[1] == 0,
            "CABAC macroblock syntax P_8x8 chroma DC residual CBF values");
}

void testReadCabacMacroblockSyntaxP8x8ChromaDcResidualCbfNonZero()
{
    BitReader reader(QByteArray::fromHex("000000000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(267);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    for (int ctxIdx = 73; ctxIdx <= 76; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
    contexts.setModel(77, {0, 1});
    contexts.setModel(81, {0, 0});
    contexts.setModel(97, {0, 1});
    contexts.setModel(149, {0, 0});
    contexts.setModel(150, {0, 0});
    contexts.setModel(151, {0, 0});
    contexts.setModel(258, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok && result.complete,
            "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF complete");
    require(result.parsedCodedBlockPattern, "CABAC macroblock syntax P_8x8 chroma DC non-zero CBF keeps CBP");
    require(result.parsedResidual && !result.parsedResidualCodedBlockFlagsZero,
            "CABAC macroblock syntax marks non-zero chroma DC residual parsed");
    require(result.residualChromaDcComponents == QVector<int>({0, 1}),
            "CABAC macroblock syntax preserves chroma DC components");
    require(result.residualChromaDcCodedBlockFlags == QVector<int>({1, 1}),
            "CABAC macroblock syntax preserves non-zero chroma DC CBF values");
    require(result.residualChromaDcCoeffComponents == QVector<int>({0, 1}),
            "CABAC macroblock syntax propagates chroma DC coefficient components");
    require(result.residualChromaDcCoeffScanIndices == QVector<int>({3, 3}),
            "CABAC macroblock syntax propagates chroma DC scan indices");
    require(result.residualChromaDcCoefficientLevels == QVector<int>({1, 1}),
            "CABAC macroblock syntax propagates chroma DC levels");
}

void testReadCabacMacroblockSyntaxP8x8ResidualCbfNonZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    sps.chromaFormatIdc = 0;
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(249);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(73, {0, 0});
    contexts.setModel(74, {0, 0});
    contexts.setModel(75, {0, 0});
    contexts.setModel(76, {0, 1});
    contexts.setModel(85, {0, 1});
    setLuma4x4SignificantZeroContexts(contexts);
    contexts.setModel(248, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 residual non-zero CBF prefix result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 residual non-zero CBF incomplete");
    require(result.parsedCodedBlockPattern, "CABAC macroblock syntax P_8x8 residual non-zero CBF keeps CBP");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 residual non-zero CBF diagnostic");
    require(result.residualLuma4x4BlockIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF partial block count");
    require(result.residualCodedBlockFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF partial flag count");
    require(result.residualLuma4x4BlockIndices[0] == 12,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF partial block index");
    require(result.residualCodedBlockFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF partial flag");
    require(result.residualSignificantScanIndices.size() == 15,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF partial significant scan count");
    require(result.residualSignificantCoeffFlags.size() == 15,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF partial significant flag count");
    require(result.residualSignificantScanIndices[0] == 0
                && result.residualSignificantScanIndices[14] == 14,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF partial significant scan indices");
    require(result.residualSignificantCoeffFlags[0] == 0
                && result.residualSignificantCoeffFlags[14] == 0,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF partial significant flag values");
    require(result.residualIncompleteBlockIndex == 12,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF incomplete block index");
    require(result.residualIncompleteScanIndex == 15,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF incomplete scan index");
    require(result.residualIncompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC macroblock syntax P_8x8 residual non-zero CBF incomplete stage");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF inferred coeff level count");
    require(result.residualCoeffReverseScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF reverse scan count");
    require(result.residualCoeffReverseScanIndices[0] == 15,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF reverse scan order");
    require(result.residualCoeffAbsLevelScanIndices[0] == 15,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF inferred coeff level scan");
    require(result.residualCoeffAbsLevelInferredFinalFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF inferred coeff level flag count");
    require(result.residualCoeffAbsLevelInferredFinalFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF inferred coeff level flag");
    require(result.residualCoeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF inferred coeff level first bin count");
    require(result.residualCoeffAbsLevelPrefixFirstBins[0] == 0,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF inferred coeff level first bin");
    require(result.residualCoeffSignFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF sign flag count");
    require(result.residualCoeffSignFlags[0] == 0,
            "CABAC macroblock syntax P_8x8 residual non-zero CBF sign flag");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag[12][15]")),
            "CABAC macroblock syntax P_8x8 residual non-zero CBF indexed message");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag")),
            "CABAC macroblock syntax P_8x8 residual non-zero CBF stage message");
}

void testReadCabacMacroblockSyntaxP8x8ResidualSignificantOneIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    sps.chromaFormatIdc = 0;
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(249);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 0});
    contexts.setModel(47, {0, 0});
    contexts.setModel(60, {0, 0});
    contexts.setModel(73, {0, 0});
    contexts.setModel(74, {0, 0});
    contexts.setModel(75, {0, 0});
    contexts.setModel(76, {0, 1});
    contexts.setModel(85, {0, 1});
    contexts.setModel(134, {0, 1});
    contexts.setModel(166, {0, 1});
    contexts.setModel(248, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 significant one prefix result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 significant one incomplete");
    require(result.residualLuma4x4BlockIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 significant one partial block count");
    require(result.residualCodedBlockFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 significant one CBF value");
    require(result.residualSignificantScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 significant one scan count");
    require(result.residualSignificantCoeffFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 significant one flag count");
    require(result.residualSignificantScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 significant one scan index");
    require(result.residualSignificantCoeffFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 significant one flag value");
    require(result.residualLastSignificantScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 significant one last scan count");
    require(result.residualLastSignificantCoeffFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 significant one last flag count");
    require(result.residualLastSignificantScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 significant one last scan index");
    require(result.residualLastSignificantCoeffFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 significant one last flag value");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 significant one coeff level scan count");
    require(result.residualCoeffReverseScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 significant one reverse scan count");
    require(result.residualCoeffReverseScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 significant one reverse scan order");
    require(result.residualCoeffAbsLevelScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 significant one coeff level scan index");
    require(result.residualCoeffAbsLevelInferredFinalFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 significant one coeff level inferred flag count");
    require(result.residualCoeffAbsLevelInferredFinalFlags[0] == 0,
            "CABAC macroblock syntax P_8x8 significant one coeff level inferred flag");
    require(result.residualCoeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC macroblock syntax P_8x8 significant one coeff level first bin count");
    require(result.residualCoeffAbsLevelPrefixFirstBins[0] == 0,
            "CABAC macroblock syntax P_8x8 significant one coeff level first bin value");
    require(result.residualIncompleteBlockIndex == 12,
            "CABAC macroblock syntax P_8x8 significant one incomplete block index");
    require(result.residualIncompleteScanIndex == 0,
            "CABAC macroblock syntax P_8x8 significant one incomplete scan index");
    require(result.residualIncompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC macroblock syntax P_8x8 significant one incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 significant one diagnostic");
    require(result.residualCoeffSignFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 significant one sign flag count");
    require(result.residualCoeffSignFlags[0] == 0,
            "CABAC macroblock syntax P_8x8 significant one sign flag");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag")),
            "CABAC macroblock syntax P_8x8 significant one stage message");
}

void testReadCabacMacroblockSyntaxP8x8ResidualCoeffLevelNextBinPartial()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContexts(context.currentQp, 1, 0);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 coeff level next-bin result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 coeff level next-bin incomplete");
    require(result.residualCoeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level next-bin first count");
    require(result.residualCoeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level next-bin first value");
    require(result.residualCoeffAbsLevelPrefixNextBins.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level next-bin count");
    require(result.residualCoeffAbsLevelPrefixNextBins[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level next-bin value");
    require(result.residualIncompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC macroblock syntax P_8x8 coeff level next-bin incomplete stage");
    require(result.residualCoeffSignFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level next-bin sign flag count");
    require(result.residualCoeffSignFlags[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level next-bin sign flag");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag")),
            "CABAC macroblock syntax P_8x8 coeff level next-bin message");
}

void testReadCabacMacroblockSyntaxP8x8ResidualCoeffLevelThirdBinZeroPartial()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContexts(context.currentQp, 1, 1, 0);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 coeff level third-bin zero result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 coeff level third-bin zero incomplete");
    require(result.parsedCodedBlockPattern,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero keeps CBP");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero scan count");
    require(result.residualCoeffAbsLevelScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero scan index");
    require(result.residualCoeffReverseScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero reverse count");
    require(result.residualCoeffReverseScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero reverse scan");
    require(result.residualCoeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero first count");
    require(result.residualCoeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero first value");
    require(result.residualCoeffAbsLevelPrefixNextBins.size() == 2,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero next count");
    require(result.residualCoeffAbsLevelPrefixNextBins[0] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[1] == 0,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero next values");
    require(result.residualCoeffSignFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero sign flag count");
    require(result.residualCoeffSignFlags[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero sign flag");
    require(result.residualCoeffAbsLevelReadyForValueFlags.size()
                == result.residualCoeffAbsLevelScanIndices.size(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero ready flag alignment");
    require(result.residualCoeffAbsLevelReadyForValueFlags[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero ready flag value");
    require(result.residualCoeffAbsLevelReadyPrefixOneCounts.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no ready prefix one-count");
    require(result.residualCoeffAbsLevelReadySuffixBins.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no ready suffix bins");
    require(result.residualCoeffAbsLevelRemainingInputBins.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no remaining input bins");
    require(result.residualCoeffAbsLevelRemainingInputBinCounts.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no remaining input count");
    require(result.residualCoeffAbsLevelReadyRemainingInputBins.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no ready remaining input bins");
    require(result.residualCoeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no ready additional pre-UEG0 parsing flag");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no ready additional pre-UEG0 target");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no ready additional pre-UEG0 remaining prefix bins");
    require(result.residualCoeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no ready can-continue pre-UEG0 prefix parsing flag");
    require(result.residualCoeffAbsLevelValueInputCompleteFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no value input complete flag");
    require(result.residualCoeffAbsLevelFixedInputRecognizedFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no fixed input recognized flag");
    require(result.residualCoeffAbsLevelPreUeg0RemainingInputFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no pre-UEG0 remaining input flag");
    require(result.residualCoeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero no additional pre-UEG0 parsing flag");
    require(result.residualIncompleteBlockIndex == 12,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero incomplete block");
    require(result.residualIncompleteScanIndex == 0,
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero incomplete scan");
    require(result.residualIncompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag")),
            "CABAC macroblock syntax P_8x8 coeff level third-bin zero message");
}

void testReadCabacMacroblockSyntaxP8x8ResidualCoeffLevelThirdBinOnePartial()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContexts(context.currentQp, 1, 1, 1);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 coeff level third-bin one result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 coeff level third-bin one incomplete");
    require(result.parsedCodedBlockPattern,
            "CABAC macroblock syntax P_8x8 coeff level third-bin one keeps CBP");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level third-bin one scan count");
    require(result.residualCoeffAbsLevelScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level third-bin one scan index");
    require(result.residualCoeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level third-bin one first count");
    require(result.residualCoeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level third-bin one first value");
    require(result.residualCoeffAbsLevelPrefixNextBins.size() == 2,
            "CABAC macroblock syntax P_8x8 coeff level third-bin one next count");
    require(result.residualCoeffAbsLevelPrefixNextBins[0] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[1] == 1,
            "CABAC macroblock syntax P_8x8 coeff level third-bin one next values");
    require(result.residualCoeffSignFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no sign flag");
    require(result.residualCoeffAbsLevelSuffixBins.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no suffix bins");
    require(result.residualCoeffAbsLevelReadyForValueFlags.size()
                == result.residualCoeffAbsLevelScanIndices.size(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one ready flag alignment");
    require(result.residualCoeffAbsLevelReadyForValueFlags[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level third-bin one ready flag value");
    require(result.residualCoeffAbsLevelReadyPrefixOneCounts.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no ready prefix one-count");
    require(result.residualCoeffAbsLevelReadySuffixBins.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no ready suffix bins");
    require(result.residualCoeffAbsLevelRemainingInputBins.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no remaining input bins");
    require(result.residualCoeffAbsLevelRemainingInputBinCounts.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no remaining input count");
    require(result.residualCoeffAbsLevelReadyRemainingInputBins.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no ready remaining input bins");
    require(result.residualCoeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no ready additional pre-UEG0 parsing flag");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no ready additional pre-UEG0 target");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no ready additional pre-UEG0 remaining prefix bins");
    require(result.residualCoeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no ready can-continue pre-UEG0 prefix parsing flag");
    require(result.residualCoeffAbsLevelValueInputCompleteFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no value input complete flag");
    require(result.residualCoeffAbsLevelFixedInputRecognizedFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no fixed input recognized flag");
    require(result.residualCoeffAbsLevelPreUeg0RemainingInputFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no pre-UEG0 remaining input flag");
    require(result.residualCoeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one no additional pre-UEG0 parsing flag");
    require(result.residualIncompleteBlockIndex == 12,
            "CABAC macroblock syntax P_8x8 coeff level third-bin one incomplete block");
    require(result.residualIncompleteScanIndex == 0,
            "CABAC macroblock syntax P_8x8 coeff level third-bin one incomplete scan");
    require(result.residualIncompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("covered prefix bins did not terminate")),
            "CABAC macroblock syntax P_8x8 coeff level third-bin one message");
}

void testReadCabacMacroblockSyntaxP8x8ResidualCoeffLevelFourthBinZeroPartial()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContextsWithFourthContext(context.currentQp, 1, 1, 1, 0);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero incomplete");
    require(result.parsedCodedBlockPattern,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero keeps CBP");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero scan count");
    require(result.residualCoeffAbsLevelScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero scan index");
    require(result.residualCoeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero first count");
    require(result.residualCoeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero first value");
    require(result.residualCoeffAbsLevelPrefixNextBins.size() == 3,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero next count");
    require(result.residualCoeffAbsLevelPrefixNextBins[0] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[1] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[2] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero next values");
    require(result.residualCoeffSignFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero sign flag count");
    require(result.residualCoeffSignFlags[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero sign flag");
    require(result.residualIncompleteBlockIndex == 12,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero incomplete block");
    require(result.residualIncompleteScanIndex == 0,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero incomplete scan");
    require(result.residualIncompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag")),
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin zero message");
}

void testReadCabacMacroblockSyntaxP8x8ResidualCoeffLevelFourthBinOnePartial()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContextsWithFourthContext(context.currentQp, 1, 1, 1, 1);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 coeff level fourth-bin one result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 coeff level fourth-bin one incomplete");
    require(result.parsedCodedBlockPattern,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one keeps CBP");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one scan count");
    require(result.residualCoeffAbsLevelScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one scan index");
    require(result.residualCoeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one first count");
    require(result.residualCoeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one first value");
    require(result.residualCoeffAbsLevelPrefixNextBins.size() == 3,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one next count");
    require(result.residualCoeffAbsLevelPrefixNextBins[0] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[1] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[2] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one next values");
    require(result.residualCoeffSignFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one no sign flag");
    require(result.residualIncompleteBlockIndex == 12,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one incomplete block");
    require(result.residualIncompleteScanIndex == 0,
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one incomplete scan");
    require(result.residualIncompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("covered prefix bins did not terminate")),
            "CABAC macroblock syntax P_8x8 coeff level fourth-bin one message");
}

void testReadCabacMacroblockSyntaxP8x8ResidualCoeffLevelFifthBinZeroPartial()
{
    BitReader reader(QByteArray::fromHex("00000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContextsWithFifthContext(context.currentQp, 1, 1, 1, 1, 0);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero incomplete");
    require(result.parsedCodedBlockPattern,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero keeps CBP");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero scan count");
    require(result.residualCoeffAbsLevelScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero scan index");
    require(result.residualCoeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero first count");
    require(result.residualCoeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero first value");
    require(result.residualCoeffAbsLevelPrefixNextBins.size() == 4,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero next count");
    require(result.residualCoeffAbsLevelPrefixNextBins[0] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[1] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[2] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[3] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero next values");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero terminated count");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero terminated value");
    require(result.residualCoeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero one-count count");
    require(result.residualCoeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero one-count value");
    require(result.residualCoeffSignFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero no sign flag");
    require(result.residualIncompleteBlockIndex == 12,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero incomplete block");
    require(result.residualIncompleteScanIndex == 0,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero incomplete scan");
    require(result.residualIncompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("suffix bypass bin")),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero message");
    require(result.diagnosticMessage.contains(QStringLiteral("prefix one-count 4")),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero one-count message");
    require(result.residualCoeffAbsLevelSuffixBins.size() == 4,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero suffix count");
    require(result.residualCoeffAbsLevelSuffixBins[0] == 0
                && result.residualCoeffAbsLevelSuffixBins[1] == 0
                && result.residualCoeffAbsLevelSuffixBins[2] == 0
                && result.residualCoeffAbsLevelSuffixBins[3] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero suffix values");
    require(result.residualCoeffAbsLevelReadyForValueFlags.size()
                == result.residualCoeffAbsLevelScanIndices.size(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready flag alignment");
    require(result.residualCoeffAbsLevelReadyForValueFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready flag value");
    require(result.residualCoeffAbsLevelReadyPrefixOneCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready prefix one-count count");
    require(result.residualCoeffAbsLevelReadyPrefixOneCounts[0] == 4,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready prefix one-count value");
    require(result.residualCoeffAbsLevelReadySuffixBins.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready suffix group count");
    require(result.residualCoeffAbsLevelReadySuffixBins[0].size() == 4,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready suffix group size");
    require(result.residualCoeffAbsLevelReadySuffixBins[0][0] == 0
                && result.residualCoeffAbsLevelReadySuffixBins[0][1] == 0
                && result.residualCoeffAbsLevelReadySuffixBins[0][2] == 0
                && result.residualCoeffAbsLevelReadySuffixBins[0][3] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready suffix values");
    require(result.residualCoeffAbsLevelRemainingInputBins.size() == 4,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero remaining input count");
    require(result.residualCoeffAbsLevelRemainingInputBins[0] == 0
                && result.residualCoeffAbsLevelRemainingInputBins[1] == 0
                && result.residualCoeffAbsLevelRemainingInputBins[2] == 0
                && result.residualCoeffAbsLevelRemainingInputBins[3] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero remaining input values");
    require(result.residualCoeffAbsLevelRemainingInputBinCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero remaining input bin-count count");
    require(result.residualCoeffAbsLevelRemainingInputBinCounts[0] == 4,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero remaining input bin-count value");
    require(result.residualCoeffAbsLevelReadyRemainingInputBins.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready remaining input group count");
    require(result.residualCoeffAbsLevelReadyRemainingInputBins[0].size() == 4,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready remaining input group size");
    require(result.residualCoeffAbsLevelReadyRemainingInputBins[0][0] == 0
                && result.residualCoeffAbsLevelReadyRemainingInputBins[0][1] == 0
                && result.residualCoeffAbsLevelReadyRemainingInputBins[0][2] == 0
                && result.residualCoeffAbsLevelReadyRemainingInputBins[0][3] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready remaining input values");
    require(result.residualCoeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready additional pre-UEG0 parsing count");
    require(result.residualCoeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready additional pre-UEG0 parsing value");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready additional pre-UEG0 target count");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts[0]
                == h264CabacCoeffAbsLevelMinus1Ueg0Cutoff(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready additional pre-UEG0 target value");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready additional pre-UEG0 remaining prefix bin count");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts[0] == 10,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready additional pre-UEG0 remaining prefix bins");
    require(result.residualCoeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready can-continue pre-UEG0 prefix parsing count");
    require(result.residualCoeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero ready can-continue pre-UEG0 prefix parsing value");
    require(result.residualCoeffAbsLevelValueInputCompleteFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero value input complete count");
    require(result.residualCoeffAbsLevelValueInputCompleteFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero value input complete value");
    require(result.residualCoeffAbsLevelFixedInputRecognizedFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero fixed input recognized count");
    require(result.residualCoeffAbsLevelFixedInputRecognizedFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero fixed input recognized value");
    require(result.residualCoeffAbsLevelPreUeg0RemainingInputFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero pre-UEG0 remaining input count");
    require(result.residualCoeffAbsLevelPreUeg0RemainingInputFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero pre-UEG0 remaining input value");
    require(result.residualCoeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero additional pre-UEG0 parsing count");
    require(result.residualCoeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin zero additional pre-UEG0 parsing value");
}

void testReadCabacMacroblockSyntaxP8x8ResidualLargeTerminatedPrefixStopsBeforeSign()
{
    BitReader reader(QByteArray::fromHex("00000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContextsWithFifthContext(context.currentQp, 1, 1, 1, 1, 0);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 large terminated prefix result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 large terminated prefix incomplete");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 large terminated prefix scan count");
    require(result.residualCoeffAbsLevelScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 large terminated prefix scan index");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 large terminated prefix terminated count");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 large terminated prefix terminated value");
    require(result.residualCoeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 large terminated prefix one-count count");
    require(result.residualCoeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC macroblock syntax P_8x8 large terminated prefix one-count value");
    require(result.residualCoeffSignFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 large terminated prefix stops before sign flag");
    require(result.residualIncompleteBlockIndex == 12,
            "CABAC macroblock syntax P_8x8 large terminated prefix incomplete block");
    require(result.residualIncompleteScanIndex == 0,
            "CABAC macroblock syntax P_8x8 large terminated prefix incomplete scan");
    require(result.residualIncompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC macroblock syntax P_8x8 large terminated prefix stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 large terminated prefix diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("suffix bypass bin")),
            "CABAC macroblock syntax P_8x8 large terminated prefix message");
    require(result.diagnosticMessage.contains(QStringLiteral("prefix one-count 4")),
            "CABAC macroblock syntax P_8x8 large terminated prefix one-count message");
    require(result.residualCoeffAbsLevelSuffixBins.size() == 4,
            "CABAC macroblock syntax P_8x8 large terminated prefix suffix count");
    require(result.residualCoeffAbsLevelSuffixBins[0] == 0
                && result.residualCoeffAbsLevelSuffixBins[1] == 0
                && result.residualCoeffAbsLevelSuffixBins[2] == 0
                && result.residualCoeffAbsLevelSuffixBins[3] == 0,
            "CABAC macroblock syntax P_8x8 large terminated prefix suffix values");
}

void testReadCabacMacroblockSyntaxP8x8ResidualFirstSuffixBypassBinPartial()
{
    BitReader reader(QByteArray::fromHex("00000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContextsWithFifthContext(context.currentQp, 1, 1, 1, 1, 0);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 first suffix bypass bin result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 first suffix bypass bin incomplete");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 first suffix bypass bin scan count");
    require(result.residualCoeffAbsLevelScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 first suffix bypass bin scan index");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 first suffix bypass bin terminated count");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 first suffix bypass bin terminated value");
    require(result.residualCoeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 first suffix bypass bin one-count count");
    require(result.residualCoeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC macroblock syntax P_8x8 first suffix bypass bin one-count value");
    require(result.residualCoeffAbsLevelSuffixBins.size() == 4,
            "CABAC macroblock syntax P_8x8 first suffix bypass bin suffix count");
    require(result.residualCoeffAbsLevelSuffixBins[0] == 0
                && result.residualCoeffAbsLevelSuffixBins[1] == 0
                && result.residualCoeffAbsLevelSuffixBins[2] == 0
                && result.residualCoeffAbsLevelSuffixBins[3] == 0,
            "CABAC macroblock syntax P_8x8 first suffix bypass bin suffix values");
    require(result.residualCoeffSignFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 first suffix bypass bin no sign flag");
    require(result.residualIncompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC macroblock syntax P_8x8 first suffix bypass bin stage");
    require(result.diagnosticMessage.contains(QStringLiteral("suffix bypass bin")),
            "CABAC macroblock syntax P_8x8 first suffix bypass bin message");
}

void testReadCabacMacroblockSyntaxP8x8ResidualSecondSuffixBypassBinPartial()
{
    BitReader reader(QByteArray::fromHex("00000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContextsWithFifthContext(context.currentQp, 1, 1, 1, 1, 0);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 second suffix bypass bin result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 second suffix bypass bin incomplete");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 second suffix bypass bin scan count");
    require(result.residualCoeffAbsLevelScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 second suffix bypass bin scan index");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 second suffix bypass bin terminated count");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 second suffix bypass bin terminated value");
    require(result.residualCoeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 second suffix bypass bin one-count count");
    require(result.residualCoeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC macroblock syntax P_8x8 second suffix bypass bin one-count value");
    require(result.residualCoeffAbsLevelSuffixBins.size() == 4,
            "CABAC macroblock syntax P_8x8 second suffix bypass bin suffix count");
    require(result.residualCoeffAbsLevelSuffixBins[0] == 0
                && result.residualCoeffAbsLevelSuffixBins[1] == 0
                && result.residualCoeffAbsLevelSuffixBins[2] == 0
                && result.residualCoeffAbsLevelSuffixBins[3] == 0,
            "CABAC macroblock syntax P_8x8 second suffix bypass bin suffix values");
    require(result.residualCoeffSignFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 second suffix bypass bin no sign flag");
    require(result.residualIncompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC macroblock syntax P_8x8 second suffix bypass bin stage");
    require(result.diagnosticMessage.contains(QStringLiteral("suffix bypass bin")),
            "CABAC macroblock syntax P_8x8 second suffix bypass bin message");
}

void testReadCabacMacroblockSyntaxP8x8ResidualThirdSuffixBypassBinPartial()
{
    BitReader reader(QByteArray::fromHex("00000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContextsWithFifthContext(context.currentQp, 1, 1, 1, 1, 0);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 third suffix bypass bin result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 third suffix bypass bin incomplete");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 third suffix bypass bin scan count");
    require(result.residualCoeffAbsLevelScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 third suffix bypass bin scan index");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 third suffix bypass bin terminated count");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 third suffix bypass bin terminated value");
    require(result.residualCoeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 third suffix bypass bin one-count count");
    require(result.residualCoeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC macroblock syntax P_8x8 third suffix bypass bin one-count value");
    require(result.residualCoeffAbsLevelSuffixBins.size() == 4,
            "CABAC macroblock syntax P_8x8 third suffix bypass bin suffix count");
    require(result.residualCoeffAbsLevelSuffixBins[0] == 0
                && result.residualCoeffAbsLevelSuffixBins[1] == 0
                && result.residualCoeffAbsLevelSuffixBins[2] == 0
                && result.residualCoeffAbsLevelSuffixBins[3] == 0,
            "CABAC macroblock syntax P_8x8 third suffix bypass bin suffix values");
    require(result.residualCoeffSignFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 third suffix bypass bin no sign flag");
    require(result.residualIncompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC macroblock syntax P_8x8 third suffix bypass bin stage");
    require(result.diagnosticMessage.contains(QStringLiteral("suffix bypass bin")),
            "CABAC macroblock syntax P_8x8 third suffix bypass bin message");
}

void testReadCabacMacroblockSyntaxP8x8ResidualFourthSuffixBypassBinPartial()
{
    BitReader reader(QByteArray::fromHex("00000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContextsWithFifthContext(context.currentQp, 1, 1, 1, 1, 0);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 fourth suffix bypass bin result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 fourth suffix bypass bin incomplete");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin scan count");
    require(result.residualCoeffAbsLevelScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin scan index");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin terminated count");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin terminated value");
    require(result.residualCoeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin one-count count");
    require(result.residualCoeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin one-count value");
    require(result.residualCoeffAbsLevelSuffixBins.size() == 4,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin suffix count");
    require(result.residualCoeffAbsLevelSuffixBins[0] == 0
                && result.residualCoeffAbsLevelSuffixBins[1] == 0
                && result.residualCoeffAbsLevelSuffixBins[2] == 0
                && result.residualCoeffAbsLevelSuffixBins[3] == 0,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin suffix values");
    require(result.residualCoeffAbsLevelSuffixBinCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin count count");
    require(result.residualCoeffAbsLevelSuffixBinCounts[0] == 4,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin count value");
    require(result.residualCoeffAbsLevelReadyForValueFlags.size()
                == result.residualCoeffAbsLevelScanIndices.size(),
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready flag alignment");
    require(result.residualCoeffAbsLevelReadyForValueFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready flag value");
    require(result.residualCoeffAbsLevelReadyPrefixOneCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready prefix one-count count");
    require(result.residualCoeffAbsLevelReadyPrefixOneCounts[0] == 4,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready prefix one-count value");
    require(result.residualCoeffAbsLevelReadySuffixBins.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready suffix group count");
    require(result.residualCoeffAbsLevelReadySuffixBins[0].size() == 4,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready suffix group size");
    require(result.residualCoeffAbsLevelReadySuffixBins[0][0] == 0
                && result.residualCoeffAbsLevelReadySuffixBins[0][1] == 0
                && result.residualCoeffAbsLevelReadySuffixBins[0][2] == 0
                && result.residualCoeffAbsLevelReadySuffixBins[0][3] == 0,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready suffix values");
    require(result.residualCoeffAbsLevelRemainingInputBins.size() == 4,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin remaining input count");
    require(result.residualCoeffAbsLevelRemainingInputBinCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin remaining input bin-count count");
    require(result.residualCoeffAbsLevelRemainingInputBinCounts[0] == 4,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin remaining input bin-count value");
    require(result.residualCoeffAbsLevelReadyRemainingInputBins.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready remaining input group count");
    require(result.residualCoeffAbsLevelReadyRemainingInputBins[0].size() == 4,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready remaining input group size");
    require(result.residualCoeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready additional pre-UEG0 parsing count");
    require(result.residualCoeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready additional pre-UEG0 parsing value");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready additional pre-UEG0 target count");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts[0]
                == h264CabacCoeffAbsLevelMinus1Ueg0Cutoff(),
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready additional pre-UEG0 target value");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready additional pre-UEG0 remaining prefix bin count");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts[0] == 10,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready additional pre-UEG0 remaining prefix bins");
    require(result.residualCoeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready can-continue pre-UEG0 prefix parsing count");
    require(result.residualCoeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin ready can-continue pre-UEG0 prefix parsing value");
    require(result.residualCoeffAbsLevelValueInputCompleteFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin value input complete count");
    require(result.residualCoeffAbsLevelValueInputCompleteFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin value input complete value");
    require(result.residualCoeffAbsLevelFixedInputRecognizedFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin fixed input recognized count");
    require(result.residualCoeffAbsLevelFixedInputRecognizedFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin fixed input recognized value");
    require(result.residualCoeffAbsLevelPreUeg0RemainingInputFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin pre-UEG0 remaining input count");
    require(result.residualCoeffAbsLevelPreUeg0RemainingInputFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin pre-UEG0 remaining input value");
    require(result.residualCoeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin additional pre-UEG0 parsing count");
    require(result.residualCoeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags[0] == 1,
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin additional pre-UEG0 parsing value");
    require(result.residualCoeffSignFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin no sign flag");
    require(result.residualIncompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin stage");
    require(result.diagnosticMessage.contains(QStringLiteral("fourth suffix bypass bin")),
            "CABAC macroblock syntax P_8x8 fourth suffix bypass bin message");
}

void testReadCabacMacroblockSyntaxP8x8ResidualCoeffLevelSixthBinOnePartial()
{
    BitReader reader(QByteArray::fromHex("00000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContextsWithFifthContext(context.currentQp, 1, 1, 1, 1, 1);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 coeff level fifth-bin one result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 coeff level fifth-bin one incomplete");
    require(result.parsedCodedBlockPattern,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one keeps CBP");
    require(result.residualCoeffAbsLevelScanIndices.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one scan count");
    require(result.residualCoeffAbsLevelScanIndices[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one scan index");
    require(result.residualCoeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one first count");
    require(result.residualCoeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one first value");
    require(result.residualCoeffAbsLevelPrefixNextBins.size() == 5,
            "CABAC macroblock syntax P_8x8 coeff level sixth-bin one next count");
    require(result.residualCoeffAbsLevelPrefixNextBins[0] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[1] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[2] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[3] == 1
                && result.residualCoeffAbsLevelPrefixNextBins[4] == 1,
            "CABAC macroblock syntax P_8x8 coeff level sixth-bin one next values");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one terminated count");
    require(result.residualCoeffAbsLevelPrefixTerminatedFlags[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one terminated value");
    require(result.residualCoeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one one-count count");
    require(result.residualCoeffAbsLevelPrefixOneCounts[0] == 6,
            "CABAC macroblock syntax P_8x8 coeff level sixth-bin one one-count value");
    require(result.residualCoeffSignFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no sign flag");
    require(result.residualCoeffAbsLevelSuffixBins.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no suffix bins");
    require(result.residualCoeffAbsLevelReadyForValueFlags.size()
                == result.residualCoeffAbsLevelScanIndices.size(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one ready flag alignment");
    require(result.residualCoeffAbsLevelReadyForValueFlags[0] == 0,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one ready flag value");
    require(result.residualCoeffAbsLevelReadyPrefixOneCounts.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no ready prefix one-count");
    require(result.residualCoeffAbsLevelReadySuffixBins.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no ready suffix bins");
    require(result.residualCoeffAbsLevelRemainingInputBins.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no remaining input bins");
    require(result.residualCoeffAbsLevelRemainingInputBinCounts.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no remaining input count");
    require(result.residualCoeffAbsLevelReadyRemainingInputBins.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no ready remaining input bins");
    require(result.residualCoeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no ready additional pre-UEG0 parsing flag");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no ready additional pre-UEG0 target");
    require(result.residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no ready additional pre-UEG0 remaining prefix bins");
    require(result.residualCoeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no ready can-continue pre-UEG0 prefix parsing flag");
    require(result.residualCoeffAbsLevelValueInputCompleteFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no value input complete flag");
    require(result.residualCoeffAbsLevelFixedInputRecognizedFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no fixed input recognized flag");
    require(result.residualCoeffAbsLevelPreUeg0RemainingInputFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no pre-UEG0 remaining input flag");
    require(result.residualCoeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags.isEmpty(),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one no additional pre-UEG0 parsing flag");
    require(result.residualIncompleteBlockIndex == 12,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one incomplete block");
    require(result.residualIncompleteScanIndex == 0,
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one incomplete scan");
    require(result.residualIncompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("covered prefix bins did not terminate")),
            "CABAC macroblock syntax P_8x8 coeff level fifth-bin one message");
}

void testReadCabacMacroblockSyntaxP8x8SmallNonZeroMvd()
{
    BitReader reader(QByteArray::fromHex("000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(78);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 1});
    contexts.setModel(43, {0, 0});
    contexts.setModel(47, {0, 0});
    setCbpZeroContexts(contexts);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 small non-zero mvd result");
    require(result.complete, "CABAC macroblock syntax P_8x8 small non-zero mvd complete");
    require(result.parsedCodedBlockPatternZero,
            "CABAC macroblock syntax P_8x8 small non-zero mvd CBP zero flag");
    require(result.mvdL0.size() == 4, "CABAC macroblock syntax P_8x8 small non-zero mvd pair count");
    require(result.mvdL0[0].x == 1 && result.mvdL0[0].y == 0,
            "CABAC macroblock syntax P_8x8 small non-zero first mvd pair");
}

void testReadCabacMacroblockSyntaxP8x8NonZeroMvdIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    slice.numRefIdxL0ActiveMinus1 = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(60);
    contexts.setModel(14, {0, 0});
    contexts.setModel(15, {0, 0});
    contexts.setModel(16, {0, 1});
    contexts.setModel(21, {0, 1});
    contexts.setModel(40, {0, 1});
    contexts.setModel(43, {0, 1});
    contexts.setModel(44, {0, 1});
    contexts.setModel(47, {0, 0});

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);
    require(result.ok, "CABAC macroblock syntax P_8x8 non-zero mvd prefix result");
    require(!result.complete, "CABAC macroblock syntax P_8x8 non-zero mvd incomplete");
    require(result.diagnosticCode == QStringLiteral("cabac_mvd_incomplete"),
            "CABAC macroblock syntax P_8x8 non-zero mvd diagnostic");
}

void testReadCodedBlockPatternZeroMonochrome()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    sps.chromaFormatIdc = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(78);
    setCbpZeroContexts(contexts);

    const H264CabacCodedBlockPatternResult result =
        h264ReadCabacCodedBlockPatternZero(reader, decoder, contexts, context);
    require(result.ok, "CABAC coded_block_pattern zero monochrome result");
    require(result.complete, "CABAC coded_block_pattern zero monochrome complete");
    require(result.codedBlockPattern == 0, "CABAC coded_block_pattern zero monochrome value");
    require(result.codedBlockPatternLuma == 0, "CABAC coded_block_pattern zero monochrome luma");
    require(result.codedBlockPatternChroma == 0, "CABAC coded_block_pattern zero monochrome chroma");
    require(result.firstCtxIdx == 73, "CABAC coded_block_pattern zero first ctxIdx");
    require(contexts.model(73).stateIndex == 1, "CABAC coded_block_pattern zero updates ctxIdx 73");
    require(contexts.model(76).stateIndex == 1, "CABAC coded_block_pattern zero updates ctxIdx 76");
}

void testReadCodedBlockPatternZeroChroma()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(78);
    setCbpZeroContexts(contexts);

    const H264CabacCodedBlockPatternResult result =
        h264ReadCabacCodedBlockPatternZero(reader, decoder, contexts, context);
    require(result.ok, "CABAC coded_block_pattern zero chroma result");
    require(result.complete, "CABAC coded_block_pattern zero chroma complete");
    require(result.codedBlockPattern == 0, "CABAC coded_block_pattern zero chroma value");
    require(result.codedBlockPatternLuma == 0, "CABAC coded_block_pattern zero chroma luma");
    require(result.codedBlockPatternChroma == 0, "CABAC coded_block_pattern zero chroma component");
    require(contexts.model(77).stateIndex == 1, "CABAC coded_block_pattern zero updates ctxIdx 77");
}

void testReadCodedBlockPatternNonZeroLumaIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    sps.chromaFormatIdc = 0;
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(78);
    contexts.setModel(73, {0, 1});
    contexts.setModel(74, {0, 0});
    contexts.setModel(75, {0, 0});
    contexts.setModel(76, {0, 0});

    const H264CabacCodedBlockPatternResult result =
        h264ReadCabacCodedBlockPatternZero(reader, decoder, contexts, context);
    require(result.ok, "CABAC coded_block_pattern non-zero luma prefix result");
    require(!result.complete, "CABAC coded_block_pattern non-zero luma incomplete");
    require(result.codedBlockPatternLuma != 0, "CABAC coded_block_pattern non-zero luma value");
    require(result.diagnosticCode == QStringLiteral("cabac_cbp_incomplete"),
            "CABAC coded_block_pattern non-zero luma diagnostic");
}

void testReadCodedBlockPatternNonZeroChromaIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(78);
    for (int ctxIdx = 73; ctxIdx <= 76; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
    contexts.setModel(77, {0, 1});

    const H264CabacCodedBlockPatternResult result =
        h264ReadCabacCodedBlockPatternZero(reader, decoder, contexts, context);
    require(result.ok, "CABAC coded_block_pattern non-zero chroma prefix result");
    require(!result.complete, "CABAC coded_block_pattern non-zero chroma incomplete");
    require(result.codedBlockPatternChroma != 0, "CABAC coded_block_pattern non-zero chroma value");
    require(result.diagnosticCode == QStringLiteral("cabac_cbp_incomplete"),
            "CABAC coded_block_pattern non-zero chroma diagnostic");
}

void testReadMbQpDeltaZero()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(61);
    contexts.setModel(60, {0, 0});

    const H264CabacMbQpDeltaResult result =
        h264ReadCabacMbQpDeltaZero(reader, decoder, contexts);
    require(result.ok, "CABAC mb_qp_delta zero result");
    require(result.complete, "CABAC mb_qp_delta zero complete");
    require(result.mbQpDelta == 0, "CABAC mb_qp_delta zero value");
    require(result.firstCtxIdx == 60, "CABAC mb_qp_delta zero ctxIdx");
    require(contexts.model(60).stateIndex == 1, "CABAC mb_qp_delta zero updates ctxIdx 60");
}

void testReadMbQpDeltaNonZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(61);
    contexts.setModel(60, {0, 1});

    const H264CabacMbQpDeltaResult result =
        h264ReadCabacMbQpDeltaZero(reader, decoder, contexts);
    require(result.ok, "CABAC mb_qp_delta non-zero prefix result");
    require(!result.complete, "CABAC mb_qp_delta non-zero incomplete");
    require(result.firstCtxIdx == 60, "CABAC mb_qp_delta non-zero ctxIdx");
    require(result.diagnosticCode == QStringLiteral("cabac_mb_qp_delta_incomplete"),
            "CABAC mb_qp_delta non-zero diagnostic");
}

void testReadResidualCodedBlockFlagZeroLuma4x4()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(85, {0, 0});

    const H264CabacResidualBlockResult result =
        h264ReadCabacResidualCodedBlockFlagZero(
            reader,
            decoder,
            contexts,
            H264CabacResidualBlockCategory::Luma4x4);
    require(result.ok, "CABAC residual coded_block_flag zero result");
    require(result.complete, "CABAC residual coded_block_flag zero complete");
    require(result.codedBlockFlag == 0, "CABAC residual coded_block_flag zero value");
    require(result.ctxIdx == 85, "CABAC residual coded_block_flag zero ctxIdx");
    require(contexts.model(85).stateIndex == 1,
            "CABAC residual coded_block_flag zero updates ctxIdx 85");
}

void testReadResidualCodedBlockFlagNonZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(85, {0, 1});

    const H264CabacResidualBlockResult result =
        h264ReadCabacResidualCodedBlockFlagZero(
            reader,
            decoder,
            contexts,
            H264CabacResidualBlockCategory::Luma4x4);
    require(result.ok, "CABAC residual coded_block_flag non-zero prefix result");
    require(!result.complete, "CABAC residual coded_block_flag non-zero incomplete");
    require(result.codedBlockFlag == 1, "CABAC residual coded_block_flag non-zero value");
    require(result.ctxIdx == 85, "CABAC residual coded_block_flag non-zero ctxIdx");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual coded_block_flag non-zero diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("luma4x4 coded_block_flag")),
            "CABAC residual coded_block_flag non-zero category message");
    require(result.diagnosticMessage.contains(QStringLiteral("significant_coeff_flag")),
            "CABAC residual coded_block_flag non-zero next-stage message");
}

void testReadResidualLuma4x4CodedBlockFlagNonZeroPartial()
{
    BitReader reader(QByteArray::fromHex("000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(249);
    contexts.setModel(85, {0, 1});
    setLuma4x4SignificantZeroContexts(contexts);
    contexts.setModel(248, {0, 0});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 non-zero CBF partial result");
    require(!result.complete, "CABAC residual luma4x4 non-zero CBF partial incomplete");
    require(result.blockIndices.size() == 1, "CABAC residual luma4x4 non-zero CBF partial block count");
    require(result.codedBlockFlags.size() == 1, "CABAC residual luma4x4 non-zero CBF partial flag count");
    require(result.blockIndices[0] == 12, "CABAC residual luma4x4 non-zero CBF partial block index");
    require(result.codedBlockFlags[0] == 1, "CABAC residual luma4x4 non-zero CBF partial flag");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 non-zero CBF partial incomplete block index");
    require(result.significantScanIndices.size() == 15,
            "CABAC residual luma4x4 non-zero CBF partial significant scan count");
    require(result.significantCoeffFlags.size() == 15,
            "CABAC residual luma4x4 non-zero CBF partial significant flag count");
    require(result.significantScanIndices[0] == 0 && result.significantScanIndices[14] == 14,
            "CABAC residual luma4x4 non-zero CBF partial significant scan indices");
    require(result.significantCoeffFlags[0] == 0 && result.significantCoeffFlags[14] == 0,
            "CABAC residual luma4x4 non-zero CBF partial significant flag values");
    require(result.incompleteScanIndex == 15,
            "CABAC residual luma4x4 non-zero CBF partial incomplete scan index");
    require(result.incompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC residual luma4x4 non-zero CBF partial incomplete stage");
    require(result.coeffAbsLevelScanIndices.size() == 1,
            "CABAC residual luma4x4 non-zero CBF partial inferred coeff level count");
    require(result.coeffReverseScanIndices.size() == 1,
            "CABAC residual luma4x4 non-zero CBF partial reverse scan count");
    require(result.coeffReverseScanIndices[0] == 15,
            "CABAC residual luma4x4 non-zero CBF partial reverse scan order");
    require(result.coeffAbsLevelScanIndices[0] == 15,
            "CABAC residual luma4x4 non-zero CBF partial inferred coeff level scan");
    require(result.coeffAbsLevelInferredFinalFlags.size() == 1,
            "CABAC residual luma4x4 non-zero CBF partial inferred coeff level flag count");
    require(result.coeffAbsLevelInferredFinalFlags[0] == 1,
            "CABAC residual luma4x4 non-zero CBF partial inferred coeff level flag");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 non-zero CBF partial inferred coeff level first bin count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 0,
            "CABAC residual luma4x4 non-zero CBF partial inferred coeff level first bin");
    require(result.coeffSignFlags.size() == 1,
            "CABAC residual luma4x4 non-zero CBF partial sign flag count");
    require(result.coeffSignFlags[0] == 0,
            "CABAC residual luma4x4 non-zero CBF partial sign flag");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 non-zero CBF partial diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag[12][15]")),
            "CABAC residual luma4x4 non-zero CBF partial indexed message");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag")),
            "CABAC residual luma4x4 non-zero CBF partial stage message");
}

void testReadResidualLuma4x4SignificantCoeffFlagMissingLaterContext()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(138);
    contexts.setModel(85, {0, 1});
    contexts.setModel(134, {0, 0});
    contexts.setModel(135, {0, 0});
    contexts.setModel(136, {0, 0});
    contexts.setModel(137, {0, 0});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(!result.ok, "CABAC residual luma4x4 later significant context missing fails");
    require(result.significantScanIndices.size() == 4,
            "CABAC residual luma4x4 later significant context missing keeps partial scans");
    require(result.diagnosticCode == QStringLiteral("cabac_context_uninitialized"),
            "CABAC residual luma4x4 later significant context missing diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("138")),
            "CABAC residual luma4x4 later significant context missing ctxIdx message");
}

void testReadResidualLuma4x4InferredFinalCoeffLevelMissingContext()
{
    BitReader reader(QByteArray::fromHex("000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(149);
    contexts.setModel(85, {0, 1});
    setLuma4x4SignificantZeroContexts(contexts);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(!result.ok, "CABAC residual luma4x4 inferred final coeff level missing context fails");
    require(result.significantScanIndices.size() == 15,
            "CABAC residual luma4x4 inferred final coeff level missing context keeps significant scans");
    require(result.coeffAbsLevelScanIndices.isEmpty(),
            "CABAC residual luma4x4 inferred final coeff level missing context no coeff level entry");
    require(result.diagnosticCode == QStringLiteral("cabac_context_uninitialized"),
            "CABAC residual luma4x4 inferred final coeff level missing context diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("248")),
            "CABAC residual luma4x4 inferred final coeff level missing context ctxIdx message");
    require(result.diagnosticMessage.contains(QStringLiteral("[12][15]")),
            "CABAC residual luma4x4 inferred final coeff level missing context scan message");
}

void testReadResidualLuma4x4SignificantCoeffFlagOneIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(249);
    contexts.setModel(85, {0, 1});
    contexts.setModel(134, {0, 1});
    contexts.setModel(166, {0, 1});
    contexts.setModel(248, {0, 0});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 significant one partial result");
    require(!result.complete, "CABAC residual luma4x4 significant one incomplete");
    require(result.blockIndices.size() == 1, "CABAC residual luma4x4 significant one block count");
    require(result.codedBlockFlags[0] == 1, "CABAC residual luma4x4 significant one CBF value");
    require(result.significantScanIndices.size() == 1,
            "CABAC residual luma4x4 significant one scan count");
    require(result.significantCoeffFlags.size() == 1,
            "CABAC residual luma4x4 significant one flag count");
    require(result.significantScanIndices[0] == 0,
            "CABAC residual luma4x4 significant one scan index");
    require(result.significantCoeffFlags[0] == 1,
            "CABAC residual luma4x4 significant one flag value");
    require(result.lastSignificantScanIndices.size() == 1,
            "CABAC residual luma4x4 significant one last scan count");
    require(result.lastSignificantCoeffFlags.size() == 1,
            "CABAC residual luma4x4 significant one last flag count");
    require(result.lastSignificantScanIndices[0] == 0,
            "CABAC residual luma4x4 significant one last scan index");
    require(result.lastSignificantCoeffFlags[0] == 1,
            "CABAC residual luma4x4 significant one last flag value");
    require(result.coeffAbsLevelScanIndices.size() == 1,
            "CABAC residual luma4x4 significant one coeff level scan count");
    require(result.coeffReverseScanIndices.size() == 1,
            "CABAC residual luma4x4 significant one reverse scan count");
    require(result.coeffReverseScanIndices[0] == 0,
            "CABAC residual luma4x4 significant one reverse scan order");
    require(result.coeffAbsLevelScanIndices[0] == 0,
            "CABAC residual luma4x4 significant one coeff level scan index");
    require(result.coeffAbsLevelInferredFinalFlags.size() == 1,
            "CABAC residual luma4x4 significant one coeff level inferred flag count");
    require(result.coeffAbsLevelInferredFinalFlags[0] == 0,
            "CABAC residual luma4x4 significant one coeff level inferred flag");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 significant one coeff level first bin count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 0,
            "CABAC residual luma4x4 significant one coeff level first bin value");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 significant one incomplete block index");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 significant one incomplete scan index");
    require(result.incompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC residual luma4x4 significant one incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 significant one diagnostic");
    require(result.coeffSignFlags.size() == 1,
            "CABAC residual luma4x4 significant one sign flag count");
    require(result.coeffSignFlags[0] == 0,
            "CABAC residual luma4x4 significant one sign flag");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag")),
            "CABAC residual luma4x4 significant one stage message");
}

void testReadResidualLuma4x4MultipleSignificantReverseScanOrder()
{
    BitReader reader(QByteArray::fromHex("0000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(250);
    contexts.setModel(85, {0, 1});
    contexts.setModel(134, {0, 1});
    contexts.setModel(135, {0, 0});
    contexts.setModel(136, {0, 1});
    contexts.setModel(166, {0, 0});
    contexts.setModel(168, {0, 1});
    contexts.setModel(248, {0, 0});
    contexts.setModel(249, {0, 0});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 multiple significant result");
    require(!result.complete, "CABAC residual luma4x4 multiple significant incomplete");
    require(result.significantScanIndices.size() == 3,
            "CABAC residual luma4x4 multiple significant scanned positions");
    require(result.significantScanIndices[0] == 0 && result.significantScanIndices[1] == 1
                && result.significantScanIndices[2] == 2,
            "CABAC residual luma4x4 multiple significant scan order");
    require(result.significantCoeffFlags[0] == 1 && result.significantCoeffFlags[1] == 0
                && result.significantCoeffFlags[2] == 1,
            "CABAC residual luma4x4 multiple significant values");
    require(result.lastSignificantScanIndices.size() == 2,
            "CABAC residual luma4x4 multiple significant last scan count");
    require(result.lastSignificantScanIndices[0] == 0 && result.lastSignificantScanIndices[1] == 2,
            "CABAC residual luma4x4 multiple significant last scan indices");
    require(result.lastSignificantCoeffFlags[0] == 0 && result.lastSignificantCoeffFlags[1] == 1,
            "CABAC residual luma4x4 multiple significant last values");
    require(result.coeffReverseScanIndices.size() == 2,
            "CABAC residual luma4x4 multiple significant reverse scan count");
    require(result.coeffReverseScanIndices[0] == 2 && result.coeffReverseScanIndices[1] == 0,
            "CABAC residual luma4x4 multiple significant reverse scan order");
    require(result.coeffAbsLevelScanIndices.size() == 2,
            "CABAC residual luma4x4 multiple significant coeff level count");
    require(result.coeffAbsLevelScanIndices[0] == 2 && result.coeffAbsLevelScanIndices[1] == 0,
            "CABAC residual luma4x4 multiple significant coeff level scan order");
    require(result.coeffAbsLevelInferredFinalFlags.size() == 2,
            "CABAC residual luma4x4 multiple significant inferred flag count");
    require(result.coeffAbsLevelInferredFinalFlags[0] == 0
                && result.coeffAbsLevelInferredFinalFlags[1] == 0,
            "CABAC residual luma4x4 multiple significant inferred flags");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 2,
            "CABAC residual luma4x4 multiple significant first prefix count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 0
                && result.coeffAbsLevelPrefixFirstBins[1] == 0,
            "CABAC residual luma4x4 multiple significant first prefix values");
    require(result.coeffAbsLevelPrefixFirstCtxIndices == QVector<int>({248, 249}),
            "CABAC residual luma4x4 magnitude-one coefficients adapt first contexts");
    require(result.coeffAbsLevelMinus1Values == QVector<int>({0, 0}),
            "CABAC residual luma4x4 magnitude-one coefficient values");
    require(result.coeffSignFlags.size() == 2,
            "CABAC residual luma4x4 multiple significant sign count");
    require(result.coeffSignFlags[0] == 0 && result.coeffSignFlags[1] == 0,
            "CABAC residual luma4x4 multiple significant sign values");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 multiple significant stops at second coefficient");
    require(result.incompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC residual luma4x4 multiple significant incomplete stage");
}

void testReadResidualLuma4x4CoeffAbsLevelNextBinZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts = initializedLuma4x4CoeffLevelContexts(1, 0);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 coeff level next-bin zero result");
    require(!result.complete, "CABAC residual luma4x4 coeff level next-bin zero incomplete");
    require(result.coeffAbsLevelScanIndices.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin zero scan count");
    require(result.coeffReverseScanIndices.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin zero reverse scan count");
    require(result.coeffReverseScanIndices[0] == 0,
            "CABAC residual luma4x4 coeff level next-bin zero reverse scan order");
    require(result.coeffAbsLevelInferredFinalFlags.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin zero inferred flag count");
    require(result.coeffAbsLevelInferredFinalFlags[0] == 0,
            "CABAC residual luma4x4 coeff level next-bin zero inferred flag");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin zero first bin count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC residual luma4x4 coeff level next-bin zero first bin value");
    require(result.coeffAbsLevelPrefixFirstCtxIndices == QVector<int>{248},
            "CABAC residual luma4x4 coeff level first context");
    require(result.coeffAbsLevelPrefixNextBins.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin zero bin count");
    require(result.coeffAbsLevelPrefixNextBins[0] == 0,
            "CABAC residual luma4x4 coeff level next-bin zero bin value");
    require(result.coeffAbsLevelPrefixNextCtxIndices == QVector<int>{252},
            "CABAC residual luma4x4 coeff level continuation context");
    require(result.coeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin zero terminated count");
    require(result.coeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC residual luma4x4 coeff level next-bin zero terminated value");
    require(result.coeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin zero one-count count");
    require(result.coeffAbsLevelPrefixOneCounts[0] == 1,
            "CABAC residual luma4x4 coeff level next-bin zero one-count value");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 coeff level next-bin zero incomplete block");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 coeff level next-bin zero incomplete scan");
    require(result.incompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC residual luma4x4 coeff level next-bin zero incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 coeff level next-bin zero diagnostic");
    require(result.coeffSignFlags.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin zero sign flag count");
    require(result.coeffSignFlags[0] == 0,
            "CABAC residual luma4x4 coeff level next-bin zero sign flag");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag")),
            "CABAC residual luma4x4 coeff level next-bin zero message");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag")),
            "CABAC residual luma4x4 coeff level next-bin zero sign message");
}

void testReadResidualLuma4x4GreaterThanOneAdaptsNextFirstContext()
{
    BitReader reader(QByteArray::fromHex("00000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(253);
    contexts.setModel(85, {0, 1});
    contexts.setModel(134, {0, 1});
    contexts.setModel(135, {0, 0});
    contexts.setModel(136, {0, 1});
    contexts.setModel(166, {0, 0});
    contexts.setModel(168, {0, 1});
    contexts.setModel(247, {0, 0});
    contexts.setModel(248, {0, 1});
    contexts.setModel(252, {0, 0});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);

    require(result.ok, "CABAC residual greater-than-one adaptive context result");
    require(result.coeffAbsLevelMinus1Values == QVector<int>({1, 0}),
            "CABAC residual greater-than-one adaptive coefficient values");
    require(result.coeffAbsLevelPrefixFirstCtxIndices == QVector<int>({248, 247}),
            "CABAC residual greater-than-one selects ctxIdx 247 for the next coefficient");
    require(result.coeffAbsLevelPrefixNextCtxIndices == QVector<int>{252},
            "CABAC residual first coefficient continuation uses state-derived ctxIdx 252");
}

void testReadResidualLuma4x4WalksAllCoefficientsAndSaturatesContext()
{
    BitReader reader(QByteArray::fromHex("00000000000000000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(257);
    contexts.setModel(85, {0, 1});
    for (int ctxIdx = 134; ctxIdx <= 138; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 1});
    }
    for (int ctxIdx = 166; ctxIdx <= 169; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
    contexts.setModel(170, {0, 1});
    contexts.setModel(247, {0, 1});
    contexts.setModel(248, {0, 1});
    for (int ctxIdx = 252; ctxIdx <= 256; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);

    require(result.ok, "CABAC residual full reverse-scan coefficient loop result");
    require(result.coeffReverseScanIndices == QVector<int>({4, 3, 2, 1, 0}),
            "CABAC residual full reverse-scan coefficient order");
    require(result.coeffAbsLevelScanIndices == result.coeffReverseScanIndices,
            "CABAC residual reads every saved reverse-scan coefficient");
    require(result.coeffAbsLevelMinus1Values == QVector<int>(5, 1),
            "CABAC residual full loop coefficient values");
    require(result.coeffAbsLevelPrefixFirstCtxIndices
                == QVector<int>({248, 247, 247, 247, 247}),
            "CABAC residual greater-than-one state saturates first-bin context");
    require(result.coeffAbsLevelPrefixNextCtxIndices
                == QVector<int>({252, 253, 254, 255, 256}),
            "CABAC residual continuation context reaches and saturates at ctxIdx 256");
    require(result.coeffSignFlags.size() == 5,
            "CABAC residual full loop reads every coefficient sign");
}

void testReadResidualLuma4x4CoeffAbsLevelNextBinOneIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts = initializedLuma4x4CoeffLevelContexts(1, 1);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 coeff level next-bin one result");
    require(!result.complete, "CABAC residual luma4x4 coeff level next-bin one incomplete");
    require(result.coeffAbsLevelScanIndices.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin one scan count");
    require(result.coeffReverseScanIndices.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin one reverse scan count");
    require(result.coeffReverseScanIndices[0] == 0,
            "CABAC residual luma4x4 coeff level next-bin one reverse scan order");
    require(result.coeffAbsLevelInferredFinalFlags.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin one inferred flag count");
    require(result.coeffAbsLevelInferredFinalFlags[0] == 0,
            "CABAC residual luma4x4 coeff level next-bin one inferred flag");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin one first bin count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC residual luma4x4 coeff level next-bin one first bin value");
    require(result.coeffAbsLevelPrefixNextBins.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin one bin count");
    require(result.coeffAbsLevelPrefixNextBins[0] == 1,
            "CABAC residual luma4x4 coeff level next-bin one bin value");
    require(result.coeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin one terminated count");
    require(result.coeffAbsLevelPrefixTerminatedFlags[0] == 0,
            "CABAC residual luma4x4 coeff level next-bin one terminated value");
    require(result.coeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin one one-count count");
    require(result.coeffAbsLevelPrefixOneCounts[0] == 2,
            "CABAC residual luma4x4 coeff level next-bin one one-count value");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level next-bin one no sign flag");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 coeff level next-bin one incomplete block");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 coeff level next-bin one incomplete scan");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 coeff level next-bin one incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 coeff level next-bin one diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("remaining coefficient level prefix")),
            "CABAC residual luma4x4 coeff level next-bin one message");
}

void testReadResidualLuma4x4CoeffAbsLevelThirdBinMissingKeepsBoundary()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts = initializedLuma4x4CoeffLevelContexts(1, 1);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 coeff level third-bin missing result");
    require(!result.complete, "CABAC residual luma4x4 coeff level third-bin missing incomplete");
    require(result.coeffAbsLevelScanIndices.size() == 1,
            "CABAC residual luma4x4 coeff level third-bin missing scan count");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 coeff level third-bin missing first bin count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC residual luma4x4 coeff level third-bin missing first bin value");
    require(result.coeffAbsLevelPrefixNextBins.size() == 1,
            "CABAC residual luma4x4 coeff level third-bin missing next bin count");
    require(result.coeffAbsLevelPrefixNextBins[0] == 1,
            "CABAC residual luma4x4 coeff level third-bin missing next bin value");
    require(result.coeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC residual luma4x4 coeff level third-bin missing terminated count");
    require(result.coeffAbsLevelPrefixTerminatedFlags[0] == 0,
            "CABAC residual luma4x4 coeff level third-bin missing terminated value");
    require(result.coeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 coeff level third-bin missing one-count count");
    require(result.coeffAbsLevelPrefixOneCounts[0] == 2,
            "CABAC residual luma4x4 coeff level third-bin missing one-count value");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin missing no sign flag");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 coeff level third-bin missing incomplete block");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 coeff level third-bin missing incomplete scan");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 coeff level third-bin missing incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 coeff level third-bin missing diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("covered prefix bins did not terminate")),
            "CABAC residual luma4x4 coeff level third-bin missing message");
}

void testReadResidualLuma4x4CoeffAbsLevelThirdBinZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts = initializedLuma4x4CoeffLevelContexts(1, 1, 0);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 coeff level third-bin zero result");
    require(!result.complete, "CABAC residual luma4x4 coeff level third-bin zero incomplete");
    require(result.coeffAbsLevelScanIndices.size() == 1,
            "CABAC residual luma4x4 coeff level third-bin zero scan count");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 coeff level third-bin zero first bin count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC residual luma4x4 coeff level third-bin zero first bin value");
    require(result.coeffAbsLevelPrefixNextBins.size() == 2,
            "CABAC residual luma4x4 coeff level third-bin zero next bin count");
    require(result.coeffAbsLevelPrefixNextBins[0] == 1
                && result.coeffAbsLevelPrefixNextBins[1] == 0,
            "CABAC residual luma4x4 coeff level third-bin zero next bin values");
    require(result.coeffSignFlags.size() == 1,
            "CABAC residual luma4x4 coeff level third-bin zero sign flag count");
    require(result.coeffSignFlags[0] == 0,
            "CABAC residual luma4x4 coeff level third-bin zero sign flag");
    require(result.coeffAbsLevelReadyForValueFlags.size() == result.coeffAbsLevelScanIndices.size(),
            "CABAC residual luma4x4 coeff level third-bin zero ready flag alignment");
    require(result.coeffAbsLevelReadyForValueFlags[0] == 0,
            "CABAC residual luma4x4 coeff level third-bin zero ready flag value");
    require(result.coeffAbsLevelReadyPrefixOneCounts.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no ready prefix one-count");
    require(result.coeffAbsLevelReadySuffixBins.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no ready suffix bins");
    require(result.coeffAbsLevelRemainingInputBins.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no remaining input bins");
    require(result.coeffAbsLevelRemainingInputBinCounts.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no remaining input count");
    require(result.coeffAbsLevelReadyRemainingInputBins.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no ready remaining input bins");
    require(result.coeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no ready additional pre-UEG0 parsing flag");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no ready additional pre-UEG0 target");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no ready additional pre-UEG0 remaining prefix bins");
    require(result.coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no ready can-continue pre-UEG0 prefix parsing flag");
    require(result.coeffAbsLevelValueInputCompleteFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no value input complete flag");
    require(result.coeffAbsLevelFixedInputRecognizedFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no fixed input recognized flag");
    require(result.coeffAbsLevelPreUeg0RemainingInputFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no pre-UEG0 remaining input flag");
    require(result.coeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin zero no additional pre-UEG0 parsing flag");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 coeff level third-bin zero incomplete block");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 coeff level third-bin zero incomplete scan");
    require(result.incompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC residual luma4x4 coeff level third-bin zero incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 coeff level third-bin zero diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag")),
            "CABAC residual luma4x4 coeff level third-bin zero message");
}

void testReadResidualLuma4x4CoeffAbsLevelThirdBinOneIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts = initializedLuma4x4CoeffLevelContexts(1, 1, 1);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 coeff level third-bin one result");
    require(!result.complete, "CABAC residual luma4x4 coeff level third-bin one incomplete");
    require(result.coeffAbsLevelScanIndices.size() == 1,
            "CABAC residual luma4x4 coeff level third-bin one scan count");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 coeff level third-bin one first bin count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC residual luma4x4 coeff level third-bin one first bin value");
    require(result.coeffAbsLevelPrefixNextBins.size() == 2,
            "CABAC residual luma4x4 coeff level third-bin one next bin count");
    require(result.coeffAbsLevelPrefixNextBins[0] == 1
                && result.coeffAbsLevelPrefixNextBins[1] == 1,
            "CABAC residual luma4x4 coeff level third-bin one next bin values");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no sign flag");
    require(result.coeffAbsLevelSuffixBins.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no suffix bins");
    require(result.coeffAbsLevelReadyForValueFlags.size() == result.coeffAbsLevelScanIndices.size(),
            "CABAC residual luma4x4 coeff level third-bin one ready flag alignment");
    require(result.coeffAbsLevelReadyForValueFlags[0] == 0,
            "CABAC residual luma4x4 coeff level third-bin one ready flag value");
    require(result.coeffAbsLevelReadyPrefixOneCounts.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no ready prefix one-count");
    require(result.coeffAbsLevelReadySuffixBins.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no ready suffix bins");
    require(result.coeffAbsLevelRemainingInputBins.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no remaining input bins");
    require(result.coeffAbsLevelRemainingInputBinCounts.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no remaining input count");
    require(result.coeffAbsLevelReadyRemainingInputBins.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no ready remaining input bins");
    require(result.coeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no ready additional pre-UEG0 parsing flag");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no ready additional pre-UEG0 target");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no ready additional pre-UEG0 remaining prefix bins");
    require(result.coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no ready can-continue pre-UEG0 prefix parsing flag");
    require(result.coeffAbsLevelValueInputCompleteFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no value input complete flag");
    require(result.coeffAbsLevelFixedInputRecognizedFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no fixed input recognized flag");
    require(result.coeffAbsLevelPreUeg0RemainingInputFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no pre-UEG0 remaining input flag");
    require(result.coeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level third-bin one no additional pre-UEG0 parsing flag");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 coeff level third-bin one incomplete block");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 coeff level third-bin one incomplete scan");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 coeff level third-bin one incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 coeff level third-bin one diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("covered prefix bins did not terminate")),
            "CABAC residual luma4x4 coeff level third-bin one message");
}

void testReadResidualLuma4x4CoeffAbsLevelFourthBinZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFourthContext(1, 1, 1, 0);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 coeff level fourth-bin zero result");
    require(!result.complete, "CABAC residual luma4x4 coeff level fourth-bin zero incomplete");
    require(result.coeffAbsLevelScanIndices.size() == 1,
            "CABAC residual luma4x4 coeff level fourth-bin zero scan count");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 coeff level fourth-bin zero first bin count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC residual luma4x4 coeff level fourth-bin zero first bin value");
    require(result.coeffAbsLevelPrefixNextBins.size() == 3,
            "CABAC residual luma4x4 coeff level fourth-bin zero next bin count");
    require(result.coeffAbsLevelPrefixNextBins[0] == 1
                && result.coeffAbsLevelPrefixNextBins[1] == 1
                && result.coeffAbsLevelPrefixNextBins[2] == 0,
            "CABAC residual luma4x4 coeff level fourth-bin zero next bin values");
    require(result.coeffSignFlags.size() == 1,
            "CABAC residual luma4x4 coeff level fourth-bin zero sign flag count");
    require(result.coeffSignFlags[0] == 0,
            "CABAC residual luma4x4 coeff level fourth-bin zero sign flag");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 coeff level fourth-bin zero incomplete block");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 coeff level fourth-bin zero incomplete scan");
    require(result.incompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC residual luma4x4 coeff level fourth-bin zero incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 coeff level fourth-bin zero diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag")),
            "CABAC residual luma4x4 coeff level fourth-bin zero message");
}

void testReadResidualLuma4x4CoeffAbsLevelFourthBinOneIncomplete()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFourthContext(1, 1, 1, 1);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 coeff level fourth-bin one result");
    require(!result.complete, "CABAC residual luma4x4 coeff level fourth-bin one incomplete");
    require(result.coeffAbsLevelScanIndices.size() == 1,
            "CABAC residual luma4x4 coeff level fourth-bin one scan count");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 coeff level fourth-bin one first bin count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC residual luma4x4 coeff level fourth-bin one first bin value");
    require(result.coeffAbsLevelPrefixNextBins.size() == 3,
            "CABAC residual luma4x4 coeff level fourth-bin one next bin count");
    require(result.coeffAbsLevelPrefixNextBins[0] == 1
                && result.coeffAbsLevelPrefixNextBins[1] == 1
                && result.coeffAbsLevelPrefixNextBins[2] == 1,
            "CABAC residual luma4x4 coeff level fourth-bin one next bin values");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level fourth-bin one no sign flag");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 coeff level fourth-bin one incomplete block");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 coeff level fourth-bin one incomplete scan");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 coeff level fourth-bin one incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 coeff level fourth-bin one diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("covered prefix bins did not terminate")),
            "CABAC residual luma4x4 coeff level fourth-bin one message");
}

void testReadResidualLuma4x4CoeffAbsLevelFourthBinDecodeFailure()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFourthContext(1, 1, 1, 0);
    contexts.setModel(254, {64, 0});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(!result.ok, "CABAC residual luma4x4 coeff level fourth-bin decode failure fails");
    require(!result.complete,
            "CABAC residual luma4x4 coeff level fourth-bin decode failure incomplete");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 coeff level fourth-bin decode failure keeps first bin");
    require(result.coeffAbsLevelPrefixNextBins.size() == 2,
            "CABAC residual luma4x4 coeff level fourth-bin decode failure keeps prior next bins");
    require(result.coeffAbsLevelPrefixNextBins[0] == 1
                && result.coeffAbsLevelPrefixNextBins[1] == 1,
            "CABAC residual luma4x4 coeff level fourth-bin decode failure prior next values");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level fourth-bin decode failure no sign flag");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 coeff level fourth-bin decode failure incomplete block");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 coeff level fourth-bin decode failure incomplete scan");
    require(result.diagnosticCode == QStringLiteral("cabac_bin_decode_failed"),
            "CABAC residual luma4x4 coeff level fourth-bin decode failure diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("fourth prefix bin")),
            "CABAC residual luma4x4 coeff level fourth-bin decode failure message");
}

void testReadResidualLuma4x4CoeffAbsLevelFifthBinZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFifthContext(1, 1, 1, 1, 0);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 coeff level fifth-bin zero result");
    require(!result.complete, "CABAC residual luma4x4 coeff level fifth-bin zero incomplete");
    require(result.coeffAbsLevelScanIndices.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero scan count");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero first bin count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero first bin value");
    require(result.coeffAbsLevelPrefixNextBins.size() == 4,
            "CABAC residual luma4x4 coeff level fifth-bin zero next bin count");
    require(result.coeffAbsLevelPrefixNextBins[0] == 1
                && result.coeffAbsLevelPrefixNextBins[1] == 1
                && result.coeffAbsLevelPrefixNextBins[2] == 1
                && result.coeffAbsLevelPrefixNextBins[3] == 0,
            "CABAC residual luma4x4 coeff level fifth-bin zero next bin values");
    require(result.coeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero terminated count");
    require(result.coeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero terminated value");
    require(result.coeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero one-count count");
    require(result.coeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC residual luma4x4 coeff level fifth-bin zero one-count value");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin zero no sign flag");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 coeff level fifth-bin zero incomplete block");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 coeff level fifth-bin zero incomplete scan");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 coeff level fifth-bin zero incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 coeff level fifth-bin zero diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("suffix bypass bin")),
            "CABAC residual luma4x4 coeff level fifth-bin zero message");
    require(result.diagnosticMessage.contains(QStringLiteral("prefix one-count 4")),
            "CABAC residual luma4x4 coeff level fifth-bin zero one-count message");
    require(result.diagnosticMessage.contains(QStringLiteral("computing coeff_abs_level_minus1 is not implemented")),
            "CABAC residual luma4x4 coeff level fifth-bin zero compute-not-implemented message");
    require(result.coeffAbsLevelSuffixBins.size() == 4,
            "CABAC residual luma4x4 coeff level fifth-bin zero suffix count");
    require(result.coeffAbsLevelSuffixBins[0] == 0
                && result.coeffAbsLevelSuffixBins[1] == 0
                && result.coeffAbsLevelSuffixBins[2] == 0
                && result.coeffAbsLevelSuffixBins[3] == 0,
            "CABAC residual luma4x4 coeff level fifth-bin zero suffix values");
    require(result.coeffAbsLevelSuffixBinCounts.size() == result.coeffAbsLevelScanIndices.size(),
            "CABAC residual luma4x4 coeff level fifth-bin zero suffix count alignment");
    require(result.coeffAbsLevelSuffixBinCounts[0] == 4,
            "CABAC residual luma4x4 coeff level fifth-bin zero suffix count value");
    require(result.coeffAbsLevelReadyForValueFlags.size() == result.coeffAbsLevelScanIndices.size(),
            "CABAC residual luma4x4 coeff level fifth-bin zero ready flag alignment");
    require(result.coeffAbsLevelReadyForValueFlags[0] == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready flag value");
    require(result.coeffAbsLevelReadyPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready prefix one-count count");
    require(result.coeffAbsLevelReadyPrefixOneCounts[0] == 4,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready prefix one-count value");
    require(result.coeffAbsLevelReadySuffixBins.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready suffix group count");
    require(result.coeffAbsLevelReadySuffixBins[0].size() == 4,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready suffix group size");
    require(result.coeffAbsLevelReadySuffixBins[0][0] == 0
                && result.coeffAbsLevelReadySuffixBins[0][1] == 0
                && result.coeffAbsLevelReadySuffixBins[0][2] == 0
                && result.coeffAbsLevelReadySuffixBins[0][3] == 0,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready suffix values");
    require(result.coeffAbsLevelRemainingInputBins.size() == 4,
            "CABAC residual luma4x4 coeff level fifth-bin zero remaining input count");
    require(result.coeffAbsLevelRemainingInputBins[0] == 0
                && result.coeffAbsLevelRemainingInputBins[1] == 0
                && result.coeffAbsLevelRemainingInputBins[2] == 0
                && result.coeffAbsLevelRemainingInputBins[3] == 0,
            "CABAC residual luma4x4 coeff level fifth-bin zero remaining input values");
    require(result.coeffAbsLevelRemainingInputBinCounts.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero remaining input bin-count count");
    require(result.coeffAbsLevelRemainingInputBinCounts[0] == 4,
            "CABAC residual luma4x4 coeff level fifth-bin zero remaining input bin-count value");
    require(result.coeffAbsLevelReadyRemainingInputBins.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready remaining input group count");
    require(result.coeffAbsLevelReadyRemainingInputBins[0].size() == 4,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready remaining input group size");
    require(result.coeffAbsLevelReadyRemainingInputBins[0][0] == 0
                && result.coeffAbsLevelReadyRemainingInputBins[0][1] == 0
                && result.coeffAbsLevelReadyRemainingInputBins[0][2] == 0
                && result.coeffAbsLevelReadyRemainingInputBins[0][3] == 0,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready remaining input values");
    require(result.coeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready additional pre-UEG0 parsing count");
    require(result.coeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags[0] == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready additional pre-UEG0 parsing value");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready additional pre-UEG0 target count");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts[0]
                == h264CabacCoeffAbsLevelMinus1Ueg0Cutoff(),
            "CABAC residual luma4x4 coeff level fifth-bin zero ready additional pre-UEG0 target value");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready additional pre-UEG0 remaining prefix bin count");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts[0] == 10,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready additional pre-UEG0 remaining prefix bins");
    require(result.coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready can-continue pre-UEG0 prefix parsing count");
    require(result.coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags[0] == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero ready can-continue pre-UEG0 prefix parsing value");
    require(result.coeffAbsLevelValueInputCompleteFlags.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero value input complete count");
    require(result.coeffAbsLevelValueInputCompleteFlags[0] == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero value input complete value");
    require(result.coeffAbsLevelFixedInputRecognizedFlags.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero fixed input recognized count");
    require(result.coeffAbsLevelFixedInputRecognizedFlags[0] == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero fixed input recognized value");
    require(result.coeffAbsLevelPreUeg0RemainingInputFlags.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero pre-UEG0 remaining input count");
    require(result.coeffAbsLevelPreUeg0RemainingInputFlags[0] == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero pre-UEG0 remaining input value");
    require(result.coeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero additional pre-UEG0 parsing count");
    require(result.coeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags[0] == 1,
            "CABAC residual luma4x4 coeff level fifth-bin zero additional pre-UEG0 parsing value");
    require(!h264CabacCoeffAbsLevelMinus1CanComputeFromUeg0Suffix(
                {result.coeffAbsLevelReadyPrefixOneCounts[0],
                 result.coeffAbsLevelReadyRemainingInputBins[0]}),
            "CABAC residual luma4x4 coeff level fifth-bin zero cannot compute from UEG0 suffix");
}

void testReadResidualLuma4x4CoeffAbsLevelSixthBinOneIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFifthContext(1, 1, 1, 1, 1);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 coeff level fifth-bin one result");
    require(!result.complete, "CABAC residual luma4x4 coeff level fifth-bin one incomplete");
    require(result.coeffAbsLevelScanIndices.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin one scan count");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin one first bin count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 1,
            "CABAC residual luma4x4 coeff level fifth-bin one first bin value");
    require(result.coeffAbsLevelPrefixNextBins.size() == 5,
            "CABAC residual luma4x4 coeff level sixth-bin one next bin count");
    require(result.coeffAbsLevelPrefixNextBins[0] == 1
                && result.coeffAbsLevelPrefixNextBins[1] == 1
                && result.coeffAbsLevelPrefixNextBins[2] == 1
                && result.coeffAbsLevelPrefixNextBins[3] == 1
                && result.coeffAbsLevelPrefixNextBins[4] == 1,
            "CABAC residual luma4x4 coeff level sixth-bin one next bin values");
    require(result.coeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin one terminated count");
    require(result.coeffAbsLevelPrefixTerminatedFlags[0] == 0,
            "CABAC residual luma4x4 coeff level fifth-bin one terminated value");
    require(result.coeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 coeff level fifth-bin one one-count count");
    require(result.coeffAbsLevelPrefixOneCounts[0] == 6,
            "CABAC residual luma4x4 coeff level sixth-bin one one-count value");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no sign flag");
    require(result.coeffAbsLevelSuffixBins.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no suffix bins");
    require(result.coeffAbsLevelReadyForValueFlags.size() == result.coeffAbsLevelScanIndices.size(),
            "CABAC residual luma4x4 coeff level fifth-bin one ready flag alignment");
    require(result.coeffAbsLevelReadyForValueFlags[0] == 0,
            "CABAC residual luma4x4 coeff level fifth-bin one ready flag value");
    require(result.coeffAbsLevelReadyPrefixOneCounts.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no ready prefix one-count");
    require(result.coeffAbsLevelReadySuffixBins.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no ready suffix bins");
    require(result.coeffAbsLevelRemainingInputBins.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no remaining input bins");
    require(result.coeffAbsLevelRemainingInputBinCounts.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no remaining input count");
    require(result.coeffAbsLevelReadyRemainingInputBins.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no ready remaining input bins");
    require(result.coeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no ready additional pre-UEG0 parsing flag");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no ready additional pre-UEG0 target");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no ready additional pre-UEG0 remaining prefix bins");
    require(result.coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no ready can-continue pre-UEG0 prefix parsing flag");
    require(result.coeffAbsLevelValueInputCompleteFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no value input complete flag");
    require(result.coeffAbsLevelFixedInputRecognizedFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no fixed input recognized flag");
    require(result.coeffAbsLevelPreUeg0RemainingInputFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no pre-UEG0 remaining input flag");
    require(result.coeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level fifth-bin one no additional pre-UEG0 parsing flag");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 coeff level fifth-bin one incomplete block");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 coeff level fifth-bin one incomplete scan");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 coeff level fifth-bin one incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 coeff level fifth-bin one diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("covered prefix bins did not terminate")),
            "CABAC residual luma4x4 coeff level fifth-bin one message");
}

void testReadResidualLuma4x4CoeffAbsLevelSixthBinZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("0100000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);
    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFifthContext(1, 1, 1, 1, 1);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);

    require(result.ok, "CABAC residual luma4x4 coeff level sixth-bin zero result");
    require(!result.complete, "CABAC residual luma4x4 coeff level sixth-bin zero incomplete");
    require(result.coeffAbsLevelPrefixNextBins.size() == 5,
            "CABAC residual luma4x4 coeff level sixth-bin zero next bin count");
    require(result.coeffAbsLevelPrefixNextBins[0] == 1
                && result.coeffAbsLevelPrefixNextBins[1] == 1
                && result.coeffAbsLevelPrefixNextBins[2] == 1
                && result.coeffAbsLevelPrefixNextBins[3] == 1
                && result.coeffAbsLevelPrefixNextBins[4] == 0,
            "CABAC residual luma4x4 coeff level sixth-bin zero next bin values");
    require(result.coeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC residual luma4x4 coeff level sixth-bin zero terminated count");
    require(result.coeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC residual luma4x4 coeff level sixth-bin zero terminated value");
    require(result.coeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 coeff level sixth-bin zero one-count count");
    require(result.coeffAbsLevelPrefixOneCounts[0] == 5,
            "CABAC residual luma4x4 coeff level sixth-bin zero one-count value");
    require(result.coeffAbsLevelReadyPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 coeff level sixth-bin zero ready prefix count");
    require(result.coeffAbsLevelReadyPrefixOneCounts[0] == 5,
            "CABAC residual luma4x4 coeff level sixth-bin zero ready prefix value");
    require(result.coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.size() == 1,
            "CABAC residual luma4x4 coeff level sixth-bin zero continuation count");
    require(result.coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags[0] == 1,
            "CABAC residual luma4x4 coeff level sixth-bin zero continuation value");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts.size() == 1,
            "CABAC residual luma4x4 coeff level sixth-bin zero remaining prefix count");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts[0] == 9,
            "CABAC residual luma4x4 coeff level sixth-bin zero remaining prefix value");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 coeff level sixth-bin zero incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 coeff level sixth-bin zero diagnostic");
}

void testReadResidualLuma4x4CoeffAbsLevelTerminatedPrefixKeepsSuffixInputs()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFifthContext(1, 1, 1, 1, 0);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 suffix input prefix result");
    require(!result.complete, "CABAC residual luma4x4 suffix input prefix incomplete");
    require(result.coeffAbsLevelScanIndices.size() == 1,
            "CABAC residual luma4x4 suffix input scan count");
    require(result.coeffAbsLevelInferredFinalFlags.size() == result.coeffAbsLevelScanIndices.size(),
            "CABAC residual luma4x4 suffix input inferred alignment");
    require(result.coeffAbsLevelPrefixTerminatedFlags.size() == result.coeffAbsLevelScanIndices.size(),
            "CABAC residual luma4x4 suffix input terminated alignment");
    require(result.coeffAbsLevelPrefixOneCounts.size() == result.coeffAbsLevelScanIndices.size(),
            "CABAC residual luma4x4 suffix input one-count alignment");
    require(result.coeffAbsLevelScanIndices[0] == 0,
            "CABAC residual luma4x4 suffix input scan index");
    require(result.coeffAbsLevelInferredFinalFlags[0] == 0,
            "CABAC residual luma4x4 suffix input inferred flag");
    require(result.coeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC residual luma4x4 suffix input terminated flag");
    require(result.coeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC residual luma4x4 suffix input prefix one-count");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 suffix input no sign flag");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 suffix input current stop stage");
}

void testReadResidualLuma4x4CoeffAbsLevelTerminatedLargePrefixStopsBeforeSign()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFifthContext(1, 1, 1, 1, 0);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 large-prefix suffix boundary result");
    require(!result.complete, "CABAC residual luma4x4 large-prefix suffix boundary incomplete");
    require(result.coeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC residual luma4x4 large-prefix suffix boundary terminated count");
    require(result.coeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC residual luma4x4 large-prefix suffix boundary terminated value");
    require(result.coeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 large-prefix suffix boundary one-count count");
    require(result.coeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC residual luma4x4 large-prefix suffix boundary one-count value");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 large-prefix suffix boundary stops before sign flag");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 large-prefix suffix boundary stop stage");
    require(result.diagnosticMessage.contains(QStringLiteral("suffix bypass bin")),
            "CABAC residual luma4x4 large-prefix suffix boundary message");
    require(result.diagnosticMessage.contains(QStringLiteral("prefix one-count 4")),
            "CABAC residual luma4x4 large-prefix suffix boundary one-count message");
}

void testReadResidualLuma4x4CoeffAbsLevelFirstSuffixBypassBinIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFifthContext(1, 1, 1, 1, 0);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 first suffix bypass bin result");
    require(!result.complete, "CABAC residual luma4x4 first suffix bypass bin incomplete");
    require(result.coeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC residual luma4x4 first suffix bypass bin terminated count");
    require(result.coeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC residual luma4x4 first suffix bypass bin terminated value");
    require(result.coeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 first suffix bypass bin one-count count");
    require(result.coeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC residual luma4x4 first suffix bypass bin one-count value");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 first suffix bypass bin stops before sign flag");
    require(result.coeffAbsLevelSuffixBins.size() == 4,
            "CABAC residual luma4x4 first suffix bypass bin count");
    require(result.coeffAbsLevelSuffixBins[0] == 0
                && result.coeffAbsLevelSuffixBins[1] == 0
                && result.coeffAbsLevelSuffixBins[2] == 0
                && result.coeffAbsLevelSuffixBins[3] == 0,
            "CABAC residual luma4x4 first suffix bypass bin values");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 first suffix bypass bin stage");
    require(result.diagnosticMessage.contains(QStringLiteral("suffix bypass bin")),
            "CABAC residual luma4x4 first suffix bypass bin message");
}

void testReadResidualLuma4x4CoeffAbsLevelSecondSuffixBypassBinIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFifthContext(1, 1, 1, 1, 0);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 second suffix bypass bin result");
    require(!result.complete, "CABAC residual luma4x4 second suffix bypass bin incomplete");
    require(result.coeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC residual luma4x4 second suffix bypass bin terminated count");
    require(result.coeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC residual luma4x4 second suffix bypass bin terminated value");
    require(result.coeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 second suffix bypass bin one-count count");
    require(result.coeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC residual luma4x4 second suffix bypass bin one-count value");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 second suffix bypass bin stops before sign flag");
    require(result.coeffAbsLevelSuffixBins.size() == 4,
            "CABAC residual luma4x4 second suffix bypass bin count");
    require(result.coeffAbsLevelSuffixBins[0] == 0
                && result.coeffAbsLevelSuffixBins[1] == 0
                && result.coeffAbsLevelSuffixBins[2] == 0
                && result.coeffAbsLevelSuffixBins[3] == 0,
            "CABAC residual luma4x4 second suffix bypass bin values");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 second suffix bypass bin stage");
    require(result.diagnosticMessage.contains(QStringLiteral("suffix bypass bin")),
            "CABAC residual luma4x4 second suffix bypass bin message");
}

void testReadResidualLuma4x4CoeffAbsLevelThirdSuffixBypassBinIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFifthContext(1, 1, 1, 1, 0);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 third suffix bypass bin result");
    require(!result.complete, "CABAC residual luma4x4 third suffix bypass bin incomplete");
    require(result.coeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC residual luma4x4 third suffix bypass bin terminated count");
    require(result.coeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC residual luma4x4 third suffix bypass bin terminated value");
    require(result.coeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 third suffix bypass bin one-count count");
    require(result.coeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC residual luma4x4 third suffix bypass bin one-count value");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 third suffix bypass bin stops before sign flag");
    require(result.coeffAbsLevelSuffixBins.size() == 4,
            "CABAC residual luma4x4 third suffix bypass bin count");
    require(result.coeffAbsLevelSuffixBins[0] == 0
                && result.coeffAbsLevelSuffixBins[1] == 0
                && result.coeffAbsLevelSuffixBins[2] == 0
                && result.coeffAbsLevelSuffixBins[3] == 0,
            "CABAC residual luma4x4 third suffix bypass bin values");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 third suffix bypass bin stage");
    require(result.diagnosticMessage.contains(QStringLiteral("suffix bypass bin")),
            "CABAC residual luma4x4 third suffix bypass bin message");
}

void testReadResidualLuma4x4CoeffAbsLevelFourthSuffixBypassBinIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFifthContext(1, 1, 1, 1, 0);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 fourth suffix bypass bin result");
    require(!result.complete, "CABAC residual luma4x4 fourth suffix bypass bin incomplete");
    require(result.coeffAbsLevelPrefixTerminatedFlags.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin terminated count");
    require(result.coeffAbsLevelPrefixTerminatedFlags[0] == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin terminated value");
    require(result.coeffAbsLevelPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin one-count count");
    require(result.coeffAbsLevelPrefixOneCounts[0] == 4,
            "CABAC residual luma4x4 fourth suffix bypass bin one-count value");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 fourth suffix bypass bin stops before sign flag");
    require(result.coeffAbsLevelSuffixBins.size() == 4,
            "CABAC residual luma4x4 fourth suffix bypass bin count");
    require(result.coeffAbsLevelSuffixBins[0] == 0
                && result.coeffAbsLevelSuffixBins[1] == 0
                && result.coeffAbsLevelSuffixBins[2] == 0
                && result.coeffAbsLevelSuffixBins[3] == 0,
            "CABAC residual luma4x4 fourth suffix bypass bin values");
    require(result.coeffAbsLevelSuffixBinCounts.size() == result.coeffAbsLevelScanIndices.size(),
            "CABAC residual luma4x4 fourth suffix bypass bin count alignment");
    require(result.coeffAbsLevelSuffixBinCounts[0] == 4,
            "CABAC residual luma4x4 fourth suffix bypass bin count value");
    require(result.coeffAbsLevelReadyForValueFlags.size() == result.coeffAbsLevelScanIndices.size(),
            "CABAC residual luma4x4 fourth suffix bypass bin ready flag alignment");
    require(result.coeffAbsLevelReadyForValueFlags[0] == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin ready flag value");
    require(result.coeffAbsLevelReadyPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin ready prefix one-count count");
    require(result.coeffAbsLevelReadyPrefixOneCounts[0] == 4,
            "CABAC residual luma4x4 fourth suffix bypass bin ready prefix one-count value");
    require(result.coeffAbsLevelReadySuffixBins.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin ready suffix group count");
    require(result.coeffAbsLevelReadySuffixBins[0].size() == 4,
            "CABAC residual luma4x4 fourth suffix bypass bin ready suffix group size");
    require(result.coeffAbsLevelReadySuffixBins[0][0] == 0
                && result.coeffAbsLevelReadySuffixBins[0][1] == 0
                && result.coeffAbsLevelReadySuffixBins[0][2] == 0
                && result.coeffAbsLevelReadySuffixBins[0][3] == 0,
            "CABAC residual luma4x4 fourth suffix bypass bin ready suffix values");
    require(result.coeffAbsLevelRemainingInputBins.size() == 4,
            "CABAC residual luma4x4 fourth suffix bypass bin remaining input count");
    require(result.coeffAbsLevelRemainingInputBinCounts.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin remaining input bin-count count");
    require(result.coeffAbsLevelRemainingInputBinCounts[0] == 4,
            "CABAC residual luma4x4 fourth suffix bypass bin remaining input bin-count value");
    require(result.coeffAbsLevelReadyRemainingInputBins.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin ready remaining input group count");
    require(result.coeffAbsLevelReadyRemainingInputBins[0].size() == 4,
            "CABAC residual luma4x4 fourth suffix bypass bin ready remaining input group size");
    require(result.coeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin ready additional pre-UEG0 parsing count");
    require(result.coeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags[0] == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin ready additional pre-UEG0 parsing value");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin ready additional pre-UEG0 target count");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts[0]
                == h264CabacCoeffAbsLevelMinus1Ueg0Cutoff(),
            "CABAC residual luma4x4 fourth suffix bypass bin ready additional pre-UEG0 target value");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin ready additional pre-UEG0 remaining prefix bin count");
    require(result.coeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts[0] == 10,
            "CABAC residual luma4x4 fourth suffix bypass bin ready additional pre-UEG0 remaining prefix bins");
    require(result.coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin ready can-continue pre-UEG0 prefix parsing count");
    require(result.coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags[0] == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin ready can-continue pre-UEG0 prefix parsing value");
    require(result.coeffAbsLevelValueInputCompleteFlags.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin value input complete count");
    require(result.coeffAbsLevelValueInputCompleteFlags[0] == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin value input complete value");
    require(result.coeffAbsLevelFixedInputRecognizedFlags.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin fixed input recognized count");
    require(result.coeffAbsLevelFixedInputRecognizedFlags[0] == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin fixed input recognized value");
    require(result.coeffAbsLevelPreUeg0RemainingInputFlags.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin pre-UEG0 remaining input count");
    require(result.coeffAbsLevelPreUeg0RemainingInputFlags[0] == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin pre-UEG0 remaining input value");
    require(result.coeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags.size() == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin additional pre-UEG0 parsing count");
    require(result.coeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags[0] == 1,
            "CABAC residual luma4x4 fourth suffix bypass bin additional pre-UEG0 parsing value");
    require(result.incompleteStage == QStringLiteral("coeff_abs_level_minus1"),
            "CABAC residual luma4x4 fourth suffix bypass bin stage");
    require(result.diagnosticMessage.contains(QStringLiteral("fourth suffix bypass bin")),
            "CABAC residual luma4x4 fourth suffix bypass bin message");
}

void testReadResidualLuma4x4CoeffAbsLevelNextBinMissingContext()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(249);
    contexts.setModel(85, {0, 1});
    contexts.setModel(134, {0, 1});
    contexts.setModel(166, {0, 1});
    contexts.setModel(248, {0, 1});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(!result.ok, "CABAC residual luma4x4 coeff level next-bin missing context fails");
    require(!result.complete, "CABAC residual luma4x4 coeff level next-bin missing context incomplete");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 1,
            "CABAC residual luma4x4 coeff level next-bin missing context keeps first bin");
    require(result.coeffAbsLevelPrefixNextBins.isEmpty(),
            "CABAC residual luma4x4 coeff level next-bin missing context has no next bin");
    require(result.coeffSignFlags.isEmpty(),
            "CABAC residual luma4x4 coeff level next-bin missing context has no sign flag");
    require(result.diagnosticCode == QStringLiteral("cabac_context_uninitialized"),
            "CABAC residual luma4x4 coeff level next-bin missing context diagnostic");
    require(result.diagnosticMessage.contains(QStringLiteral("252")),
            "CABAC residual luma4x4 coeff level next-bin missing context ctxIdx message");
}

void testReadResidualLuma4x4LastSignificantZeroIncomplete()
{
    BitReader reader(QByteArray::fromHex("000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(250);
    contexts.setModel(85, {0, 1});
    setLuma4x4SignificantZeroContexts(contexts);
    contexts.setModel(134, {0, 1});
    contexts.setModel(166, {0, 0});
    contexts.setModel(248, {0, 0});
    contexts.setModel(249, {0, 0});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 last-significant zero partial result");
    require(!result.complete, "CABAC residual luma4x4 last-significant zero incomplete");
    require(result.significantCoeffFlags.size() == 15,
            "CABAC residual luma4x4 last-significant zero significant count");
    require(result.significantCoeffFlags[0] == 1,
            "CABAC residual luma4x4 last-significant zero significant value");
    require(result.significantCoeffFlags[14] == 0,
            "CABAC residual luma4x4 last-significant zero final explicit significant value");
    require(result.lastSignificantScanIndices.size() == 1,
            "CABAC residual luma4x4 last-significant zero scan count");
    require(result.lastSignificantCoeffFlags[0] == 0,
            "CABAC residual luma4x4 last-significant zero flag value");
    require(result.coeffAbsLevelScanIndices.size() == 2,
            "CABAC residual luma4x4 last-significant zero coeff level count");
    require(result.coeffReverseScanIndices.size() == 2,
            "CABAC residual luma4x4 last-significant zero reverse scan count");
    require(result.coeffReverseScanIndices[0] == 15 && result.coeffReverseScanIndices[1] == 0,
            "CABAC residual luma4x4 last-significant zero reverse scan order");
    require(result.coeffAbsLevelScanIndices[0] == 15 && result.coeffAbsLevelScanIndices[1] == 0,
            "CABAC residual luma4x4 last-significant zero coeff level scan order");
    require(result.coeffAbsLevelInferredFinalFlags.size() == 2,
            "CABAC residual luma4x4 last-significant zero inferred flag count");
    require(result.coeffAbsLevelInferredFinalFlags[0] == 1
                && result.coeffAbsLevelInferredFinalFlags[1] == 0,
            "CABAC residual luma4x4 last-significant zero inferred flags");
    require(result.coeffAbsLevelPrefixFirstBins.size() == 2,
            "CABAC residual luma4x4 last-significant zero first prefix count");
    require(result.coeffAbsLevelPrefixFirstBins[0] == 0
                && result.coeffAbsLevelPrefixFirstBins[1] == 0,
            "CABAC residual luma4x4 last-significant zero first prefix values");
    require(result.coeffAbsLevelPrefixFirstCtxIndices == QVector<int>({248, 249}),
            "CABAC residual luma4x4 inferred-final path adapts first contexts");
    require(result.incompleteBlockIndex == 12,
            "CABAC residual luma4x4 last-significant zero incomplete block index");
    require(result.incompleteScanIndex == 0,
            "CABAC residual luma4x4 last-significant zero incomplete scan index");
    require(result.incompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC residual luma4x4 last-significant zero incomplete stage");
    require(result.diagnosticCode == QStringLiteral("cabac_residual_incomplete"),
            "CABAC residual luma4x4 last-significant zero diagnostic");
    require(result.coeffSignFlags.size() == 2,
            "CABAC residual luma4x4 last-significant zero sign flag count");
    require(result.coeffSignFlags[0] == 0 && result.coeffSignFlags[1] == 0,
            "CABAC residual luma4x4 last-significant zero sign flags");
    require(result.diagnosticMessage.contains(QStringLiteral("coeff_sign_flag")),
            "CABAC residual luma4x4 last-significant zero message");
}

void testReadResidualLuma4x4CodedBlockFlagsZeroSingleLuma8x8()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(85, {0, 0});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);
    require(result.ok, "CABAC residual luma4x4 CBF-zero group result");
    require(result.complete, "CABAC residual luma4x4 CBF-zero group complete");
    require(result.blockIndices.size() == 4, "CABAC residual luma4x4 CBF-zero block count");
    require(result.blockIndices[0] == 12 && result.blockIndices[3] == 15,
            "CABAC residual luma4x4 CBF-zero block indices");
    require(result.codedBlockFlags[0] == 0 && result.codedBlockFlags[3] == 0,
            "CABAC residual luma4x4 CBF-zero values");
    require(result.firstCtxIdx == 85, "CABAC residual luma4x4 CBF-zero first ctxIdx");
}

void testReadResidualLuma4x4CodedBlockFlagsZeroAllLuma8x8()
{
    BitReader reader(QByteArray::fromHex("000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(86);
    contexts.setModel(85, {0, 0});

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 15);
    require(result.ok, "CABAC residual luma4x4 all-CBF-zero group result");
    require(result.complete, "CABAC residual luma4x4 all-CBF-zero group complete");
    require(result.blockIndices.size() == 16, "CABAC residual luma4x4 all-CBF-zero block count");
    require(result.blockIndices[0] == 0 && result.blockIndices[15] == 15,
            "CABAC residual luma4x4 all-CBF-zero block indices");
    require(result.codedBlockFlags[0] == 0 && result.codedBlockFlags[15] == 0,
            "CABAC residual luma4x4 all-CBF-zero values");
}

void testReadResidualChromaDcCodedBlockFlagsZero()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(97, {0, 0});

    const H264CabacResidualChromaDcResult result =
        h264ReadCabacResidualChromaDcCodedBlockFlagsZero(reader, decoder, contexts, 1, 1);
    require(result.ok, "CABAC residual chroma DC CBF-zero result");
    require(result.complete, "CABAC residual chroma DC CBF-zero complete");
    require(result.firstCtxIdx == 97, "CABAC residual chroma DC CBF-zero first ctxIdx");
    require(result.components.size() == 2, "CABAC residual chroma DC CBF-zero component count");
    require(result.components[0] == 0 && result.components[1] == 1,
            "CABAC residual chroma DC CBF-zero components");
    require(result.codedBlockFlags[0] == 0 && result.codedBlockFlags[1] == 0,
            "CABAC residual chroma DC CBF-zero values");
}

void testReadResidualChromaDcCodedBlockFlagsZeroWithChromaAcPresent()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(98);
    contexts.setModel(97, {0, 0});

    const H264CabacResidualChromaDcResult result =
        h264ReadCabacResidualChromaDcCodedBlockFlagsZero(reader, decoder, contexts, 1, 2);
    require(result.ok, "CABAC residual chroma DC CBF-zero with AC result");
    require(result.complete, "CABAC residual chroma DC CBF-zero with AC complete");
    require(result.components.size() == 2,
            "CABAC residual chroma DC CBF-zero with AC component count");
    require(result.codedBlockFlags[0] == 0 && result.codedBlockFlags[1] == 0,
            "CABAC residual chroma DC CBF-zero with AC values");
}

void testReadResidualChromaDcCodedBlockFlagNonZeroInferredFinal()
{
    BitReader reader(QByteArray::fromHex("000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    H264CabacContextModelSet contexts(267);
    contexts.setModel(97, {0, 1});
    contexts.setModel(149, {0, 0});
    contexts.setModel(150, {0, 0});
    contexts.setModel(151, {0, 0});
    contexts.setModel(258, {0, 0});

    const H264CabacResidualChromaDcResult result =
        h264ReadCabacResidualChromaDcCodedBlockFlagsZero(reader, decoder, contexts, 1, 1);
    require(result.ok && result.complete, "CABAC residual chroma DC inferred-final complete");
    require(result.codedBlockFlags.size() == 2
                && result.codedBlockFlags[0] == 1 && result.codedBlockFlags[1] == 1,
            "CABAC residual chroma DC inferred-final CBF values");
    require(result.coeffComponentIndices == QVector<int>({0, 1}),
            "CABAC residual chroma DC inferred-final component isolation");
    require(result.coeffScanIndices == QVector<int>({3, 3}),
            "CABAC residual chroma DC inferred-final scan indices");
    require(result.coefficientLevels == QVector<int>({1, 1}),
            "CABAC residual chroma DC inferred-final levels");
    require(result.coeffAbsLevelPrefixFirstCtxIndices == QVector<int>({258, 258}),
            "CABAC residual chroma DC resets level contexts per component");
}

void testReadResidualChromaDcExplicitLastAndAdaptiveLevels()
{
    BitReader reader(QByteArray::fromHex("0000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);
    H264CabacContextModelSet contexts(267);
    contexts.setModel(97, {0, 1});
    contexts.setModel(149, {0, 1});
    contexts.setModel(150, {0, 1});
    contexts.setModel(181, {0, 0});
    contexts.setModel(182, {0, 1});
    contexts.setModel(258, {0, 0});
    contexts.setModel(259, {0, 0});

    const H264CabacResidualChromaDcResult result =
        h264ReadCabacResidualChromaDcCodedBlockFlagsZero(reader, decoder, contexts, 1, 1);
    require(result.ok && result.complete, "CABAC residual chroma DC explicit-last complete");
    require(result.lastSignificantScanIndices == QVector<int>({0, 1, 0, 1}),
            "CABAC residual chroma DC explicit-last scans for both components");
    require(result.coeffScanIndices == QVector<int>({1, 0, 1, 0}),
            "CABAC residual chroma DC reverse scan order");
    require(result.coeffAbsLevelPrefixFirstCtxIndices == QVector<int>({258, 259, 258, 259}),
            "CABAC residual chroma DC adaptive first contexts");
    require(result.coeffAbsLevelMinus1Values == QVector<int>({0, 0, 0, 0}),
            "CABAC residual chroma DC multiple coefficient magnitudes");
}

void testAppendCabacP8x8MacroblockSyntaxSkeleton()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedCodedBlockPatternZero = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 0;
    syntax.codedBlockPatternLuma = 0;
    syntax.codedBlockPatternChroma = 0;

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 syntax skeleton append result");
    require(context.currentAddress == 1, "CABAC P_8x8 syntax skeleton advances address");
    require(slice.macroblocks.size() == 1, "CABAC P_8x8 syntax skeleton macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.address == 0, "CABAC P_8x8 syntax skeleton address");
    require(mb.mbType == QStringLiteral("P_8x8"), "CABAC P_8x8 syntax skeleton type");
    require(mb.predictionMode == QStringLiteral("Pred_L0 sub-macroblock"),
            "CABAC P_8x8 syntax skeleton prediction mode");
    require(mb.parsed, "CABAC P_8x8 syntax skeleton is parsed for CBP zero");
    require(mb.residualParsed, "CABAC P_8x8 syntax skeleton marks no residual parsed");
    require(mb.motionVectors.size() == 4, "CABAC P_8x8 syntax skeleton motion-vector count");
    require(mb.motionVectors.first().list == 0, "CABAC P_8x8 syntax skeleton L0 motion vector");
    require(mb.motionVectors.first().referenceIndex == 0, "CABAC P_8x8 syntax skeleton ref_idx_l0");
    require(mb.motionVectors.first().mvXQuarterPel == 0 && mb.motionVectors.first().mvYQuarterPel == 0,
            "CABAC P_8x8 syntax skeleton zero predicted motion vector");
    require(context.mvStatesL0[0].valid, "CABAC P_8x8 syntax skeleton updates L0 MV state");
    require(mb.note.contains(QStringLiteral("no residual blocks")),
            "CABAC P_8x8 syntax skeleton note describes no residual");
}

void testAppendCabacP8x8MacroblockSyntaxSkeletonWithResidualCbfZero()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedResidualCodedBlockFlagsZero = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 8;
    syntax.codedBlockPatternLuma = 8;
    syntax.codedBlockPatternChroma = 0;
    syntax.residualLuma4x4BlockIndices = {12, 13, 14, 15};
    syntax.residualCodedBlockFlags = {0, 0, 0, 0};

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 residual CBF-zero skeleton append result");
    require(slice.macroblocks.size() == 1,
            "CABAC P_8x8 residual CBF-zero skeleton macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.parsed, "CABAC P_8x8 residual CBF-zero skeleton parsed flag");
    require(mb.residualParsed, "CABAC P_8x8 residual CBF-zero skeleton residual parsed flag");
    require(mb.codedBlockPatternLuma == 8,
            "CABAC P_8x8 residual CBF-zero skeleton luma CBP");
    require(mb.residualBlocks.size() == 4,
            "CABAC P_8x8 residual CBF-zero skeleton residual block count");
    require(mb.residualBlocks.first().kind == QStringLiteral("luma4x4"),
            "CABAC P_8x8 residual CBF-zero skeleton residual kind");
    require(mb.residualBlocks.first().blockIndex == 12
                && mb.residualBlocks.last().blockIndex == 15,
            "CABAC P_8x8 residual CBF-zero skeleton residual indices");
    require(mb.residualCoefficientCount == 0,
            "CABAC P_8x8 residual CBF-zero skeleton coefficient count");
    require(mb.note.contains(QStringLiteral("coded_block_flag")),
            "CABAC P_8x8 residual CBF-zero skeleton note");
}

void testAppendCabacP8x8MacroblockSyntaxSkeletonWithMultiResidualCbfZero()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedResidualCodedBlockFlagsZero = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 15;
    syntax.codedBlockPatternLuma = 15;
    syntax.codedBlockPatternChroma = 0;
    for (int blockIndex = 0; blockIndex < 16; ++blockIndex) {
        syntax.residualLuma4x4BlockIndices.append(blockIndex);
        syntax.residualCodedBlockFlags.append(0);
    }

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 multi residual CBF-zero skeleton append result");
    require(slice.macroblocks.size() == 1,
            "CABAC P_8x8 multi residual CBF-zero skeleton macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.codedBlockPatternLuma == 15,
            "CABAC P_8x8 multi residual CBF-zero skeleton luma CBP");
    require(mb.residualBlocks.size() == 16,
            "CABAC P_8x8 multi residual CBF-zero skeleton residual block count");
    require(mb.residualBlocks.first().blockIndex == 0
                && mb.residualBlocks.last().blockIndex == 15,
            "CABAC P_8x8 multi residual CBF-zero skeleton residual indices");
    require(mb.residualCoefficientCount == 0,
            "CABAC P_8x8 multi residual CBF-zero skeleton coefficient count");
}

void testAppendCabacP8x8MacroblockSyntaxSkeletonWithChromaDcCbfZero()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedResidualCodedBlockFlagsZero = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 16;
    syntax.codedBlockPatternLuma = 0;
    syntax.codedBlockPatternChroma = 1;
    syntax.residualChromaDcComponents = {0, 1};
    syntax.residualChromaDcCodedBlockFlags = {0, 0};

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 chroma DC CBF-zero skeleton append result");
    require(slice.macroblocks.size() == 1,
            "CABAC P_8x8 chroma DC CBF-zero skeleton macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.codedBlockPatternLuma == 0,
            "CABAC P_8x8 chroma DC CBF-zero skeleton luma CBP");
    require(mb.codedBlockPatternChroma == 1,
            "CABAC P_8x8 chroma DC CBF-zero skeleton chroma CBP");
    require(mb.residualBlocks.size() == 2,
            "CABAC P_8x8 chroma DC CBF-zero skeleton residual block count");
    require(mb.residualBlocks.first().kind == QStringLiteral("chroma_dc"),
            "CABAC P_8x8 chroma DC CBF-zero skeleton residual kind");
    require(mb.residualBlocks.first().component == 0
                && mb.residualBlocks.last().component == 1,
            "CABAC P_8x8 chroma DC CBF-zero skeleton residual components");
    require(mb.residualCoefficientCount == 0,
            "CABAC P_8x8 chroma DC CBF-zero skeleton coefficient count");
}

void testAppendCabacP8x8MacroblockSyntaxSkeletonWithChromaDcCoefficients()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedResidual = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 16;
    syntax.codedBlockPatternLuma = 0;
    syntax.codedBlockPatternChroma = 1;
    syntax.residualChromaDcComponents = {0, 1};
    syntax.residualChromaDcCodedBlockFlags = {1, 1};
    syntax.residualChromaDcCoeffComponents = {0, 0, 1};
    syntax.residualChromaDcCoeffScanIndices = {3, 0, 2};
    syntax.residualChromaDcCoefficientLevels = {2, -1, 1};

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 chroma DC coefficient skeleton append result");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.residualBlocks.size() == 2,
            "CABAC P_8x8 chroma DC coefficient residual block count");
    require(mb.residualBlocks[0].totalCoefficientCount == 2
                && mb.residualBlocks[1].totalCoefficientCount == 1,
            "CABAC P_8x8 chroma DC coefficient counts by component");
    require(mb.residualBlocks[0].coefficients[0].scanIndex == 3
                && mb.residualBlocks[0].coefficients[0].level == 2,
            "CABAC P_8x8 chroma DC coefficient scan and level");
    require(mb.residualBlocks[0].coefficients[1].scanIndex == 0
                && mb.residualBlocks[0].coefficients[1].level == -1,
            "CABAC P_8x8 chroma DC second coefficient scan and level");
    require(mb.residualCoefficientCount == 3,
            "CABAC P_8x8 chroma DC total coefficient count");
}

void testAppendCabacP8x8MacroblockSyntaxSkeletonUsesNeighborPrediction()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 1);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    context.mvStatesL0[0] = {true, 0, 3, -1};

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedCodedBlockPatternZero = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 0;
    syntax.codedBlockPatternLuma = 0;
    syntax.codedBlockPatternChroma = 0;

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 syntax skeleton neighbor append result");
    require(slice.macroblocks.size() == 1, "CABAC P_8x8 syntax skeleton neighbor macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.address == 1, "CABAC P_8x8 syntax skeleton neighbor address");
    require(mb.motionVectors.size() == 4, "CABAC P_8x8 syntax skeleton neighbor motion-vector count");
    require(mb.motionVectors.first().mvXQuarterPel == 3 && mb.motionVectors.first().mvYQuarterPel == -1,
            "CABAC P_8x8 syntax skeleton uses neighboring MV prediction");
    require(context.mvStatesL0[1].valid, "CABAC P_8x8 syntax skeleton neighbor updates L0 MV state");
    require(context.mvStatesL0[1].mvX == 3 && context.mvStatesL0[1].mvY == -1,
            "CABAC P_8x8 syntax skeleton neighbor state value");
}

void testAppendCabacP8x8MacroblockSyntaxSkeletonAppliesMvd()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 0, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedCodedBlockPatternZero = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{1, 0}, {0, -1}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 0;
    syntax.codedBlockPatternLuma = 0;
    syntax.codedBlockPatternChroma = 0;

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC P_8x8 syntax skeleton non-zero MVD append result");
    require(slice.macroblocks.size() == 1, "CABAC P_8x8 syntax skeleton non-zero MVD macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.motionVectors.size() == 4,
            "CABAC P_8x8 syntax skeleton non-zero MVD motion-vector count");
    require(mb.motionVectors[0].mvXQuarterPel == 1 && mb.motionVectors[0].mvYQuarterPel == 0,
            "CABAC P_8x8 syntax skeleton applies first tiny MVD");
    require(mb.motionVectors[1].mvXQuarterPel == 0 && mb.motionVectors[1].mvYQuarterPel == -1,
            "CABAC P_8x8 syntax skeleton applies second tiny MVD");
}

void testReadISliceMbTypePrefix()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 2, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(true, 0, context.currentQp, 23);

    const H264CabacSyntaxResult result = h264ReadCabacMbTypePrefix(reader, decoder, contexts, context);
    require(result.ok, "CABAC I mb_type prefix result");
    require(result.value == 0 || result.value == 1, "CABAC I mb_type prefix bin value is binary");
    require(result.ctxIdx == 3, "CABAC I mb_type prefix ctxIdx");
}

void testReadISliceMbTypeINxN()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 2, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(true, 0, context.currentQp, 23);

    const H264CabacMbTypeResult result = h264ReadCabacMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC I mb_type result");
    require(result.complete, "CABAC I mb_type I_NxN is complete");
    require(result.mbType == 0, "CABAC I mb_type I_NxN value");
    require(result.prefixBin == 0, "CABAC I mb_type I_NxN prefix bin");
    require(result.ctxIdx == 3, "CABAC I mb_type I_NxN ctxIdx");
}

void testReadISliceMbTypeI16x16()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 2, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacContextModelSet contexts(11);
    for (int ctxIdx = 0; ctxIdx <= 10; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
    contexts.setModel(3, {0, 1});

    const H264CabacMbTypeResult result = h264ReadCabacMbType(reader, decoder, contexts, context);
    require(result.ok, "CABAC I mb_type I_16x16 result");
    require(result.complete, "CABAC I mb_type I_16x16 is complete");
    require(result.mbType == 1, "CABAC I mb_type I_16x16 value");
    require(result.prefixBin == 1, "CABAC I mb_type I_16x16 prefix bin");
    require(result.ctxIdx == 3, "CABAC I mb_type I_16x16 ctxIdx");
}

void testReadBSliceMbSkipFlagReportsMissingContext()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 1, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 23);

    const H264CabacSyntaxResult result = h264ReadCabacMbSkipFlag(reader, decoder, contexts, context);
    require(!result.ok, "CABAC B mb_skip_flag missing context fails cleanly");
    require(result.ctxIdx == 24, "CABAC B mb_skip_flag ctxIdx");
    require(result.diagnosticCode == QStringLiteral("cabac_context_uninitialized"),
            "CABAC B mb_skip_flag missing context diagnostic");
}

void testReadBSliceMbSkipFlagWithCoveredContext()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder = initializedDecoder(reader);

    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeBasicSlice(slice, 1, 0);
    initializeBasicSps(sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, context.currentQp, 39);

    const H264CabacSyntaxResult result = h264ReadCabacMbSkipFlag(reader, decoder, contexts, context);
    require(result.ok, "CABAC B mb_skip_flag covered context result");
    require(result.value == 0 || result.value == 1, "CABAC B mb_skip_flag bin value is binary");
    require(result.ctxIdx == 24, "CABAC B mb_skip_flag covered ctxIdx");
}

void testReadResidualLuma4x4TerminatedPrefixProducesCoefficientValue()
{
    BitReader reader(QByteArray::fromHex("000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);
    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFifthContext(1, 1, 1, 1, 0);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);

    require(result.ok, "CABAC terminated prefix coefficient result");
    require(!result.complete, "CABAC terminated prefix residual block remains incomplete");
    require(result.coeffAbsLevelPrefixOneCounts == QVector<int>{4},
            "CABAC terminated prefix one-count");
    require(result.coeffAbsLevelMinus1Values == QVector<int>{4},
            "CABAC terminated prefix coeff_abs_level_minus1 value");
    require(result.coeffSignFlags == QVector<int>{0},
            "CABAC terminated prefix sign flag");
    require(result.coefficientLevels == QVector<int>{5},
            "CABAC terminated prefix signed coefficient value");
    require(result.coeffAbsLevelSuffixBins.isEmpty(),
            "CABAC terminated pre-UEG0 prefix consumes no suffix bins");
    require(result.coeffAbsLevelRemainingInputBins.isEmpty(),
            "CABAC terminated pre-UEG0 prefix has no remaining input bins");
    require(result.incompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC terminated prefix stops at residual coefficient completion");
}

void testReadResidualLuma4x4SixthBinZeroProducesCoefficientValue()
{
    BitReader reader(QByteArray::fromHex("0100000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);
    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFifthContext(1, 1, 1, 1, 1);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);

    require(result.ok, "CABAC sixth-bin zero coefficient result");
    require(!result.complete, "CABAC sixth-bin zero residual block remains incomplete");
    require(result.coeffAbsLevelPrefixOneCounts == QVector<int>{5},
            "CABAC sixth-bin zero prefix one-count");
    require(result.coeffAbsLevelMinus1Values == QVector<int>{5},
            "CABAC sixth-bin zero coeff_abs_level_minus1 value");
    require(result.coeffSignFlags.size() == 1,
            "CABAC sixth-bin zero sign count");
    require(result.coefficientLevels.size() == 1
                && (result.coefficientLevels[0] == 6 || result.coefficientLevels[0] == -6),
            "CABAC sixth-bin zero signed coefficient magnitude");
    require(result.coeffAbsLevelSuffixBins.isEmpty(),
            "CABAC sixth-bin zero consumes no suffix bins");
    require(result.incompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC sixth-bin zero stops at residual coefficient completion");
}

void testReadResidualLuma4x4CutoffUsesUeg0ZeroCodeword()
{
    BitReader reader(QByteArray::fromHex("000000000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);
    H264CabacContextModelSet contexts =
        initializedLuma4x4CoeffLevelContextsWithFifthContext(1, 1, 1, 1, 1);

    const H264CabacResidualLuma4x4Result result =
        h264ReadCabacResidualLuma4x4CodedBlockFlagsZero(reader, decoder, contexts, 8);

    require(result.ok, "CABAC cutoff UEG0 zero-codeword result");
    require(!result.complete, "CABAC cutoff UEG0 residual block remains incomplete");
    require(result.coeffAbsLevelPrefixNextBins.size() == 13,
            "CABAC cutoff reads all thirteen additional context-coded prefix bins");
    require(result.coeffAbsLevelPrefixFirstCtxIndices == QVector<int>{248},
            "CABAC cutoff first prefix context");
    require(result.coeffAbsLevelPrefixNextCtxIndices == QVector<int>(13, 252),
            "CABAC cutoff reuses one state-derived context for every continuation bin");
    require(result.coeffAbsLevelPrefixOneCounts == QVector<int>{14},
            "CABAC cutoff prefix one-count");
    require(result.coeffAbsLevelPrefixTerminatedFlags == QVector<int>{1},
            "CABAC cutoff prefix completion flag");
    require(result.coeffAbsLevelSuffixBins == QVector<int>{0},
            "CABAC cutoff UEG0 zero codeword bins");
    require(result.coeffAbsLevelSuffixBinCounts == QVector<int>{1},
            "CABAC cutoff UEG0 codeword bin count");
    require(result.coeffAbsLevelReadyForValueFlags == QVector<int>{1},
            "CABAC cutoff UEG0 value-ready flag");
    require(result.coeffAbsLevelMinus1Values == QVector<int>{14},
            "CABAC cutoff coeff_abs_level_minus1 value");
    require(result.coeffSignFlags == QVector<int>{0},
            "CABAC cutoff coefficient sign");
    require(result.coefficientLevels == QVector<int>{15},
            "CABAC cutoff signed coefficient value");
    require(result.incompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC cutoff stops at residual coefficient completion");
}

void testReadCabacMacroblockSyntaxPropagatesCoefficientValue()
{
    BitReader reader(QByteArray::fromHex("00000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContextsWithFifthContext(
            context.currentQp, 1, 1, 1, 1, 0);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);

    require(result.ok, "CABAC macroblock coefficient propagation result");
    require(!result.complete, "CABAC macroblock coefficient propagation remains incomplete");
    require(result.residualCoeffAbsLevelMinus1Values == QVector<int>{4},
            "CABAC macroblock coeff_abs_level_minus1 propagation");
    require(result.residualCoeffSignFlags == QVector<int>{0},
            "CABAC macroblock coefficient sign propagation");
    require(result.residualCoefficientLevels == QVector<int>{5},
            "CABAC macroblock signed coefficient propagation");
    require(result.residualCoeffAbsLevelSuffixBins.isEmpty(),
            "CABAC macroblock pre-UEG0 coefficient has no suffix bins");
    require(result.residualIncompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC macroblock coefficient propagation stop stage");
}

void testReadCabacMacroblockSyntaxPropagatesCutoffUeg0Value()
{
    BitReader reader(QByteArray::fromHex("000000000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContextsWithFifthContext(
            context.currentQp, 1, 1, 1, 1, 1);

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);

    require(result.ok, "CABAC macroblock cutoff UEG0 propagation result");
    require(!result.complete, "CABAC macroblock cutoff UEG0 remains incomplete");
    require(result.residualCoeffAbsLevelPrefixOneCounts == QVector<int>{14},
            "CABAC macroblock cutoff prefix propagation");
    require(result.residualCoeffAbsLevelPrefixFirstCtxIndices == QVector<int>{248},
            "CABAC macroblock first prefix context propagation");
    require(result.residualCoeffAbsLevelPrefixNextCtxIndices == QVector<int>(13, 252),
            "CABAC macroblock continuation context propagation");
    require(result.residualCoeffAbsLevelSuffixBins == QVector<int>{0},
            "CABAC macroblock UEG0 codeword propagation");
    require(result.residualCoeffAbsLevelMinus1Values == QVector<int>{14},
            "CABAC macroblock cutoff coeff_abs_level_minus1 propagation");
    require(result.residualCoefficientLevels == QVector<int>{15},
            "CABAC macroblock cutoff signed coefficient propagation");
    require(result.residualIncompleteStage == QStringLiteral("residual_coefficients"),
            "CABAC macroblock cutoff propagation stop stage");
}

void testReadCabacMacroblockSyntaxPropagatesFullCoefficientLoop()
{
    BitReader reader(QByteArray::fromHex("0000000000000000000000000000000000000000"));
    H264CabacDecoder decoder = initializedDecoder(reader);
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);
    H264CabacContextModelSet contexts =
        initializedP8x8LumaResidualCoeffLevelContextsWithFifthContext(
            context.currentQp, 1, 0, 0, 0, 0);

    for (int ctxIdx = 134; ctxIdx <= 138; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 1});
    }
    for (int ctxIdx = 166; ctxIdx <= 169; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }
    contexts.setModel(170, {0, 1});
    contexts.setModel(247, {0, 1});
    contexts.setModel(248, {0, 1});
    for (int ctxIdx = 252; ctxIdx <= 256; ++ctxIdx) {
        contexts.setModel(ctxIdx, {0, 0});
    }

    const H264CabacMacroblockSyntaxResult result =
        h264ReadCabacMacroblockSyntax(context, decoder, contexts);

    require(result.ok, "CABAC macroblock full coefficient loop propagation result");
    require(result.residualCoeffReverseScanIndices == QVector<int>({4, 3, 2, 1, 0}),
            "CABAC macroblock full reverse-scan order propagation");
    require(result.residualCoeffAbsLevelMinus1Values == QVector<int>(5, 1),
            "CABAC macroblock full coefficient value propagation");
    require(result.residualCoeffAbsLevelPrefixNextCtxIndices
                == QVector<int>({252, 253, 254, 255, 256}),
            "CABAC macroblock saturated continuation context propagation");
    require(result.residualCoeffSignFlags.size() == 5,
            "CABAC macroblock full coefficient sign propagation");
}

void testAppendCabacP8x8MacroblockSyntaxWithNonZeroResidualBlock()
{
    BitReader reader(QByteArray::fromHex("0000"));
    SliceInfo slice;
    PpsInfo pps;
    SpsInfo sps;
    initializeP8x8LumaResidualSlice(slice, sps);
    H264SliceDataContext context(reader, slice, pps, sps);

    H264CabacMacroblockSyntaxResult syntax;
    syntax.ok = true;
    syntax.complete = true;
    syntax.parsedSubMacroblockSyntax = true;
    syntax.parsedCodedBlockPattern = true;
    syntax.parsedResidual = true;
    syntax.mbType = 3;
    syntax.subMbTypes = {0, 0, 0, 0};
    syntax.refIdxL0 = {0, 0, 0, 0};
    syntax.mvdL0 = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
    syntax.codedBlockPattern = 8;
    syntax.codedBlockPatternLuma = 8;
    syntax.codedBlockPatternChroma = 0;
    syntax.residualLuma4x4BlockIndices = {12, 13, 14, 15};
    syntax.residualCodedBlockFlags = {1, 0, 0, 0};
    syntax.residualCoeffAbsLevelBlockIndices = {12, 12};
    syntax.residualCoeffAbsLevelScanIndices = {5, 1};
    syntax.residualCoefficientLevels = {-3, 1};

    require(h264AppendCabacMacroblockSyntaxSkeleton(context, syntax),
            "CABAC non-zero residual macroblock append result");
    require(slice.macroblocks.size() == 1,
            "CABAC non-zero residual macroblock append count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.residualParsed && mb.parsed,
            "CABAC non-zero residual macroblock parsed flags");
    require(mb.residualBlocks.size() == 4 && mb.residualBlockCount == 4,
            "CABAC non-zero residual block model count");
    require(mb.residualCoefficientCount == 2,
            "CABAC non-zero residual macroblock coefficient count");
    require(mb.residualBlocks[0].blockIndex == 12
                && mb.residualBlocks[0].totalCoefficientCount == 2,
            "CABAC non-zero residual first block summary");
    require(mb.residualBlocks[0].coefficients[0].scanIndex == 5
                && mb.residualBlocks[0].coefficients[0].level == -3
                && mb.residualBlocks[0].coefficients[1].scanIndex == 1
                && mb.residualBlocks[0].coefficients[1].level == 1,
            "CABAC non-zero residual coefficient model values");
    require(context.coeffStates[0].luma[12] == 2
                && context.coeffStates[0].luma[13] == 0,
            "CABAC non-zero residual neighbor coefficient state");
}
}

int main()
{
    testCoeffAbsLevelMinus1Ueg0CutoffHelper();
    testCoeffAbsLevelMinus1PreUeg0RemainingInputHelper();
    testCoeffAbsLevelMinus1Ueg0SuffixValueHelper();
    testCoeffAbsLevelMinus1Ueg0SuffixComputeHelper();
    testReadPSliceMbSkipFlag();
    testReadPSliceMbSkipFlagUsesLeftNeighbor();
    testReadPSliceMbTypeP16x16();
    testReadPSliceMbTypeP16x8();
    testReadPSliceMbTypeP8x16();
    testReadPSliceMbTypeP8x8RequiresSubMbType();
    testReadPSubMbTypeP8x8();
    testReadPSubMbTypeReportsMissingContext();
    testReadPSubMbTypeP8x4();
    testReadPSubMbTypeP4x8();
    testReadPSubMbTypeP4x4();
    testReadFourPSubMbTypes();
    testReadFourPSubMbTypesReportsIndexedFailure();
    testReadPSubMbRefIdxNotPresent();
    testReadPSubMbRefIdxZero();
    testReadPSubMbRefIdxIncomplete();
    testReadMvdComponentZero();
    testReadMvdComponentAbs2Positive();
    testReadMvdComponentAbs3Positive();
    testReadMvdComponentGreaterThanThreeIncomplete();
    testReadPSubMbMvdZero8x8();
    testReadPSubMbMvdAbs2_8x8();
    testReadPSubMbMvdZero4x4();
    testReadCabacMacroblockSyntaxP8x8RefAbsent();
    testReadCabacMacroblockSyntaxP8x8RefZero();
    testReadCabacMacroblockSyntaxP8x8ChromaAcResidualIncomplete();
    testReadCabacMacroblockSyntaxP8x8SingleLumaResidualCbfZero();
    testReadCabacMacroblockSyntaxP8x8MultiLumaResidualCbfZero();
    testReadCabacMacroblockSyntaxP8x8ChromaDcResidualCbfZero();
    testReadCabacMacroblockSyntaxP8x8ChromaDcResidualCbfNonZero();
    testReadCabacMacroblockSyntaxP8x8SmallNonZeroMvd();
    testReadCabacMacroblockSyntaxP8x8NonZeroMvdIncomplete();
    testReadCodedBlockPatternZeroMonochrome();
    testReadCodedBlockPatternZeroChroma();
    testReadCodedBlockPatternNonZeroLumaIncomplete();
    testReadCodedBlockPatternNonZeroChromaIncomplete();
    testReadMbQpDeltaZero();
    testReadMbQpDeltaNonZeroIncomplete();
    testReadResidualCodedBlockFlagZeroLuma4x4();
    testReadResidualCodedBlockFlagNonZeroIncomplete();
    testReadResidualLuma4x4SignificantCoeffFlagMissingLaterContext();
    testReadResidualLuma4x4InferredFinalCoeffLevelMissingContext();
    testReadResidualLuma4x4CoeffAbsLevelNextBinMissingContext();
    testReadResidualLuma4x4CodedBlockFlagsZeroSingleLuma8x8();
    testReadResidualLuma4x4CodedBlockFlagsZeroAllLuma8x8();
    testReadResidualChromaDcCodedBlockFlagsZero();
    testReadResidualChromaDcCodedBlockFlagsZeroWithChromaAcPresent();
    testReadResidualChromaDcCodedBlockFlagNonZeroInferredFinal();
    testReadResidualChromaDcExplicitLastAndAdaptiveLevels();
    testAppendCabacP8x8MacroblockSyntaxSkeleton();
    testAppendCabacP8x8MacroblockSyntaxSkeletonWithResidualCbfZero();
    testAppendCabacP8x8MacroblockSyntaxSkeletonWithMultiResidualCbfZero();
    testAppendCabacP8x8MacroblockSyntaxSkeletonWithChromaDcCbfZero();
    testAppendCabacP8x8MacroblockSyntaxSkeletonWithChromaDcCoefficients();
    testAppendCabacP8x8MacroblockSyntaxWithNonZeroResidualBlock();
    testAppendCabacP8x8MacroblockSyntaxSkeletonUsesNeighborPrediction();
    testAppendCabacP8x8MacroblockSyntaxSkeletonAppliesMvd();
    testReadISliceMbTypePrefix();
    testReadISliceMbTypeINxN();
    testReadISliceMbTypeI16x16();
    testReadBSliceMbSkipFlagReportsMissingContext();
    testReadBSliceMbSkipFlagWithCoveredContext();
    std::cout << "H264CabacSyntaxReader tests passed\n";
    return 0;
}
