#include "core/export/AnalysisExportWriter.h"
#include "core/analysis/AnalysisStats.h"
#include "core/parser/audio/AacAdtsParser.h"
#include "core/parser/video/h264/H264FrameAnalysisAdapter.h"
#include "core/parser/video/h264/H264Parser.h"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <cstdlib>
#include <iostream>

namespace
{
class BitWriter
{
public:
    void writeBit(bool bit)
    {
        if (m_bitOffset == 0) {
            m_data.append(char(0));
        }
        if (bit) {
            m_data[m_data.size() - 1] = char(static_cast<unsigned char>(m_data[m_data.size() - 1])
                                             | static_cast<unsigned char>(1U << (7 - m_bitOffset)));
        }
        m_bitOffset = (m_bitOffset + 1) % 8;
    }

    void writeBits(quint32 value, int count)
    {
        for (int bit = count - 1; bit >= 0; --bit) {
            writeBit(((value >> bit) & 1U) != 0);
        }
    }

    void writeUE(quint32 value)
    {
        const quint32 codeNum = value + 1;
        int leadingZeroBits = 0;
        for (quint32 probe = codeNum; probe > 1; probe >>= 1) {
            ++leadingZeroBits;
        }
        for (int i = 0; i < leadingZeroBits; ++i) {
            writeBit(false);
        }
        writeBits(codeNum, leadingZeroBits + 1);
    }

    void writeSE(qint32 value)
    {
        const quint32 codeNum = value <= 0
            ? static_cast<quint32>(-value * 2)
            : static_cast<quint32>(value * 2 - 1);
        writeUE(codeNum);
    }

    QByteArray finishRbsp()
    {
        writeBit(true);
        while (m_bitOffset != 0) {
            writeBit(false);
        }
        return m_data;
    }

