#include "core/parser/video/h264/cabac/H264CabacDecoder.h"

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

void testConsumeAlignmentBits()
{
    BitReader aligned(QByteArray::fromHex("ff"));
    require(H264CabacDecoder::consumeAlignmentBits(aligned), "already aligned CABAC alignment");
    require(aligned.bitOffset() == 0, "already aligned bit offset unchanged");

    BitReader reader(QByteArray::fromHex("bf")); // 10111111
    reader.readBits(3);
    require(H264CabacDecoder::consumeAlignmentBits(reader), "CABAC alignment consumes one bits");
    require(reader.bitOffset() == 8, "CABAC alignment reaches byte boundary");

    BitReader malformed(QByteArray::fromHex("af")); // 10101111
    malformed.readBits(3);
    require(!H264CabacDecoder::consumeAlignmentBits(malformed), "CABAC alignment rejects zero bit");
}

void testInitializeDecoderState()
{
    BitReader reader(QByteArray::fromHex("aa80")); // first 9 bits: 101010101
    H264CabacDecoder decoder;
    require(decoder.initialize(reader), "CABAC decoder initializes from 9 bits");
    require(decoder.state().initialized, "CABAC decoder initialized flag");
    require(decoder.state().codIRange == 510, "CABAC initial range");
    require(decoder.state().codIOffset == 341, "CABAC initial offset");
    require(reader.bitOffset() == 9, "CABAC initialize consumes 9 bits");
}

void testContextModelInitialization()
{
    const H264CabacContextModel low = H264CabacDecoder::initializedContextModel(0, 10, 26);
    require(low.stateIndex == 53, "CABAC context low state index");
    require(low.valueMps == 0, "CABAC context low MPS");

    const H264CabacContextModel high = H264CabacDecoder::initializedContextModel(0, 100, 26);
    require(high.stateIndex == 36, "CABAC context high state index");
    require(high.valueMps == 1, "CABAC context high MPS");

    const H264CabacContextModel clipped = H264CabacDecoder::initializedContextModel(-100, -100, 26);
    require(clipped.stateIndex == 62, "CABAC context clipped state index");
    require(clipped.valueMps == 0, "CABAC context clipped MPS");
}