    QByteArray data() const
    {
        return m_data;
    }

private:
    QByteArray m_data;
    int m_bitOffset = 0;
};

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

QByteArray loadFixture(const QString &name)
{
    const QString path = QStringLiteral(H264_ANALYZER_TEST_FIXTURE_DIR) + QLatin1Char('/') + name;
    QFile file(path);
    require(file.open(QIODevice::ReadOnly | QIODevice::Text), "open fixture");
    QByteArray hex = file.readAll();
    hex = hex.simplified();
    hex.replace(" ", "");
    const QByteArray data = QByteArray::fromHex(hex);
    require(!data.isEmpty(), "decode fixture hex");
    return data;
}

QByteArray loadBinaryFixture(const QString &name)
{
    QFile file(QStringLiteral(H264_ANALYZER_TEST_FIXTURE_DIR) + QLatin1Char('/') + name);
    require(file.open(QIODevice::ReadOnly), "open binary fixture");
    return file.readAll();
}

const SliceInfo &firstSlice(const FrameSyntaxInfo &frame, const char *message)
{
    require(!frame.slices.isEmpty(), message);
    return frame.slices.first();
}

bool hasDiagnosticCode(const SliceInfo &slice, const QString &code)
{
    for (const ParserDiagnosticInfo &diagnostic : slice.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

bool hasFrameDiagnosticCode(const FrameSyntaxInfo &frame, const QString &code)
{
    for (const ParserDiagnosticInfo &diagnostic : frame.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

bool hasNaluDiagnosticCode(const NaluInfo &nalu, const QString &code)
{
    for (const ParserDiagnosticInfo &diagnostic : nalu.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

bool hasAnalysisDiagnosticCode(const FrameAnalysis &analysis, const QString &code)
{
    for (const AnalysisDiagnostic &diagnostic : analysis.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

quint32 readBit(const QByteArray &data, qsizetype &bitOffset, bool &ok)
{
    if (!ok || bitOffset >= data.size() * 8) {
        ok = false;
        return 0;
    }

    const qsizetype byteOffset = bitOffset / 8;
    const int bitInByte = int(7 - (bitOffset % 8));
    ++bitOffset;
    return (quint8(data.at(byteOffset)) >> bitInByte) & 1U;
}

quint32 decodeUnsignedExpGolomb(const QByteArray &data, bool *ok = nullptr)
{
    bool localOk = true;
    qsizetype bitOffset = 0;
    int leadingZeroBits = 0;
    while (readBit(data, bitOffset, localOk) == 0U && localOk) {
        ++leadingZeroBits;
    }

    quint32 value = 1U;
    for (int i = 0; i < leadingZeroBits; ++i) {
        value = (value << 1) | readBit(data, bitOffset, localOk);
    }

    if (ok != nullptr) {
        *ok = localOk;
    }
    return localOk ? value - 1U : 0U;
}

qint32 decodeSignedExpGolomb(const QByteArray &data, bool *ok = nullptr)
{
    const quint32 codeNum = decodeUnsignedExpGolomb(data, ok);
    return (codeNum & 1U) != 0U
        ? qint32((codeNum + 1U) / 2U)
        : -qint32(codeNum / 2U);
}

QByteArray makeMinimalSpsNalu(int widthInMbs = 1, int heightInMapUnits = 1)
{
    BitWriter rbsp;
    rbsp.writeBits(66, 8); // profile_idc: baseline
    rbsp.writeBits(0, 8);  // constraint flags + reserved bits
    rbsp.writeBits(30, 8); // level_idc
    rbsp.writeUE(0);       // seq_parameter_set_id
    rbsp.writeUE(0);       // log2_max_frame_num_minus4
    rbsp.writeUE(0);       // pic_order_cnt_type
    rbsp.writeUE(0);       // log2_max_pic_order_cnt_lsb_minus4
    rbsp.writeUE(1);       // max_num_ref_frames
    rbsp.writeBit(false);  // gaps_in_frame_num_value_allowed_flag
    rbsp.writeUE(widthInMbs > 0 ? quint32(widthInMbs - 1) : 0); // pic_width_in_mbs_minus1
    rbsp.writeUE(heightInMapUnits > 0 ? quint32(heightInMapUnits - 1) : 0); // pic_height_in_map_units_minus1
    rbsp.writeBit(true);   // frame_mbs_only_flag
    rbsp.writeBit(true);   // direct_8x8_inference_flag
    rbsp.writeBit(false);  // frame_cropping_flag
    rbsp.writeBit(false);  // vui_parameters_present_flag

    QByteArray nalu;
    nalu.append(char(0x67));
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeMinimalPpsNalu(int numRefIdxL0DefaultActiveMinus1 = 0,
                              int numRefIdxL1DefaultActiveMinus1 = 0)
{
    BitWriter rbsp;
    rbsp.writeUE(0);                                      // pic_parameter_set_id
    rbsp.writeUE(0);                                      // seq_parameter_set_id
    rbsp.writeBit(false);                                 // entropy_coding_mode_flag: CAVLC
    rbsp.writeBit(false);                                 // bottom_field_pic_order_in_frame_present_flag
    rbsp.writeUE(0);                                      // num_slice_groups_minus1
    rbsp.writeUE(numRefIdxL0DefaultActiveMinus1);         // num_ref_idx_l0_default_active_minus1
    rbsp.writeUE(numRefIdxL1DefaultActiveMinus1);         // num_ref_idx_l1_default_active_minus1
    rbsp.writeBit(false);                                 // weighted_pred_flag
    rbsp.writeBits(0, 2);                                 // weighted_bipred_idc
    rbsp.writeSE(0);                                      // pic_init_qp_minus26
    rbsp.writeSE(0);                                      // pic_init_qs_minus26
    rbsp.writeSE(0);                                      // chroma_qp_index_offset
    rbsp.writeBit(true);                                  // deblocking_filter_control_present_flag
    rbsp.writeBit(false);                                 // constrained_intra_pred_flag
    rbsp.writeBit(false);                                 // redundant_pic_cnt_present_flag

    QByteArray nalu;
    nalu.append(char(0x68));
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makePMultiMacroblockPredictionSliceNalu()
{
    BitWriter rbsp;
    rbsp.writeUE(0);         // first_mb_in_slice
    rbsp.writeUE(0);         // slice_type: P
    rbsp.writeUE(0);         // pic_parameter_set_id
    rbsp.writeBits(0, 4);    // frame_num
    rbsp.writeBits(0, 4);    // pic_order_cnt_lsb
    rbsp.writeBit(false);    // num_ref_idx_active_override_flag
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l0
    rbsp.writeBit(false);    // adaptive_ref_pic_marking_mode_flag
    rbsp.writeSE(0);         // slice_qp_delta
    rbsp.writeUE(1);         // disable_deblocking_filter_idc

    rbsp.writeUE(0);         // mb_skip_run
    rbsp.writeUE(0);         // mb_type: P_L0_16x16
    rbsp.writeSE(2);         // mvd_l0[0][0]
    rbsp.writeSE(0);         // mvd_l0[0][1]
    rbsp.writeUE(0);         // coded_block_pattern

    rbsp.writeUE(0);         // mb_skip_run
    rbsp.writeUE(0);         // mb_type: P_L0_16x16
    rbsp.writeSE(1);         // mvd_l0[0][0], predicted from left macroblock
    rbsp.writeSE(-1);        // mvd_l0[0][1], predicted from left macroblock
    rbsp.writeUE(0);         // coded_block_pattern

    QByteArray nalu;
    nalu.append(char(0x41)); // nal_ref_idc=2, nal_unit_type=1
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeP8x8SliceNalu(int mbType, int numRefIdxL0DefaultActiveMinus1 = 0)
{
    BitWriter rbsp;
    rbsp.writeUE(0);         // first_mb_in_slice
    rbsp.writeUE(0);         // slice_type: P
    rbsp.writeUE(0);         // pic_parameter_set_id
    rbsp.writeBits(0, 4);    // frame_num
    rbsp.writeBits(0, 4);    // pic_order_cnt_lsb
    rbsp.writeBit(false);    // num_ref_idx_active_override_flag
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l0
    rbsp.writeBit(false);    // adaptive_ref_pic_marking_mode_flag
    rbsp.writeSE(0);         // slice_qp_delta
    rbsp.writeUE(1);         // disable_deblocking_filter_idc

    rbsp.writeUE(0);         // mb_skip_run
    rbsp.writeUE(mbType);    // mb_type: P_8x8 or P_8x8ref0
    for (int i = 0; i < 4; ++i) {
        rbsp.writeUE(0);     // sub_mb_type: P_L0_8x8
    }
    if (numRefIdxL0DefaultActiveMinus1 > 0 && mbType != 4) {
        rbsp.writeBit(false); // ref_idx_l0[0] as te(v), range 1 -> 1
        rbsp.writeBit(true);  // ref_idx_l0[1] as te(v), range 1 -> 0
        rbsp.writeBit(true);  // ref_idx_l0[2] as te(v), range 1 -> 0
        rbsp.writeBit(true);  // ref_idx_l0[3] as te(v), range 1 -> 0
    }

    rbsp.writeSE(2);
    rbsp.writeSE(-1);
    rbsp.writeSE(0);
    rbsp.writeSE(1);
    rbsp.writeSE(-2);
    rbsp.writeSE(0);
    rbsp.writeSE(1);
    rbsp.writeSE(1);
    rbsp.writeUE(0);         // coded_block_pattern

    QByteArray nalu;
    nalu.append(char(0x41)); // nal_ref_idc=2, nal_unit_type=1
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeI16x16NonZeroResidualSliceNalu()
{
    BitWriter rbsp;
    rbsp.writeUE(0);         // first_mb_in_slice
    rbsp.writeUE(2);         // slice_type: I
    rbsp.writeUE(0);         // pic_parameter_set_id
    rbsp.writeBits(0, 4);    // frame_num
    rbsp.writeBits(0, 4);    // pic_order_cnt_lsb
    rbsp.writeBit(false);    // adaptive_ref_pic_marking_mode_flag
    rbsp.writeSE(0);         // slice_qp_delta
    rbsp.writeUE(1);         // disable_deblocking_filter_idc

    rbsp.writeUE(13);        // mb_type: I_16x16 with luma AC present, chroma DC/AC absent
    rbsp.writeUE(0);         // intra_chroma_pred_mode
    rbsp.writeSE(0);         // mb_qp_delta

    rbsp.writeBit(true);     // luma16x16 DC coeff_token: total_coeff=0

    rbsp.writeBit(false);    // luma16x16 AC[0] coeff_token: total_coeff=1 trailing_ones=1
    rbsp.writeBit(true);
    rbsp.writeBit(false);    // trailing_one_sign_flag: positive
    rbsp.writeBit(true);     // total_zeros: 0

    for (int i = 1; i < 16; ++i) {
        rbsp.writeBit(true); // luma16x16 AC[i] coeff_token: total_coeff=0
    }

    QByteArray nalu;
    nalu.append(char(0x41)); // nal_ref_idc=2, nal_unit_type=1
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeBSliceNalu()
{
    BitWriter rbsp;
    rbsp.writeUE(0);         // first_mb_in_slice
    rbsp.writeUE(1);         // slice_type: B
    rbsp.writeUE(0);         // pic_parameter_set_id
    rbsp.writeBits(0, 4);    // frame_num
    rbsp.writeBits(0, 4);    // pic_order_cnt_lsb
    rbsp.writeBit(true);     // direct_spatial_mv_pred_flag
    rbsp.writeBit(false);    // num_ref_idx_active_override_flag
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l0
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l1
    rbsp.writeBit(false);    // adaptive_ref_pic_marking_mode_flag
    rbsp.writeSE(0);         // slice_qp_delta
    rbsp.writeUE(1);         // disable_deblocking_filter_idc

    rbsp.writeUE(0);         // mb_skip_run
    rbsp.writeUE(0);         // mb_type: B_Direct_16x16

    QByteArray nalu;
    nalu.append(char(0x41)); // nal_ref_idc=2, nal_unit_type=1
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeB8x8UnsupportedSliceNalu()
{
    BitWriter rbsp;
    rbsp.writeUE(0);         // first_mb_in_slice
    rbsp.writeUE(1);         // slice_type: B
    rbsp.writeUE(0);         // pic_parameter_set_id
    rbsp.writeBits(0, 4);    // frame_num
    rbsp.writeBits(0, 4);    // pic_order_cnt_lsb
    rbsp.writeBit(true);     // direct_spatial_mv_pred_flag
    rbsp.writeBit(false);    // num_ref_idx_active_override_flag
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l0
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l1
    rbsp.writeBit(false);    // adaptive_ref_pic_marking_mode_flag
    rbsp.writeSE(0);         // slice_qp_delta
    rbsp.writeUE(1);         // disable_deblocking_filter_idc

    rbsp.writeUE(0);         // mb_skip_run
    rbsp.writeUE(22);        // mb_type: B_8x8

    QByteArray nalu;
    nalu.append(char(0x41)); // nal_ref_idc=2, nal_unit_type=1
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeBMotionSliceNalu()
{
    BitWriter rbsp;
    rbsp.writeUE(0);         // first_mb_in_slice
    rbsp.writeUE(1);         // slice_type: B
    rbsp.writeUE(0);         // pic_parameter_set_id
    rbsp.writeBits(0, 4);    // frame_num
    rbsp.writeBits(0, 4);    // pic_order_cnt_lsb
    rbsp.writeBit(true);     // direct_spatial_mv_pred_flag
    rbsp.writeBit(false);    // num_ref_idx_active_override_flag
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l0
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l1
    rbsp.writeBit(false);    // adaptive_ref_pic_marking_mode_flag
    rbsp.writeSE(0);         // slice_qp_delta
    rbsp.writeUE(1);         // disable_deblocking_filter_idc

    rbsp.writeUE(0);         // mb_skip_run
    rbsp.writeUE(3);         // mb_type: B_Bi_16x16
    rbsp.writeSE(2);         // mvd_l0[0][0][0]
    rbsp.writeSE(-1);        // mvd_l0[0][0][1]
    rbsp.writeSE(-2);        // mvd_l1[0][0][0]
    rbsp.writeSE(1);         // mvd_l1[0][0][1]
    rbsp.writeUE(0);         // coded_block_pattern

    QByteArray nalu;
    nalu.append(char(0x41)); // nal_ref_idc=2, nal_unit_type=1
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeBPartitionedMotionSliceNalu()
{
    BitWriter rbsp;
    rbsp.writeUE(0);         // first_mb_in_slice
    rbsp.writeUE(1);         // slice_type: B
    rbsp.writeUE(0);         // pic_parameter_set_id
    rbsp.writeBits(0, 4);    // frame_num
    rbsp.writeBits(0, 4);    // pic_order_cnt_lsb
    rbsp.writeBit(true);     // direct_spatial_mv_pred_flag
    rbsp.writeBit(false);    // num_ref_idx_active_override_flag
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l0
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l1
    rbsp.writeBit(false);    // adaptive_ref_pic_marking_mode_flag
    rbsp.writeSE(0);         // slice_qp_delta
    rbsp.writeUE(1);         // disable_deblocking_filter_idc

    rbsp.writeUE(0);         // mb_skip_run
    rbsp.writeUE(8);         // mb_type: B_L0_L1_16x8
    rbsp.writeSE(2);         // mvd_l0[0][0]
    rbsp.writeSE(-1);        // mvd_l0[0][1]
    rbsp.writeSE(-3);        // mvd_l1[1][0]
    rbsp.writeSE(1);         // mvd_l1[1][1]
    rbsp.writeUE(0);         // coded_block_pattern

    QByteArray nalu;
    nalu.append(char(0x41)); // nal_ref_idc=2, nal_unit_type=1
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeB8x16PartitionedMotionSliceNalu()
{
    BitWriter rbsp;
    rbsp.writeUE(0);         // first_mb_in_slice
    rbsp.writeUE(1);         // slice_type: B
    rbsp.writeUE(0);         // pic_parameter_set_id
    rbsp.writeBits(0, 4);    // frame_num
    rbsp.writeBits(0, 4);    // pic_order_cnt_lsb
    rbsp.writeBit(true);     // direct_spatial_mv_pred_flag
    rbsp.writeBit(false);    // num_ref_idx_active_override_flag
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l0
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l1
    rbsp.writeBit(false);    // adaptive_ref_pic_marking_mode_flag
    rbsp.writeSE(0);         // slice_qp_delta
    rbsp.writeUE(1);         // disable_deblocking_filter_idc

    rbsp.writeUE(0);         // mb_skip_run
    rbsp.writeUE(11);        // mb_type: B_L1_L0_8x16
    rbsp.writeSE(4);         // mvd_l0[1][0]
    rbsp.writeSE(-2);        // mvd_l0[1][1]
    rbsp.writeSE(-1);        // mvd_l1[0][0]
    rbsp.writeSE(3);         // mvd_l1[0][1]
    rbsp.writeUE(0);         // coded_block_pattern

    QByteArray nalu;
    nalu.append(char(0x41)); // nal_ref_idc=2, nal_unit_type=1
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeBBiBiPartitionedMotionSliceNalu()
{
    BitWriter rbsp;
    rbsp.writeUE(0);         // first_mb_in_slice
    rbsp.writeUE(1);         // slice_type: B
    rbsp.writeUE(0);         // pic_parameter_set_id
    rbsp.writeBits(0, 4);    // frame_num
    rbsp.writeBits(0, 4);    // pic_order_cnt_lsb
    rbsp.writeBit(true);     // direct_spatial_mv_pred_flag
    rbsp.writeBit(false);    // num_ref_idx_active_override_flag
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l0
    rbsp.writeBit(false);    // ref_pic_list_modification_flag_l1
    rbsp.writeBit(false);    // adaptive_ref_pic_marking_mode_flag
    rbsp.writeSE(0);         // slice_qp_delta
    rbsp.writeUE(1);         // disable_deblocking_filter_idc

    rbsp.writeUE(0);         // mb_skip_run
    rbsp.writeUE(20);        // mb_type: B_Bi_Bi_16x8
    rbsp.writeSE(1);         // mvd_l0[0][0]
    rbsp.writeSE(0);         // mvd_l0[0][1]
    rbsp.writeSE(-1);        // mvd_l0[1][0]
    rbsp.writeSE(2);         // mvd_l0[1][1]
    rbsp.writeSE(0);         // mvd_l1[0][0]
    rbsp.writeSE(-2);        // mvd_l1[0][1]
    rbsp.writeSE(3);         // mvd_l1[1][0]
    rbsp.writeSE(1);         // mvd_l1[1][1]
    rbsp.writeUE(0);         // coded_block_pattern

    QByteArray nalu;
    nalu.append(char(0x41)); // nal_ref_idc=2, nal_unit_type=1
    nalu.append(rbsp.finishRbsp());
    return nalu;
}

QByteArray makeAnnexBPacket(std::initializer_list<QByteArray> nalus)
{
    QByteArray packet;
    for (const QByteArray &nalu : nalus) {
        packet.append(char(0x00));
        packet.append(char(0x00));
        packet.append(char(0x01));
        packet.append(nalu);
    }
    return packet;
}

void testExpGolomb()
{
    BitWriter ueWriter;
    ueWriter.writeUE(5);
    bool ok = false;
    require(decodeUnsignedExpGolomb(ueWriter.data(), &ok) == 5 && ok,
            "unsigned Exp-Golomb value 5");

    BitWriter seWriter;
    seWriter.writeSE(-2);
    require(decodeSignedExpGolomb(seWriter.data(), &ok) == -2 && ok,
            "signed Exp-Golomb value -2");
}

void testAnnexBSps()
{
    QByteArray packet;
    packet.append(char(0x00));
    packet.append(char(0x00));
    packet.append(char(0x01));
    packet.append(makeMinimalSpsNalu());

    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    require(frame.codecKind == CodecKind::H264, "Annex B codec kind");
    require(frame.nalus.size() == 1, "Annex B NALU count");
    require(frame.nalus[0].sps.valid, "Annex B SPS validity");
    require(frame.nalus[0].sps.width == 16, "Annex B SPS width");
    require(frame.nalus[0].sps.height == 16, "Annex B SPS height");
}

void testAvccSps()
{
    const QByteArray nalu = makeMinimalSpsNalu();
    QByteArray packet;
    const int size = nalu.size();
    packet.append(char((size >> 24) & 0xff));
    packet.append(char((size >> 16) & 0xff));
    packet.append(char((size >> 8) & 0xff));
    packet.append(char(size & 0xff));
    packet.append(nalu);

    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    require(frame.codecKind == CodecKind::H264, "AVCC codec kind");
    require(frame.nalus.size() == 1, "AVCC NALU count");
    require(frame.nalus[0].sps.valid, "AVCC SPS validity");
    require(frame.nalus[0].sps.width == 16, "AVCC SPS width");
    require(frame.nalus[0].sps.height == 16, "AVCC SPS height");
}

void testAnnexBFixtureWithIdr()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("annexb_sps_pps_idr_i.hex")), 0, 0, 0);
    require(frame.nalus.size() == 3, "fixture Annex B NALU count");
    require(frame.slices.size() == 1, "fixture Annex B frame slice count");
    require(frame.nalus[0].sps.valid, "fixture Annex B SPS validity");
    require(frame.nalus[0].sps.width == 16, "fixture Annex B SPS width");
    require(frame.nalus[0].sps.height == 16, "fixture Annex B SPS height");

    const SliceInfo &slice = firstSlice(frame, "fixture Annex B frame has slice");
    require(slice.sliceTypeName == QStringLiteral("I"), "fixture IDR slice type");
    require(slice.macroblocks.size() == 1, "fixture IDR macroblock count");
    require(slice.macroblocks.first().parsed, "fixture IDR macroblock parsed");
}

void testAvccFixtureWithSpsPps()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("avcc_sps_pps.hex")), 0, 0, 0);
    require(frame.nalus.size() == 2, "fixture AVCC NALU count");
    require(frame.nalus[0].sps.valid, "fixture AVCC SPS validity");
    require(frame.nalus[1].pps.valid, "fixture AVCC PPS validity");
    require(frame.nalus[0].sps.width == 16, "fixture AVCC SPS width");
    require(frame.nalus[0].sps.height == 16, "fixture AVCC SPS height");
}

void testAvccLengthExceedsPacketReportsDiagnostic()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("avcc_length_exceeds_packet.hex")), 0, 0, 0);
    require(frame.nalus.isEmpty(), "AVCC overrun fixture has no complete NALU");
    require(hasFrameDiagnosticCode(frame, QStringLiteral("avcc_nalu_length_exceeds_packet")),
            "AVCC overrun fixture reports structured diagnostic");
    require(frame.slices.isEmpty(), "AVCC overrun fixture has no slices");

    const FrameAnalysis analysis = parser.parsePacket(loadFixture(QStringLiteral("avcc_length_exceeds_packet.hex")), 0, 0, 0);
    require(hasAnalysisDiagnosticCode(analysis, QStringLiteral("avcc_nalu_length_exceeds_packet")),
            "AVCC overrun diagnostic is exposed in FrameAnalysis");
}

void testTruncatedSpsReportsDiagnostic()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("truncated_sps.hex")), 0, 0, 0);
    require(frame.nalus.size() == 1, "truncated SPS fixture NALU count");
    require(frame.nalus.first().nalUnitType == 7, "truncated SPS fixture NALU type");
    require(!frame.nalus.first().sps.valid, "truncated SPS fixture is invalid");
    require(hasNaluDiagnosticCode(frame.nalus.first(), QStringLiteral("sps_truncated")),
            "truncated SPS fixture reports structured diagnostic");
    require(parser.spsMap().isEmpty(), "truncated SPS fixture is not cached");

    const FrameAnalysis analysis = parser.parsePacket(loadFixture(QStringLiteral("truncated_sps.hex")), 0, 0, 0);
    require(hasAnalysisDiagnosticCode(analysis, QStringLiteral("sps_truncated")),
            "truncated SPS diagnostic is exposed in FrameAnalysis");
}

void testTruncatedPpsReportsDiagnostic()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("truncated_pps.hex")), 0, 0, 0);
    require(frame.nalus.size() == 1, "truncated PPS fixture NALU count");
    require(frame.nalus.first().nalUnitType == 8, "truncated PPS fixture NALU type");
    require(!frame.nalus.first().pps.valid, "truncated PPS fixture is invalid");
    require(hasNaluDiagnosticCode(frame.nalus.first(), QStringLiteral("pps_truncated")),
            "truncated PPS fixture reports structured diagnostic");
    require(parser.ppsMap().isEmpty(), "truncated PPS fixture is not cached");

    const FrameAnalysis analysis = parser.parsePacket(loadFixture(QStringLiteral("truncated_pps.hex")), 0, 0, 0);
    require(hasAnalysisDiagnosticCode(analysis, QStringLiteral("pps_truncated")),
            "truncated PPS diagnostic is exposed in FrameAnalysis");
}

void testCavlcIFrameQpDeltaFixture()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("cavlc_i_qp_delta.hex")), 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC I fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("I"), "CAVLC I fixture slice type");
    require(slice.macroblocks.size() == 2, "CAVLC I fixture macroblock count");
    require(slice.macroblocks.first().mbQpDelta == 3, "CAVLC I fixture non-zero mb_qp_delta");
    require(slice.macroblocks.first().qp == 29, "CAVLC I fixture derived macroblock QP");
    require(slice.macroblocks.first().residualParsed, "CAVLC I fixture residual parsed");
    require(slice.macroblocks.first().residualBlockCount == 17, "CAVLC I fixture residual block count");
    require(slice.macroblocks.first().residualCoefficientCount == 0, "CAVLC I fixture zero residual coefficients");
    require(slice.macroblocks.first().residualBlocks.size() == 17, "CAVLC I fixture residual block summaries");
    require(slice.macroblocks.first().residualBlocks.first().kind == QStringLiteral("luma16x16_dc"),
            "CAVLC I fixture first residual block kind");
    require(slice.macroblocks.first().residualBlocks.first().maxCoefficientCount == 16,
            "CAVLC I fixture first residual block max coeffs");
    require(slice.macroblocks.first().residualBlocks.first().totalCoefficientCount == 0,
            "CAVLC I fixture first residual block coeff count");
    require(slice.macroblocks.first().residualBlocks.last().kind == QStringLiteral("luma16x16_ac"),
            "CAVLC I fixture last residual block kind");
    require(slice.macroblocks.first().residualBlocks.last().blockIndex == 15,
            "CAVLC I fixture last residual block index");
    require(slice.macroblocks[1].parsed, "CAVLC I fixture continues after residual macroblock");
    require(!hasDiagnosticCode(slice, QStringLiteral("cavlc_residual_unsupported")),
            "CAVLC I fixture has no residual unsupported diagnostic");
}

void testCavlcPSkipFixture()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("cavlc_p_skip.hex")), 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC P skip fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "CAVLC P skip fixture slice type");
    require(slice.macroblocks.size() == 1, "CAVLC P skip fixture macroblock count");
    require(slice.macroblocks.first().skipped, "CAVLC P skip fixture skipped macroblock");
    require(!slice.macroblocks.first().motionVectors.isEmpty(), "CAVLC P skip fixture motion vector");
}

void testCavlcPMotionVectorFixture()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("cavlc_p_motion_vector.hex")), 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC P MV fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "CAVLC P MV fixture slice type");
    require(slice.macroblocks.size() == 1, "CAVLC P MV fixture macroblock count");
    require(!slice.macroblocks.first().motionVectors.isEmpty(), "CAVLC P MV fixture has motion vector");

    const MotionVectorInfo &mv = slice.macroblocks.first().motionVectors.first();
    require(mv.mvXQuarterPel == 2, "CAVLC P MV fixture mv_x");
    require(mv.mvYQuarterPel == -1, "CAVLC P MV fixture mv_y");
}

void testCavlcPMultiMacroblockMotionVectorPredictionFixture()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(2, 1),
        makeMinimalPpsNalu(),
        makePMultiMacroblockPredictionSliceNalu()
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC P multi-macroblock MV fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "CAVLC P multi-macroblock MV fixture slice type");
    require(slice.picWidthInMbs == 2, "CAVLC P multi-macroblock MV fixture width");
    require(slice.macroblocks.size() == 2, "CAVLC P multi-macroblock MV fixture macroblock count");
    require(slice.macroblocks[0].address == 0, "CAVLC P multi-macroblock MV fixture first address");
    require(slice.macroblocks[1].address == 1, "CAVLC P multi-macroblock MV fixture second address");
    require(slice.macroblocks[0].motionVectors.size() == 1, "CAVLC P multi-macroblock first MV count");
    require(slice.macroblocks[1].motionVectors.size() == 1, "CAVLC P multi-macroblock second MV count");

    const MotionVectorInfo &firstMv = slice.macroblocks[0].motionVectors.first();
    require(firstMv.mvXQuarterPel == 2, "CAVLC P multi-macroblock first mv_x");
    require(firstMv.mvYQuarterPel == 0, "CAVLC P multi-macroblock first mv_y");

    const MotionVectorInfo &secondMv = slice.macroblocks[1].motionVectors.first();
    require(secondMv.mvXQuarterPel == 3, "CAVLC P multi-macroblock second mv_x uses left prediction");
    require(secondMv.mvYQuarterPel == -1, "CAVLC P multi-macroblock second mv_y uses left prediction");

    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);
    require(analysis.motionVectors.size() == 2, "CAVLC P multi-macroblock FrameAnalysis MV count");
    require(analysis.motionVectors[0].regionAddress == 0, "CAVLC P multi-macroblock FrameAnalysis first region");
    require(analysis.motionVectors[1].regionAddress == 1, "CAVLC P multi-macroblock FrameAnalysis second region");
    require(analysis.motionVectors[1].mvXQuarterPel == 3, "CAVLC P multi-macroblock FrameAnalysis second mv_x");
    require(analysis.motionVectors[1].mvYQuarterPel == -1, "CAVLC P multi-macroblock FrameAnalysis second mv_y");
}