void testContextModelSetInitialization()
{
    const H264CabacContextModelSet pIdc0 =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, 26, 266);
    const H264CabacContextModelSet pIdc1 =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 1, 26, 266);
    const H264CabacContextModelSet pIdc2 =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 2, 26, 266);
    require(pIdc0.size() == 267, "CABAC P context set size");
    require(pIdc0.isInitialized(0), "CABAC ctxIdx 0 initialized");
    require(pIdc0.isInitialized(23), "CABAC ctxIdx 23 initialized");
    require(pIdc0.isInitialized(24), "CABAC ctxIdx 24 initialized");
    require(pIdc0.isInitialized(39), "CABAC ctxIdx 39 initialized");
    require(pIdc0.isInitialized(54), "CABAC ctxIdx 54 initialized");
    require(pIdc0.isInitialized(59), "CABAC ctxIdx 59 initialized");
    require(pIdc0.isInitialized(73), "CABAC ctxIdx 73 initialized");
    require(pIdc0.isInitialized(77), "CABAC ctxIdx 77 initialized");
    require(pIdc0.isInitialized(85), "CABAC ctxIdx 85 initialized");
    require(pIdc0.isInitialized(97), "CABAC ctxIdx 97 initialized");
    require(pIdc0.isInitialized(134), "CABAC ctxIdx 134 initialized");
    require(pIdc0.isInitialized(148), "CABAC ctxIdx 148 initialized");
    require(pIdc0.isInitialized(166), "CABAC ctxIdx 166 initialized");
    require(pIdc0.isInitialized(180), "CABAC ctxIdx 180 initialized");
    require(pIdc0.isInitialized(248), "CABAC ctxIdx 248 initialized");
    require(pIdc0.isInitialized(252), "CABAC ctxIdx 252 initialized");
    require(pIdc0.isInitialized(253), "CABAC ctxIdx 253 initialized");
    require(pIdc0.isInitialized(254), "CABAC ctxIdx 254 initialized");
    require(pIdc0.isInitialized(255), "CABAC ctxIdx 255 initialized");
    require(pIdc0.isInitialized(256), "CABAC ctxIdx 256 initialized");
    require(pIdc0.isInitialized(257), "CABAC ctxIdx 257 initialized");
    require(pIdc0.isInitialized(262), "CABAC ctxIdx 262 initialized");
    require(pIdc0.isInitialized(266), "CABAC ctxIdx 266 initialized");
    require(pIdc0.model(257).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(0, 70, 26).stateIndex,
            "CABAC ctxIdx 257 initialization state");
    require(pIdc1.model(262).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-2, 55, 26).stateIndex,
            "CABAC ctxIdx 262 cabac_init_idc 1 initialization state");
    require(pIdc2.model(266).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-13, 81, 26).stateIndex,
            "CABAC ctxIdx 266 cabac_init_idc 2 initialization state");
    const H264CabacContextModelSet intraExtended =
        H264CabacContextModelInitializer::initializeSliceContexts(true, 0, 26, 266);
    require(intraExtended.model(257).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-11, 97, 26).stateIndex,
            "CABAC ctxIdx 257 intra initialization state");
    require(intraExtended.model(266).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(-8, 78, 26).valueMps,
            "CABAC ctxIdx 266 intra initialization MPS");
    require(pIdc0.model(73).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-27, 126, 26).stateIndex,
            "CABAC ctxIdx 73 initialization state");
    require(pIdc0.model(73).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(-27, 126, 26).valueMps,
            "CABAC ctxIdx 73 initialization MPS");
    require(pIdc0.model(77).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-28, 82, 26).stateIndex,
            "CABAC ctxIdx 77 initialization state");
    require(pIdc0.model(77).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(-28, 82, 26).valueMps,
            "CABAC ctxIdx 77 initialization MPS");
    require(pIdc0.model(85).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-7, 92, 26).stateIndex,
            "CABAC ctxIdx 85 initialization state");
    require(pIdc0.model(85).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(-7, 92, 26).valueMps,
            "CABAC ctxIdx 85 initialization MPS");
    require(pIdc1.model(85).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(0, 80, 26).stateIndex,
            "CABAC ctxIdx 85 cabac_init_idc 1 initialization state");
    require(pIdc1.model(85).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(0, 80, 26).valueMps,
            "CABAC ctxIdx 85 cabac_init_idc 1 initialization MPS");
    require(pIdc2.model(85).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(11, 80, 26).stateIndex,
            "CABAC ctxIdx 85 cabac_init_idc 2 initialization state");
    require(pIdc2.model(85).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(11, 80, 26).valueMps,
            "CABAC ctxIdx 85 cabac_init_idc 2 initialization MPS");
    require(pIdc0.model(97).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(5, 54, 26).stateIndex,
            "CABAC ctxIdx 97 initialization state");
    require(pIdc0.model(97).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(5, 54, 26).valueMps,
            "CABAC ctxIdx 97 initialization MPS");
    require(pIdc1.model(97).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(3, 55, 26).stateIndex,
            "CABAC ctxIdx 97 cabac_init_idc 1 initialization state");
    require(pIdc2.model(97).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(0, 65, 26).stateIndex,
            "CABAC ctxIdx 97 cabac_init_idc 2 initialization state");
    require(pIdc0.model(134).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(9, 53, 26).stateIndex,
            "CABAC ctxIdx 134 initialization state");
    require(pIdc0.model(134).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(9, 53, 26).valueMps,
            "CABAC ctxIdx 134 initialization MPS");
    require(pIdc1.model(134).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(0, 54, 26).stateIndex,
            "CABAC ctxIdx 134 cabac_init_idc 1 initialization state");
    require(pIdc2.model(137).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-8, 80, 26).stateIndex,
            "CABAC ctxIdx 137 cabac_init_idc 2 initialization state");
    require(pIdc0.model(148).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(13, 68, 26).stateIndex,
            "CABAC ctxIdx 148 initialization state");
    require(pIdc0.model(166).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(11, 28, 26).stateIndex,
            "CABAC ctxIdx 166 initialization state");
    require(pIdc0.model(166).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(11, 28, 26).valueMps,
            "CABAC ctxIdx 166 initialization MPS");
    require(pIdc1.model(166).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(4, 45, 26).stateIndex,
            "CABAC ctxIdx 166 cabac_init_idc 1 initialization state");
    require(pIdc2.model(169).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(11, 29, 26).stateIndex,
            "CABAC ctxIdx 169 cabac_init_idc 2 initialization state");
    require(pIdc0.model(180).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(4, 63, 26).stateIndex,
            "CABAC ctxIdx 180 initialization state");
    require(pIdc0.model(248).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-3, 29, 26).stateIndex,
            "CABAC ctxIdx 248 initialization state");
    require(pIdc0.model(248).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(-3, 29, 26).valueMps,
            "CABAC ctxIdx 248 initialization MPS");
    require(pIdc1.model(248).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-10, 44, 26).stateIndex,
            "CABAC ctxIdx 248 cabac_init_idc 1 initialization state");
    require(pIdc2.model(248).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-8, 48, 26).stateIndex,
            "CABAC ctxIdx 248 cabac_init_idc 2 initialization state");
    require(pIdc0.model(252).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-6, 55, 26).stateIndex,
            "CABAC ctxIdx 252 initialization state");
    require(pIdc0.model(252).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(-6, 55, 26).valueMps,
            "CABAC ctxIdx 252 initialization MPS");
    require(pIdc1.model(252).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-16, 72, 26).stateIndex,
            "CABAC ctxIdx 252 cabac_init_idc 1 initialization state");
    require(pIdc2.model(252).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-14, 75, 26).stateIndex,
            "CABAC ctxIdx 252 cabac_init_idc 2 initialization state");
    require(pIdc0.model(253).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(0, 58, 26).stateIndex,
            "CABAC ctxIdx 253 initialization state");
    require(pIdc0.model(253).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(0, 58, 26).valueMps,
            "CABAC ctxIdx 253 initialization MPS");
    require(pIdc1.model(253).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-7, 69, 26).stateIndex,
            "CABAC ctxIdx 253 cabac_init_idc 1 initialization state");
    require(pIdc2.model(253).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-10, 79, 26).stateIndex,
            "CABAC ctxIdx 253 cabac_init_idc 2 initialization state");
    require(pIdc0.model(254).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(0, 64, 26).stateIndex,
            "CABAC ctxIdx 254 initialization state");
    require(pIdc0.model(254).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(0, 64, 26).valueMps,
            "CABAC ctxIdx 254 initialization MPS");
    require(pIdc1.model(254).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-4, 69, 26).stateIndex,
            "CABAC ctxIdx 254 cabac_init_idc 1 initialization state");
    require(pIdc2.model(254).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-9, 83, 26).stateIndex,
            "CABAC ctxIdx 254 cabac_init_idc 2 initialization state");
    require(pIdc0.model(255).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-3, 74, 26).stateIndex,
            "CABAC ctxIdx 255 initialization state");
    require(pIdc0.model(255).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(-3, 74, 26).valueMps,
            "CABAC ctxIdx 255 initialization MPS");
    require(pIdc1.model(255).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-5, 74, 26).stateIndex,
            "CABAC ctxIdx 255 cabac_init_idc 1 initialization state");
    require(pIdc2.model(255).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-12, 92, 26).stateIndex,
            "CABAC ctxIdx 255 cabac_init_idc 2 initialization state");
    require(pIdc0.model(256).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-10, 90, 26).stateIndex,
            "CABAC ctxIdx 256 initialization state");
    require(pIdc0.model(256).valueMps ==
                H264CabacContextModelInitializer::initializedContextModel(-10, 90, 26).valueMps,
            "CABAC ctxIdx 256 initialization MPS");
    require(pIdc1.model(256).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-9, 86, 26).stateIndex,
            "CABAC ctxIdx 256 cabac_init_idc 1 initialization state");
    require(pIdc2.model(256).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-18, 108, 26).stateIndex,
            "CABAC ctxIdx 256 cabac_init_idc 2 initialization state");
    require(pIdc0.model(11).stateIndex != pIdc1.model(11).stateIndex
                || pIdc0.model(11).valueMps != pIdc1.model(11).valueMps,
            "CABAC cabac_init_idc selects different P context table");
    require(pIdc0.model(24).stateIndex != pIdc1.model(24).stateIndex
                || pIdc0.model(24).valueMps != pIdc1.model(24).valueMps,
            "CABAC cabac_init_idc selects different B-skip context table");
    require(pIdc1.model(27).stateIndex != pIdc2.model(27).stateIndex
                || pIdc1.model(27).valueMps != pIdc2.model(27).valueMps,
            "CABAC cabac_init_idc selects different B mb_type context table");
    require(pIdc0.model(73).stateIndex != pIdc1.model(73).stateIndex
                || pIdc0.model(73).valueMps != pIdc1.model(73).valueMps,
            "CABAC cabac_init_idc selects different coded_block_pattern luma context");
    require(pIdc1.model(77).stateIndex != pIdc2.model(77).stateIndex
                || pIdc1.model(77).valueMps != pIdc2.model(77).valueMps,
            "CABAC cabac_init_idc selects different coded_block_pattern chroma context");
    require(pIdc0.model(97).stateIndex != pIdc1.model(97).stateIndex
                || pIdc0.model(97).valueMps != pIdc1.model(97).valueMps,
            "CABAC cabac_init_idc selects different chroma DC coded_block_flag context");
    require(pIdc0.model(134).stateIndex != pIdc1.model(134).stateIndex
                || pIdc0.model(134).valueMps != pIdc1.model(134).valueMps,
            "CABAC cabac_init_idc selects different luma4x4 significant_coeff_flag context");
    require(pIdc0.model(166).stateIndex != pIdc1.model(166).stateIndex
                || pIdc0.model(166).valueMps != pIdc1.model(166).valueMps,
            "CABAC cabac_init_idc selects different luma4x4 last_significant_coeff_flag context");
    require(pIdc0.model(248).stateIndex != pIdc1.model(248).stateIndex
                || pIdc0.model(248).valueMps != pIdc1.model(248).valueMps,
            "CABAC cabac_init_idc selects different luma4x4 coeff_abs_level_minus1 context");
    require(pIdc0.model(252).stateIndex != pIdc1.model(252).stateIndex
                || pIdc0.model(252).valueMps != pIdc1.model(252).valueMps,
            "CABAC cabac_init_idc selects different luma4x4 coeff_abs_level_minus1 next context");
    require(pIdc0.model(253).stateIndex != pIdc1.model(253).stateIndex
                || pIdc0.model(253).valueMps != pIdc1.model(253).valueMps,
            "CABAC cabac_init_idc selects different luma4x4 coeff_abs_level_minus1 third context");
    require(pIdc0.model(254).stateIndex != pIdc1.model(254).stateIndex
                || pIdc0.model(254).valueMps != pIdc1.model(254).valueMps,
            "CABAC cabac_init_idc selects different luma4x4 coeff_abs_level_minus1 fourth context");
    require(pIdc0.model(255).stateIndex != pIdc1.model(255).stateIndex
                || pIdc0.model(255).valueMps != pIdc1.model(255).valueMps,
            "CABAC cabac_init_idc selects different luma4x4 coeff_abs_level_minus1 fifth context");
    require(pIdc0.model(256).stateIndex != pIdc1.model(256).stateIndex
                || pIdc0.model(256).valueMps != pIdc1.model(256).valueMps,
            "CABAC cabac_init_idc selects different saturated coeff_abs_level_minus1 context");
    const H264CabacContextModelSet intra =
        H264CabacContextModelInitializer::initializeSliceContexts(true, 0, 26, 256);
    require(intra.isInitialized(0), "CABAC intra ctxIdx 0 initialized");
    require(!intra.isInitialized(11), "CABAC intra ctxIdx 11 not initialized in covered subset");
    require(!intra.isInitialized(24), "CABAC intra ctxIdx 24 not initialized in covered subset");
    require(!intra.isInitialized(54), "CABAC intra ctxIdx 54 not initialized in covered subset");
    require(intra.isInitialized(73), "CABAC intra ctxIdx 73 initialized");
    require(intra.isInitialized(77), "CABAC intra ctxIdx 77 initialized");
    require(intra.isInitialized(85), "CABAC intra ctxIdx 85 initialized");
    require(intra.isInitialized(97), "CABAC intra ctxIdx 97 initialized");
    require(intra.isInitialized(134), "CABAC intra ctxIdx 134 initialized");
    require(intra.isInitialized(148), "CABAC intra ctxIdx 148 initialized");
    require(intra.isInitialized(166), "CABAC intra ctxIdx 166 initialized");
    require(intra.isInitialized(180), "CABAC intra ctxIdx 180 initialized");
    require(intra.isInitialized(248), "CABAC intra ctxIdx 248 initialized");
    require(intra.isInitialized(252), "CABAC intra ctxIdx 252 initialized");
    require(intra.isInitialized(253), "CABAC intra ctxIdx 253 initialized");
    require(intra.model(253).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-8, 76, 26).stateIndex,
            "CABAC intra ctxIdx 253 initialization state");
    require(intra.isInitialized(254), "CABAC intra ctxIdx 254 initialized");
    require(intra.model(254).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-7, 80, 26).stateIndex,
            "CABAC intra ctxIdx 254 initialization state");
    require(intra.isInitialized(255), "CABAC intra ctxIdx 255 initialized");
    require(intra.model(255).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-9, 88, 26).stateIndex,
            "CABAC intra ctxIdx 255 initialization state");
    require(intra.isInitialized(256), "CABAC intra ctxIdx 256 initialized");
    require(intra.model(256).stateIndex ==
                H264CabacContextModelInitializer::initializedContextModel(-17, 110, 26).stateIndex,
            "CABAC intra ctxIdx 256 initialization state");

    const H264CabacContextModelSet invalid =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 3, 26, 23);
    require(!invalid.isInitialized(0), "CABAC invalid cabac_init_idc leaves contexts uninitialized");

    const H264CabacContextModelSet lowQp =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, -10, 0);
    const H264CabacContextModelSet clippedLowQp =
        H264CabacContextModelInitializer::initializeSliceContexts(false, 0, 0, 0);
    require(lowQp.model(0).stateIndex == clippedLowQp.model(0).stateIndex,
            "CABAC context set clips low QP state index");
    require(lowQp.model(0).valueMps == clippedLowQp.model(0).valueMps,
            "CABAC context set clips low QP MPS");
}