void testCavlcP8x8MotionVectorFixture()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeP8x8SliceNalu(3)
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC P_8x8 fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "CAVLC P_8x8 fixture slice type");
    require(slice.macroblocks.size() == 1, "CAVLC P_8x8 fixture macroblock count");
    require(slice.macroblocks.first().mbType == QStringLiteral("P_8x8"), "CAVLC P_8x8 fixture mb_type");
    require(slice.macroblocks.first().parsed, "CAVLC P_8x8 fixture macroblock parsed");
    require(slice.macroblocks.first().motionVectors.size() == 4, "CAVLC P_8x8 fixture motion vector count");
    require(!hasDiagnosticCode(slice, QStringLiteral("p8x8_sub_macroblock_unsupported")),
            "CAVLC P_8x8 fixture has no unsupported diagnostic");

    const QVector<MotionVectorInfo> &mvs = slice.macroblocks.first().motionVectors;
    require(mvs[0].mvXQuarterPel == 2 && mvs[0].mvYQuarterPel == -1, "CAVLC P_8x8 fixture first mv");
    require(mvs[1].mvXQuarterPel == 0 && mvs[1].mvYQuarterPel == 1, "CAVLC P_8x8 fixture second mv");
    require(mvs[2].mvXQuarterPel == -2 && mvs[2].mvYQuarterPel == 0, "CAVLC P_8x8 fixture third mv");
    require(mvs[3].mvXQuarterPel == 1 && mvs[3].mvYQuarterPel == 1, "CAVLC P_8x8 fixture fourth mv");
}

void testCavlcP8x8Ref0MotionVectorFixture()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(1),
        makeP8x8SliceNalu(4, 1)
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC P_8x8ref0 fixture has slice");
    require(slice.macroblocks.size() == 1, "CAVLC P_8x8ref0 fixture macroblock count");
    require(slice.macroblocks.first().mbType == QStringLiteral("P_8x8ref0"), "CAVLC P_8x8ref0 fixture mb_type");
    require(slice.macroblocks.first().motionVectors.size() == 4, "CAVLC P_8x8ref0 fixture motion vector count");
    for (const MotionVectorInfo &mv : slice.macroblocks.first().motionVectors) {
        require(mv.referenceIndex == 0, "CAVLC P_8x8ref0 fixture forces ref_idx_l0 to zero");
    }
}

void testCavlcResidualCoefficientFixture()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeI16x16NonZeroResidualSliceNalu()
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC coefficient fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("I"), "CAVLC coefficient fixture slice type");
    require(slice.macroblocks.size() == 1, "CAVLC coefficient fixture macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.residualParsed, "CAVLC coefficient fixture residual parsed");
    require(mb.residualBlockCount == 17, "CAVLC coefficient fixture residual block count");
    require(mb.residualCoefficientCount == 1, "CAVLC coefficient fixture total coefficient count");
    require(mb.residualBlocks.size() == 17, "CAVLC coefficient fixture residual block summaries");

    const ResidualBlockInfo &dcBlock = mb.residualBlocks.first();
    require(dcBlock.kind == QStringLiteral("luma16x16_dc"), "CAVLC coefficient fixture DC block kind");
    require(dcBlock.totalCoefficientCount == 0, "CAVLC coefficient fixture zero DC coefficients");
    require(dcBlock.coefficients.isEmpty(), "CAVLC coefficient fixture empty DC coefficients");

    const ResidualBlockInfo &acBlock = mb.residualBlocks[1];
    require(acBlock.kind == QStringLiteral("luma16x16_ac"), "CAVLC coefficient fixture AC block kind");
    require(acBlock.blockIndex == 0, "CAVLC coefficient fixture AC block index");
    require(acBlock.totalCoefficientCount == 1, "CAVLC coefficient fixture AC coefficient count");
    require(acBlock.trailingOnes == 1, "CAVLC coefficient fixture trailing ones");
    require(acBlock.totalZeros == 0, "CAVLC coefficient fixture total zeros");
    require(acBlock.coefficients.size() == 1, "CAVLC coefficient fixture coefficient summary");
    require(acBlock.coefficients.first().scanIndex == 0, "CAVLC coefficient fixture scan index");
    require(acBlock.coefficients.first().level == 1, "CAVLC coefficient fixture coefficient level");
    require(acBlock.coefficients.first().runBefore == 0, "CAVLC coefficient fixture run before");

    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);
    const FrameSyntaxInfo syntax = h264SyntaxFromFrameAnalysis(analysis);
    require(syntax.slices.first().macroblocks.first().residualBlocks[1].coefficients.first().level == 1,
            "CAVLC coefficient fixture survives FrameAnalysis details");
}

void testBSliceReportsMotionVectorDiagnostic()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeBSliceNalu()
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "B-slice fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("B"), "B-slice fixture slice type");
    require(hasDiagnosticCode(slice, QStringLiteral("b_direct_macroblock_unsupported")),
            "B direct fixture reports structured unsupported diagnostic");
    require(!slice.macroblocks.isEmpty(), "B-slice fixture keeps estimated macroblock data");
    require(slice.macroblocks.first().mbType == QStringLiteral("B_Direct_16x16"), "B-slice fixture macroblock type");

    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);
    require(hasAnalysisDiagnosticCode(analysis, QStringLiteral("b_direct_macroblock_unsupported")),
            "B-slice diagnostic is exposed in FrameAnalysis");
}

void testB8x8UnsupportedReportsDiagnosticWithoutMotionVectors()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeB8x8UnsupportedSliceNalu()
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "B_8x8 fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("B"), "B_8x8 fixture slice type");
    require(hasDiagnosticCode(slice, QStringLiteral("b8x8_sub_macroblock_unsupported")),
            "B_8x8 fixture reports structured unsupported diagnostic");
    require(!slice.macroblocks.isEmpty(), "B_8x8 fixture keeps macroblock data");
    require(slice.macroblocks.first().mbType == QStringLiteral("B_8x8"), "B_8x8 fixture macroblock type");
    require(!slice.macroblocks.first().parsed, "B_8x8 fixture macroblock is not marked parsed");
    require(slice.macroblocks.first().motionVectors.isEmpty(), "B_8x8 fixture does not invent motion vectors");

    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);
    require(hasAnalysisDiagnosticCode(analysis, QStringLiteral("b8x8_sub_macroblock_unsupported")),
            "B_8x8 diagnostic is exposed in FrameAnalysis");
    require(analysis.motionVectors.isEmpty(), "B_8x8 FrameAnalysis has no motion vectors");

    const AnalysisStats stats = calculateAnalysisStats({analysis});
    require(stats.motionVectorCount == 0, "B_8x8 stats has no motion vectors");
    require(stats.diagnosticAccessUnits == 1, "B_8x8 stats diagnostic access unit count");
    require(stats.diagnosticCount == 1, "B_8x8 stats diagnostic count");
}

void testCavlcBSliceBiMotionVectorFixture()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeBMotionSliceNalu()
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC B Bi fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("B"), "CAVLC B Bi fixture slice type");
    require(slice.macroblocks.size() == 1, "CAVLC B Bi fixture macroblock count");
    require(slice.macroblocks.first().mbType == QStringLiteral("B_Bi_16x16"), "CAVLC B Bi fixture mb_type");
    require(slice.macroblocks.first().parsed, "CAVLC B Bi fixture macroblock parsed");
    require(slice.macroblocks.first().motionVectors.size() == 2, "CAVLC B Bi fixture motion vector count");
    require(!hasDiagnosticCode(slice, QStringLiteral("b_slice_macroblock_unsupported")),
            "CAVLC B Bi fixture has no generic unsupported diagnostic");

    const QVector<MotionVectorInfo> &mvs = slice.macroblocks.first().motionVectors;
    require(mvs[0].list == 0 && mvs[0].mvXQuarterPel == 2 && mvs[0].mvYQuarterPel == -1,
            "CAVLC B Bi fixture L0 mv");
    require(mvs[1].list == 1 && mvs[1].mvXQuarterPel == -2 && mvs[1].mvYQuarterPel == 1,
            "CAVLC B Bi fixture L1 mv");

    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);
    require(analysis.motionVectors.size() == 2, "CAVLC B Bi FrameAnalysis motion vectors");
    require(analysis.motionVectors[0].list == 0, "CAVLC B Bi FrameAnalysis L0 vector");
    require(analysis.motionVectors[1].list == 1, "CAVLC B Bi FrameAnalysis L1 vector");
}

void testCavlcBSlicePartitionedMotionVectorFixture()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeBPartitionedMotionSliceNalu()
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC B partitioned fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("B"), "CAVLC B partitioned fixture slice type");
    require(slice.macroblocks.size() == 1, "CAVLC B partitioned fixture macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.mbType == QStringLiteral("B_L0_L1_16x8"), "CAVLC B partitioned fixture mb_type");
    require(mb.parsed, "CAVLC B partitioned fixture macroblock parsed");
    require(mb.motionVectors.size() == 2, "CAVLC B partitioned fixture motion vector count");
    require(!hasDiagnosticCode(slice, QStringLiteral("b_slice_macroblock_unsupported")),
            "CAVLC B partitioned fixture has no generic unsupported diagnostic");

    const QVector<MotionVectorInfo> &mvs = mb.motionVectors;
    require(mvs[0].list == 0 && mvs[0].mvXQuarterPel == 2 && mvs[0].mvYQuarterPel == -1,
            "CAVLC B partitioned fixture L0 partition mv");
    require(mvs[1].list == 1 && mvs[1].mvXQuarterPel == -3 && mvs[1].mvYQuarterPel == 1,
            "CAVLC B partitioned fixture L1 partition mv");

    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);
    require(analysis.motionVectors.size() == 2, "CAVLC B partitioned FrameAnalysis motion vectors");
    require(analysis.motionVectors[0].list == 0, "CAVLC B partitioned FrameAnalysis L0 vector");
    require(analysis.motionVectors[1].list == 1, "CAVLC B partitioned FrameAnalysis L1 vector");
    require(analysis.motionVectors[1].mvXQuarterPel == -3, "CAVLC B partitioned FrameAnalysis L1 mv_x");
    require(analysis.motionVectors[1].mvYQuarterPel == 1, "CAVLC B partitioned FrameAnalysis L1 mv_y");

    const AnalysisStats stats = calculateAnalysisStats({analysis});
    require(stats.motionVectorCount == 2, "CAVLC B partitioned stats motion vector count");
    require(stats.l0MotionVectorCount == 1, "CAVLC B partitioned stats L0 vector count");
    require(stats.l1MotionVectorCount == 1, "CAVLC B partitioned stats L1 vector count");
    require(stats.otherMotionVectorCount == 0, "CAVLC B partitioned stats other vector count");
}

void testCavlcBSlice8x16PartitionedMotionVectorFixture()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeB8x16PartitionedMotionSliceNalu()
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC B 8x16 partitioned fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("B"), "CAVLC B 8x16 partitioned fixture slice type");
    require(slice.macroblocks.size() == 1, "CAVLC B 8x16 partitioned fixture macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.mbType == QStringLiteral("B_L1_L0_8x16"), "CAVLC B 8x16 partitioned fixture mb_type");
    require(mb.parsed, "CAVLC B 8x16 partitioned fixture macroblock parsed");
    require(mb.motionVectors.size() == 2, "CAVLC B 8x16 partitioned fixture motion vector count");

    const QVector<MotionVectorInfo> &mvs = mb.motionVectors;
    require(mvs[0].list == 0 && mvs[0].mvXQuarterPel == 4 && mvs[0].mvYQuarterPel == -2,
            "CAVLC B 8x16 partitioned fixture L0 partition 1 mv");
    require(mvs[1].list == 1 && mvs[1].mvXQuarterPel == -1 && mvs[1].mvYQuarterPel == 3,
            "CAVLC B 8x16 partitioned fixture L1 partition 0 mv");

    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);
    const AnalysisStats stats = calculateAnalysisStats({analysis});
    require(stats.motionVectorCount == 2, "CAVLC B 8x16 partitioned stats MV count");
    require(stats.l0MotionVectorCount == 1, "CAVLC B 8x16 partitioned stats L0 count");
    require(stats.l1MotionVectorCount == 1, "CAVLC B 8x16 partitioned stats L1 count");
}

void testCavlcBSliceBiBiPartitionedMotionVectorFixture()
{
    H264Parser parser;
    const QByteArray packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeBBiBiPartitionedMotionSliceNalu()
    });
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC B Bi/Bi partitioned fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("B"), "CAVLC B Bi/Bi partitioned fixture slice type");
    require(slice.macroblocks.size() == 1, "CAVLC B Bi/Bi partitioned fixture macroblock count");
    const MacroblockInfo &mb = slice.macroblocks.first();
    require(mb.mbType == QStringLiteral("B_Bi_Bi_16x8"), "CAVLC B Bi/Bi partitioned fixture mb_type");
    require(mb.parsed, "CAVLC B Bi/Bi partitioned fixture macroblock parsed");
    require(mb.motionVectors.size() == 4, "CAVLC B Bi/Bi partitioned fixture motion vector count");

    const QVector<MotionVectorInfo> &mvs = mb.motionVectors;
    require(mvs[0].list == 0 && mvs[0].mvXQuarterPel == 1 && mvs[0].mvYQuarterPel == 0,
            "CAVLC B Bi/Bi partitioned fixture L0 partition 0 mv");
    require(mvs[1].list == 0 && mvs[1].mvXQuarterPel == -1 && mvs[1].mvYQuarterPel == 2,
            "CAVLC B Bi/Bi partitioned fixture L0 partition 1 mv");
    require(mvs[2].list == 1 && mvs[2].mvXQuarterPel == 0 && mvs[2].mvYQuarterPel == -2,
            "CAVLC B Bi/Bi partitioned fixture L1 partition 0 mv");
    require(mvs[3].list == 1 && mvs[3].mvXQuarterPel == 3 && mvs[3].mvYQuarterPel == 1,
            "CAVLC B Bi/Bi partitioned fixture L1 partition 1 mv");

    const FrameAnalysis analysis = parser.parsePacket(packet, 0, 0, 0);
    const AnalysisStats stats = calculateAnalysisStats({analysis});
    require(stats.motionVectorCount == 4, "CAVLC B Bi/Bi partitioned stats MV count");
    require(stats.l0MotionVectorCount == 2, "CAVLC B Bi/Bi partitioned stats L0 count");
    require(stats.l1MotionVectorCount == 2, "CAVLC B Bi/Bi partitioned stats L1 count");
}