void testDecodeDecisionMps()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder;
    require(decoder.initialize(reader), "CABAC MPS decoder initializes");

    H264CabacContextModel context;
    context.stateIndex = 0;
    context.valueMps = 0;

    int bin = -1;
    require(decoder.decodeBin(reader, context, &bin), "CABAC decodes MPS bin");
    require(bin == 0, "CABAC MPS bin value");
    require(context.stateIndex == 1, "CABAC MPS state transition");
    require(context.valueMps == 0, "CABAC MPS keeps valueMps");
    require(decoder.state().codIRange == 270, "CABAC MPS range update");
    require(decoder.state().codIOffset == 0, "CABAC MPS offset update");
    require(reader.bitOffset() == 9, "CABAC MPS no renormalization bit needed");
}

void testDecodeDecisionLps()
{
    BitReader reader(QByteArray::fromHex("ff80"));
    H264CabacDecoder decoder;
    require(decoder.initialize(reader), "CABAC LPS decoder initializes");

    H264CabacContextModel context;
    context.stateIndex = 0;
    context.valueMps = 0;

    int bin = -1;
    require(decoder.decodeBin(reader, context, &bin), "CABAC decodes LPS bin");
    require(bin == 1, "CABAC LPS bin value");
    require(context.stateIndex == 0, "CABAC LPS state transition");
    require(context.valueMps == 1, "CABAC LPS toggles valueMps at state 0");
    require(decoder.state().codIRange == 480, "CABAC LPS renormalized range");
    require(decoder.state().codIOffset == 482, "CABAC LPS renormalized offset");
    require(reader.bitOffset() == 10, "CABAC LPS consumes renormalization bit");
}