void testH264AggregateStatsAndExportRegression()
{
    H264Parser parser;
    const QByteArray pMvPacket = loadFixture(QStringLiteral("cavlc_p_motion_vector.hex"));
    const QByteArray bPartitionedPacket = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeBPartitionedMotionSliceNalu()
    });
    const QByteArray b8x8Packet = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeB8x8UnsupportedSliceNalu()
    });
    const QByteArray residualPacket = makeAnnexBPacket({
        makeMinimalSpsNalu(),
        makeMinimalPpsNalu(),
        makeI16x16NonZeroResidualSliceNalu()
    });

    const FrameAnalysis pMv = parser.parsePacket(pMvPacket, 0, 0, 0);
    const FrameAnalysis bPartitioned = parser.parsePacket(bPartitionedPacket, 1, 1, 1);
    const FrameAnalysis b8x8 = parser.parsePacket(b8x8Packet, 2, 2, 2);
    const FrameAnalysis residual = parser.parsePacket(residualPacket, 3, 3, 3);
    const QVector<FrameAnalysis> analyses {pMv, bPartitioned, b8x8, residual};

    const AnalysisStats stats = calculateAnalysisStats(analyses);
    require(stats.totalAccessUnits == 4, "H264 aggregate stats access unit count");
    require(stats.frameCount == 4, "H264 aggregate stats frame count");
    require(stats.motionVectorCount == 3, "H264 aggregate stats MV count");
    require(stats.l0MotionVectorCount == 2, "H264 aggregate stats L0 MV count");
    require(stats.l1MotionVectorCount == 1, "H264 aggregate stats L1 MV count");
    require(stats.otherMotionVectorCount == 0, "H264 aggregate stats other MV count");
    require(stats.diagnosticAccessUnits == 1, "H264 aggregate stats diagnostic access units");
    require(stats.diagnosticCount == 1, "H264 aggregate stats diagnostic count");

    StreamInfo stream;
    stream.fileName = QStringLiteral("aggregate.264");
    stream.absoluteFilePath = QStringLiteral("D:/fixtures/aggregate.264");
    stream.mediaKind = MediaKind::Video;
    stream.streamIndex = 0;
    stream.codecKind = CodecKind::H264;
    stream.codecName = QStringLiteral("h264");
    stream.width = 16;
    stream.height = 16;
    stream.isValid = true;

    const QJsonObject batch = allFramesExportToJson(stream,
                                                   analyses,
                                                   QStringLiteral("test-generator"),
                                                   QStringLiteral("1.0"));
    const QJsonArray frames = batch.value(QStringLiteral("frames")).toArray();
    require(frames.size() == 4, "H264 aggregate export frame count");
    for (const QJsonValue &frameValue : frames) {
        require(frameValue.toObject().contains(QStringLiteral("h264")),
                "H264 aggregate export keeps h264 details");
    }

    const QJsonArray bMotionVectors = frames[1].toObject().value(QStringLiteral("motion_vectors")).toArray();
    require(bMotionVectors.size() == 2, "H264 aggregate export B partitioned MV count");
    require(bMotionVectors[0].toObject().value(QStringLiteral("list")).toInt() == 0,
            "H264 aggregate export B partitioned L0 MV");
    require(bMotionVectors[1].toObject().value(QStringLiteral("list")).toInt() == 1,
            "H264 aggregate export B partitioned L1 MV");

    const QJsonArray unsupportedDiagnostics = frames[2].toObject().value(QStringLiteral("diagnostics")).toArray();
    require(unsupportedDiagnostics.size() == 1, "H264 aggregate export unsupported diagnostic count");
    require(unsupportedDiagnostics.first().toObject().value(QStringLiteral("code")).toString()
                == QStringLiteral("b8x8_sub_macroblock_unsupported"),
            "H264 aggregate export unsupported diagnostic code");
    require(frames[2].toObject().value(QStringLiteral("motion_vectors")).toArray().isEmpty(),
            "H264 aggregate export unsupported frame has no MVs");

    const QJsonArray residualSlices = frames[3].toObject().value(QStringLiteral("h264")).toObject()
                                          .value(QStringLiteral("slices")).toArray();
    const QJsonArray residualMacroblocks = residualSlices.first().toObject()
                                               .value(QStringLiteral("macroblocks")).toArray();
    const QJsonArray residualBlocks = residualMacroblocks.first().toObject()
                                           .value(QStringLiteral("residual_blocks")).toArray();
    const QJsonArray coefficients = residualBlocks[1].toObject()
                                        .value(QStringLiteral("coefficients")).toArray();
    require(coefficients.size() == 1, "H264 aggregate export residual coefficient count");
    require(coefficients.first().toObject().value(QStringLiteral("level")).toInt() == 1,
            "H264 aggregate export residual coefficient level");
    require(coefficients.first().toObject().value(QStringLiteral("scan_index")).toInt() == 0,
            "H264 aggregate export residual coefficient scan index");
}

void testFrameAnalysisMirrorsH264SyntaxFixture()
{
    H264Parser parser;
    const FrameAnalysis analysis = parser.parsePacket(loadFixture(QStringLiteral("cavlc_p_motion_vector.hex")), 11, 7, 3);
    require(analysis.codecKind == CodecKind::H264, "FrameAnalysis codec kind");
    require(analysis.frameIndex == 3, "FrameAnalysis packet index");
    require(analysis.pts == 11, "FrameAnalysis pts");
    require(analysis.dts == 7, "FrameAnalysis dts");
    require(analysis.frameType == QStringLiteral("P"), "FrameAnalysis frame type");
    require(analysis.hasFrame, "FrameAnalysis has frame");
    require(!analysis.units.isEmpty(), "FrameAnalysis units");
    require(!analysis.regions.isEmpty(), "FrameAnalysis regions");
    require(!analysis.motionVectors.isEmpty(), "FrameAnalysis motion vectors");
    require(analysis.regions.first().kind == AnalysisRegionKind::Macroblock, "FrameAnalysis macroblock region kind");
    require(analysis.regions.first().qp == 26, "FrameAnalysis macroblock QP");
    require(analysis.motionVectors.first().mvXQuarterPel == 2, "FrameAnalysis mv_x");
    require(analysis.motionVectors.first().mvYQuarterPel == -1, "FrameAnalysis mv_y");

    const FrameSyntaxInfo syntax = h264SyntaxFromFrameAnalysis(analysis);
    require(syntax.slices.size() == 1, "FrameAnalysis keeps H264 details");
    require(syntax.slices.first().macroblocks.size() == 1, "FrameAnalysis keeps H264 macroblocks");
}

void testH264ResidualBlocksExportFixture()
{
    H264Parser parser;
    const FrameAnalysis analysis = parser.parsePacket(loadFixture(QStringLiteral("cavlc_i_qp_delta.hex")), 0, 0, 0);

    StreamInfo stream;
    stream.fileName = QStringLiteral("fixture.264");
    stream.absoluteFilePath = QStringLiteral("D:/fixtures/fixture.264");
    stream.mediaKind = MediaKind::Video;
    stream.streamIndex = 0;
    stream.codecKind = CodecKind::H264;
    stream.codecName = QStringLiteral("h264");
    stream.isValid = true;

    const QJsonObject selected = selectedFrameExportToJson(stream,
                                                          analysis,
                                                          QStringLiteral("test-generator"),
                                                          QStringLiteral("1.0"));
    const QJsonArray slices = selected.value(QStringLiteral("frame")).toObject()
                                  .value(QStringLiteral("slices")).toArray();
    const QJsonArray macroblocks = slices.first().toObject()
                                      .value(QStringLiteral("macroblocks")).toArray();
    const QJsonObject firstMacroblock = macroblocks.first().toObject();
    const QJsonArray residualBlocks = firstMacroblock.value(QStringLiteral("residual_blocks")).toArray();
    require(residualBlocks.size() == 17, "H264 export residual block summary count");
    require(residualBlocks.first().toObject().value(QStringLiteral("kind")).toString() == QStringLiteral("luma16x16_dc"),
            "H264 export residual block kind");
    require(residualBlocks.first().toObject().value(QStringLiteral("max_coefficient_count")).toInt() == 16,
            "H264 export residual block max coeffs");
    require(residualBlocks.first().toObject().value(QStringLiteral("total_coefficient_count")).toInt() == 0,
            "H264 export residual block coeff count");
    require(residualBlocks.first().toObject().contains(QStringLiteral("coefficients")),
            "H264 export residual block has coefficient array");
}

void testFrameAnalysisExportSchemaFixture()
{
    H264Parser parser;
    FrameAnalysis analysis = parser.parsePacket(loadFixture(QStringLiteral("cavlc_p_motion_vector.hex")), 11, 7, 3);
    analysis.packet.streamPacketIndex = 3;
    analysis.packet.containerPacketIndex = 9;
    analysis.packet.streamIndex = 0;
    analysis.packet.mediaKind = MediaKind::Video;
    analysis.packet.codecKind = CodecKind::H264;
    analysis.packet.pts = 11;
    analysis.packet.dts = 7;
    analysis.packet.duration = 2;
    analysis.packet.position = 1234;
    analysis.packet.size = 42;
    analysis.packet.keyframe = true;
    analysis.packet.bytes = QByteArray::fromHex("00000165");

    StreamInfo stream;
    stream.fileName = QStringLiteral("fixture.264");
    stream.absoluteFilePath = QStringLiteral("D:/fixtures/fixture.264");
    stream.mediaKind = MediaKind::Video;
    stream.streamIndex = 0;
    stream.codecName = QStringLiteral("h264");
    stream.pixelFormatName = QStringLiteral("yuv420p");
    stream.width = 16;
    stream.height = 16;
    stream.frameRate = 30.0;
    stream.streams.append({
        0,
        MediaKind::Video,
        CodecKind::H264,
        QStringLiteral("h264"),
        QStringLiteral("yuv420p"),
        QString {},
        QString {},
        0,
        0,
        16,
        16,
        30.0,
        0,
        0,
        true
    });
    stream.streams.append({
        1,
        MediaKind::Audio,
        CodecKind::AAC,
        QStringLiteral("aac"),
        QString {},
        QStringLiteral("fltp"),
        QStringLiteral("stereo"),
        0,
        0,
        0,
        0,
        0.0,
        48000,
        2,
        false
    });
    stream.isValid = true;

    const QJsonObject selected = selectedFrameExportToJson(stream,
                                                          analysis,
                                                          QStringLiteral("test-generator"),
                                                          QStringLiteral("1.0"));
    require(selected.value(QStringLiteral("schema_version")).toInt() == 3, "selected export schema version");
    require(selected.value(QStringLiteral("generator")).toString() == QStringLiteral("test-generator"),
            "selected export generator");
    const QJsonArray streams = selected.value(QStringLiteral("stream")).toObject().value(QStringLiteral("streams")).toArray();
    require(streams.size() == 2, "selected export stream discovery list");
    require(streams[1].toObject().value(QStringLiteral("media_kind")).toString() == QStringLiteral("audio"),
            "selected export audio stream media kind");
    require(streams[1].toObject().value(QStringLiteral("sample_rate")).toInt() == 48000,
            "selected export audio stream sample rate");
    require(selected.contains(QStringLiteral("frame_analysis")), "selected export has frame_analysis");
    require(selected.contains(QStringLiteral("frame")), "selected export keeps legacy frame details");

    const QJsonObject frameAnalysis = selected.value(QStringLiteral("frame_analysis")).toObject();
    require(frameAnalysis.value(QStringLiteral("media_kind")).toString() == QStringLiteral("video"),
            "frame_analysis media kind");
    require(frameAnalysis.value(QStringLiteral("access_unit_kind")).toString() == QStringLiteral("video_frame"),
            "frame_analysis access unit kind");
    require(frameAnalysis.value(QStringLiteral("has_frame")).toBool(), "frame_analysis has_frame");
    require(!frameAnalysis.value(QStringLiteral("units")).toArray().isEmpty(), "frame_analysis units");
    require(!frameAnalysis.value(QStringLiteral("regions")).toArray().isEmpty(), "frame_analysis regions");
    require(!frameAnalysis.value(QStringLiteral("motion_vectors")).toArray().isEmpty(),
            "frame_analysis motion vectors");
    const QJsonArray bitFields = frameAnalysis.value(QStringLiteral("bit_fields")).toArray();
    require(!bitFields.isEmpty(), "frame_analysis bit fields");
    require(bitFields.first().toObject().value(QStringLiteral("offset_basis")).toString() == QStringLiteral("rbsp"),
            "h264 bit fields export rbsp offset basis");
    const QJsonArray packetBitRanges = bitFields.first().toObject().value(QStringLiteral("packet_bit_ranges")).toArray();
    require(!packetBitRanges.isEmpty(), "h264 bit fields export packet bit ranges");
    require(packetBitRanges.first().toObject().value(QStringLiteral("offset_basis")).toString() == QStringLiteral("packet"),
            "h264 packet bit range basis");
    bool hasMacroblockField = false;
    for (const QJsonValue &fieldValue : bitFields) {
        const QJsonObject field = fieldValue.toObject();
        if (field.value(QStringLiteral("path")).toString().contains(QStringLiteral("macroblocks"))
            && field.value(QStringLiteral("name")).toString() == QStringLiteral("mb_type")
            && !field.value(QStringLiteral("packet_bit_ranges")).toArray().isEmpty()) {
            hasMacroblockField = true;
            break;
        }
    }
    require(hasMacroblockField, "h264 macroblock bit fields export packet ranges");
    const QJsonObject packet = frameAnalysis.value(QStringLiteral("packet")).toObject();
    require(packet.value(QStringLiteral("packet_index")).toInt() == 3, "frame_analysis packet index");
    require(packet.value(QStringLiteral("stream_packet_index")).toInt() == 3, "frame_analysis stream packet index");
    require(packet.value(QStringLiteral("container_packet_index")).toInt() == 9, "frame_analysis container packet index");
    require(packet.value(QStringLiteral("pos")).toInt() == 1234, "frame_analysis packet pos");
    require(packet.value(QStringLiteral("size")).toInt() == 42, "frame_analysis packet size");
    require(packet.value(QStringLiteral("keyframe")).toBool(), "frame_analysis packet keyframe");
    require(packet.value(QStringLiteral("raw_bytes_size")).toInt() == 4, "frame_analysis raw byte size");

    const QJsonObject legacyFrame = selected.value(QStringLiteral("frame")).toObject();
    require(!legacyFrame.value(QStringLiteral("slices")).toArray().isEmpty(), "legacy frame slices");

    const QJsonObject batch = allFramesExportToJson(stream,
                                                   QVector<FrameAnalysis> {FrameAnalysis {}, analysis},
                                                   QStringLiteral("test-generator"),
                                                   QStringLiteral("1.0"));
    const QJsonArray frames = batch.value(QStringLiteral("frames")).toArray();
    require(frames.size() == 1, "batch export skips invalid frame entries");
    require(frames.first().toObject().contains(QStringLiteral("h264")), "batch export keeps h264 details");
}

QByteArray makeAacLc44100StereoHeaderOnlyFrame()
{
    QByteArray packet;
    packet.append(char(0xff));
    packet.append(char(0xf1));
    packet.append(char(0x50));
    packet.append(char(0x80));
    packet.append(char(0x00));
    packet.append(char(0xff));
    packet.append(char(0xfc));
    return packet;
}

void testAudioFrameAnalysisExportSchemaFixture()
{
    AacAdtsParser parser;
    FrameAnalysis analysis = parser.parsePacket(makeAacLc44100StereoHeaderOnlyFrame(), 21, 19, 2);
    analysis.streamIndex = 1;

    StreamInfo stream;
    stream.fileName = QStringLiteral("fixture-audio.mp4");
    stream.absoluteFilePath = QStringLiteral("D:/fixtures/fixture-audio.mp4");
    stream.mediaKind = MediaKind::Video;
    stream.streamIndex = 0;
    stream.codecName = QStringLiteral("h264");
    stream.streams.append({
        1,
        MediaKind::Audio,
        CodecKind::AAC,
        QStringLiteral("aac"),
        QString {},
        QStringLiteral("fltp"),
        QStringLiteral("stereo"),
        0,
        0,
        0,
        0,
        0.0,
        44100,
        2,
        false
    });
    stream.isValid = true;

    const QJsonObject selected = selectedFrameExportToJson(stream,
                                                          analysis,
                                                          QStringLiteral("test-generator"),
                                                          QStringLiteral("1.0"));
    require(selected.value(QStringLiteral("schema_version")).toInt() == 3, "audio selected export schema version");
    require(selected.contains(QStringLiteral("frame_analysis")), "audio selected export has frame_analysis");
    require(!selected.contains(QStringLiteral("frame")), "audio selected export does not invent legacy h264 frame");

    const QJsonObject frameAnalysis = selected.value(QStringLiteral("frame_analysis")).toObject();
    require(frameAnalysis.value(QStringLiteral("media_kind")).toString() == QStringLiteral("audio"),
            "audio frame_analysis media kind");
    require(frameAnalysis.value(QStringLiteral("access_unit_kind")).toString() == QStringLiteral("audio_frame"),
            "audio frame_analysis access unit kind");
    require(frameAnalysis.value(QStringLiteral("codec")).toString() == QStringLiteral("AAC"),
            "audio frame_analysis codec");
    require(frameAnalysis.value(QStringLiteral("units")).toArray().first().toObject().value(QStringLiteral("kind")).toString()
                == QStringLiteral("adts_frame"),
            "audio frame_analysis adts unit");
    require(!frameAnalysis.value(QStringLiteral("bit_fields")).toArray().isEmpty(),
            "audio frame_analysis bit fields");

    const QJsonObject batch = allFramesExportToJson(stream,
                                                   QVector<FrameAnalysis> {analysis},
                                                   QStringLiteral("test-generator"),
                                                   QStringLiteral("1.0"));
    const QJsonObject batchFrame = batch.value(QStringLiteral("frames")).toArray().first().toObject();
    require(batchFrame.value(QStringLiteral("media_kind")).toString() == QStringLiteral("audio"),
            "audio batch media kind");
    require(!batchFrame.contains(QStringLiteral("h264")), "audio batch does not include h264 block");
}