void testDecodeDecisionFromContextSet()
{
    BitReader reader(QByteArray::fromHex("0000"));
    H264CabacDecoder decoder;
    require(decoder.initialize(reader), "CABAC context-set decoder initializes");

    H264CabacContextModelSet contexts(1);
    H264CabacContextModel context;
    context.stateIndex = 0;
    context.valueMps = 0;
    contexts.setModel(0, context);

    int bin = -1;
    require(decoder.decodeBin(reader, contexts, 0, &bin), "CABAC decodes bin from context set");
    require(bin == 0, "CABAC context-set bin value");
    require(contexts.model(0).stateIndex == 1, "CABAC context-set stores updated state");
    require(!decoder.decodeBin(reader, contexts, 1, &bin), "CABAC rejects uninitialized context");
}

void testDecodeBypassBin()
{
    BitReader zeroReader(QByteArray::fromHex("1900")); // first 9 bits 000110010, next bit 0
    H264CabacDecoder zeroDecoder;
    require(zeroDecoder.initialize(zeroReader), "CABAC bypass zero decoder initializes");
    int bin = -1;
    require(zeroDecoder.decodeBypassBin(zeroReader, &bin), "CABAC decodes bypass zero");
    require(bin == 0, "CABAC bypass zero bin");
    require(zeroDecoder.state().codIOffset == 100, "CABAC bypass zero offset");
    require(zeroReader.bitOffset() == 10, "CABAC bypass zero consumes one bit");

    BitReader oneReader(QByteArray::fromHex("9600")); // first 9 bits 100101100, next bit 0
    H264CabacDecoder oneDecoder;
    require(oneDecoder.initialize(oneReader), "CABAC bypass one decoder initializes");
    require(oneDecoder.decodeBypassBin(oneReader, &bin), "CABAC decodes bypass one");
    require(bin == 1, "CABAC bypass one bin");
    require(oneDecoder.state().codIOffset == 90, "CABAC bypass one offset");
}