void testCavlcPResidualContinuesToMotionVectorFixture()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("cavlc_p_residual_then_motion_vector.hex")), 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CAVLC P residual fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "CAVLC P residual fixture slice type");
    require(slice.macroblocks.size() == 2, "CAVLC P residual fixture macroblock count");
    require(slice.macroblocks.first().mbQpDelta == 2, "CAVLC P residual fixture mb_qp_delta");
    require(slice.macroblocks.first().qp == 28, "CAVLC P residual fixture first macroblock QP");
    require(slice.macroblocks.first().residualParsed, "CAVLC P residual fixture residual parsed");
    require(slice.macroblocks.first().residualBlockCount == 4, "CAVLC P residual fixture residual block count");
    require(slice.macroblocks.first().residualBlocks.size() == 4,
            "CAVLC P residual fixture residual block summaries");
    for (const ResidualBlockInfo &block : slice.macroblocks.first().residualBlocks) {
        require(block.kind == QStringLiteral("luma4x4"), "CAVLC P residual fixture luma4x4 block kind");
        require(block.maxCoefficientCount == 16, "CAVLC P residual fixture max coeffs");
    }
    require(slice.macroblocks[1].parsed, "CAVLC P residual fixture second macroblock parsed");
    require(!slice.macroblocks[1].motionVectors.isEmpty(), "CAVLC P residual fixture later motion vector");

    const MotionVectorInfo &mv = slice.macroblocks[1].motionVectors.first();
    require(mv.mvXQuarterPel == 2, "CAVLC P residual fixture mv_x");
    require(mv.mvYQuarterPel == -1, "CAVLC P residual fixture mv_y");
}

void testUnsupportedCabacFixtureReportsDiagnostic()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("unsupported_cabac_p.hex")), 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "CABAC fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "CABAC fixture slice type");
    require(slice.cabacInitIdc == 0, "CABAC fixture cabac_init_idc");
    require(slice.macroblocks.size() == 1, "CABAC fixture estimated macroblock count");
    require(!slice.macroblocks.first().parsed, "CABAC fixture macroblock remains estimated");
    require(hasDiagnosticCode(slice, QStringLiteral("cabac_alignment_failed")),
            "synthetic CABAC fixture reports invalid alignment");
    require(hasDiagnosticCode(slice, QStringLiteral("cabac_unsupported")), "CABAC fixture diagnostic");
    require(!slice.macroblockParseWarnings.isEmpty(), "CABAC fixture warning text");
}

void testRealX264CabacResidualFixture()
{
    const QByteArray packet = loadBinaryFixture(QStringLiteral("x264_cabac_residual.h264"));
    require(packet.size() == 374, "x264 CABAC residual fixture byte size");

    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(packet, 0, 0, 0);
    require(frame.slices.size() == 2, "x264 CABAC residual fixture slice count");

    const SliceInfo *pSlice = nullptr;
    for (const SliceInfo &slice : frame.slices) {
        if (slice.sliceTypeName == QStringLiteral("P")) {
            pSlice = &slice;
            break;
        }
    }
    require(pSlice != nullptr, "x264 CABAC residual fixture P slice");
    require(pSlice->cabacInitIdc == 0, "x264 CABAC residual fixture cabac_init_idc");
    require(pSlice->macroblocks.size() == 4, "x264 CABAC residual fixture macroblock count");
    const MacroblockInfo &firstMacroblock = pSlice->macroblocks[0];
    const MacroblockInfo &secondMacroblock = pSlice->macroblocks[1];
    require(firstMacroblock.parsed && secondMacroblock.parsed,
            "x264 CABAC residual fixture first two macroblocks parsed");
    require(firstMacroblock.mbType == QStringLiteral("P_L0_16x16")
                && secondMacroblock.mbType == QStringLiteral("P_L0_16x16"),
            "x264 CABAC residual fixture P_16x16 macroblock types");
    require(firstMacroblock.motionVectors.size() == 1
                && firstMacroblock.motionVectors[0].mvXQuarterPel == 0
                && firstMacroblock.motionVectors[0].mvYQuarterPel == -4,
            "x264 CABAC residual fixture first motion vector");
    require(firstMacroblock.codedBlockPatternLuma == 15
                && firstMacroblock.residualCoefficientCount == 38,
            "x264 CABAC residual fixture first luma residual summary");
    require(secondMacroblock.codedBlockPatternLuma == 12
                && secondMacroblock.codedBlockPatternChroma == 0
                && secondMacroblock.residualCoefficientCount == 21,
            "x264 CABAC residual fixture second luma residual summary");
    require(firstMacroblock.residualBlocks[0].blockIndex == 0
                && firstMacroblock.residualBlocks[0].coefficients[0].scanIndex == 15
                && firstMacroblock.residualBlocks[0].coefficients[0].level == -1,
            "x264 CABAC residual fixture stable first coefficient");
    require(hasDiagnosticCode(*pSlice, QStringLiteral("cabac_mb_qp_delta_incomplete")),
            "x264 CABAC residual fixture reaches non-zero mb_qp_delta boundary");
    require(!hasDiagnosticCode(*pSlice, QStringLiteral("cabac_mvd_incomplete")),
            "x264 CABAC residual fixture completes MVD syntax");
    require(!hasDiagnosticCode(*pSlice, QStringLiteral("cabac_alignment_failed")),
            "x264 CABAC residual fixture alignment parsed");
    require(!hasDiagnosticCode(*pSlice, QStringLiteral("cabac_mb_type_incomplete")),
            "x264 CABAC residual fixture P mb_type parsed");
}

void testTruncatedPSliceDataReportsDiagnostic()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("truncated_p_slice_data.hex")), 0, 0, 0);
    const SliceInfo &slice = firstSlice(frame, "truncated P fixture has slice");
    require(slice.sliceTypeName == QStringLiteral("P"), "truncated P fixture slice type");
    require(hasDiagnosticCode(slice, QStringLiteral("slice_data_truncated")),
            "truncated P fixture reports structured diagnostic");
    require(!slice.macroblocks.isEmpty(), "truncated P fixture keeps estimated macroblock data");
    require(!slice.macroblocks.first().parsed, "truncated P fixture macroblock is not marked parsed");
    require(!slice.macroblockParseWarnings.isEmpty(), "truncated P fixture warning text");
}

void testTruncatedSliceHeaderReportsDiagnostic()
{
    H264Parser parser;
    const FrameSyntaxInfo frame = parser.parsePacketSyntax(loadFixture(QStringLiteral("truncated_slice_header.hex")), 0, 0, 0);
    require(frame.nalus.size() == 3, "truncated header fixture NALU count");
    const SliceInfo &slice = firstSlice(frame, "truncated header fixture has diagnostic slice");
    require(!slice.valid, "truncated header fixture slice is invalid");
    require(hasDiagnosticCode(slice, QStringLiteral("slice_header_truncated")),
            "truncated header fixture reports structured diagnostic");
    require(slice.macroblocks.isEmpty(), "truncated header fixture has no macroblock data");
    require(!slice.macroblockParseWarnings.isEmpty(), "truncated header fixture warning text");
}
}

int main()
{
    testExpGolomb();
    testAnnexBSps();
    testAvccSps();
    testAnnexBFixtureWithIdr();
    testAvccFixtureWithSpsPps();
    testAvccLengthExceedsPacketReportsDiagnostic();
    testTruncatedSpsReportsDiagnostic();
    testTruncatedPpsReportsDiagnostic();
    testCavlcIFrameQpDeltaFixture();
    testCavlcPSkipFixture();
    testCavlcPMotionVectorFixture();
    testCavlcPMultiMacroblockMotionVectorPredictionFixture();
    testCavlcP8x8MotionVectorFixture();
    testCavlcP8x8Ref0MotionVectorFixture();
    testCavlcResidualCoefficientFixture();
    testBSliceReportsMotionVectorDiagnostic();
    testB8x8UnsupportedReportsDiagnosticWithoutMotionVectors();
    testCavlcBSliceBiMotionVectorFixture();
    testCavlcBSlicePartitionedMotionVectorFixture();
    testCavlcBSlice8x16PartitionedMotionVectorFixture();
    testCavlcBSliceBiBiPartitionedMotionVectorFixture();
    testH264AggregateStatsAndExportRegression();
    testFrameAnalysisMirrorsH264SyntaxFixture();
    testH264ResidualBlocksExportFixture();
    testFrameAnalysisExportSchemaFixture();
    testAudioFrameAnalysisExportSchemaFixture();
    testCavlcPResidualContinuesToMotionVectorFixture();
    testRealX264CabacResidualFixture();
    testUnsupportedCabacFixtureReportsDiagnostic();
    testTruncatedPSliceDataReportsDiagnostic();
    testTruncatedSliceHeaderReportsDiagnostic();
    std::cout << "H264Parser tests passed\n";
    return 0;
}