void testDecodeTerminate()
{
    BitReader zeroReader(QByteArray::fromHex("0000"));
    H264CabacDecoder zeroDecoder;
    require(zeroDecoder.initialize(zeroReader), "CABAC terminate zero decoder initializes");
    int bin = -1;
    require(zeroDecoder.decodeTerminate(zeroReader, &bin), "CABAC decodes terminate zero");
    require(bin == 0, "CABAC terminate zero bin");
    require(zeroDecoder.state().codIRange == 508, "CABAC terminate zero range");
    require(zeroReader.bitOffset() == 9, "CABAC terminate zero no renormalization bit");

    BitReader oneReader(QByteArray::fromHex("ff80"));
    H264CabacDecoder oneDecoder;
    require(oneDecoder.initialize(oneReader), "CABAC terminate one decoder initializes");
    require(oneDecoder.decodeTerminate(oneReader, &bin), "CABAC decodes terminate one");
    require(bin == 1, "CABAC terminate one bin");
    require(oneDecoder.state().codIRange == 508, "CABAC terminate one range");
    require(oneReader.bitOffset() == 9, "CABAC terminate one consumes no extra bit");
}
}

int main()
{
    testConsumeAlignmentBits();
    testInitializeDecoderState();
    testContextModelInitialization();
    testContextModelSetInitialization();
    testDecodeDecisionMps();
    testDecodeDecisionLps();
    testDecodeDecisionFromContextSet();
    testDecodeBypassBin();
    testDecodeTerminate();
    std::cout << "H264CabacDecoder tests passed\n";
    return 0;
}
