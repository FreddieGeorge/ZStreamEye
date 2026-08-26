#include "core/parser/video/h264/H264Parser.h"

#include <algorithm>

SliceInfo H264Parser::parseSliceHeader(const QByteArray &rbsp, int nalUnitType, int nalRefIdc) const
{
    BitReader reader(rbsp);
    SliceInfo slice;
    auto addField = [&slice](const QString &name, qsizetype start, qsizetype end, const QString &value) {
        slice.fields.append({name, start, end - start, value});
    };
    auto boolText = [](bool value) {
        return value ? QStringLiteral("1") : QStringLiteral("0");
    };
    auto readBitField = [&reader, &addField, &boolText](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const bool value = reader.readBit();
        addField(name, start, reader.bitOffset(), boolText(value));
        return value;
    };
    auto readBitsField = [&reader, &addField](const QString &name, int count) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readBits(count);
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };
    auto readUEField = [&reader, &addField](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const quint32 value = reader.readUE();
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };
    auto readSEField = [&reader, &addField](const QString &name) {
        const qsizetype start = reader.bitOffset();
        const qint32 value = reader.readSE();
        addField(name, start, reader.bitOffset(), QString::number(value));
        return value;
    };
    auto appendDiagnostic = [&slice](const QString &code, const QString &message) {
        slice.diagnostics.append({code, message});
        slice.macroblockParseWarnings.append(message);
    };
    auto appendHeaderTruncated = [&appendDiagnostic]() {
        appendDiagnostic(
            QStringLiteral("slice_header_truncated"),
            QStringLiteral("slice header ended unexpectedly; slice_data was not parsed."));
    };

    slice.nalUnitType = nalUnitType;
    slice.nalRefIdc = nalRefIdc;
    slice.firstMbInSlice = static_cast<int>(readUEField(QStringLiteral("first_mb_in_slice")));
    slice.sliceType = static_cast<int>(readUEField(QStringLiteral("slice_type")));
    slice.sliceTypeName = sliceTypeName(slice.sliceType);
    slice.picParameterSetId = static_cast<int>(readUEField(QStringLiteral("pic_parameter_set_id")));
    if (reader.hasError()) {
        appendHeaderTruncated();
        slice.valid = false;
        return slice;
    }

    const PpsInfo pps = m_ppsById.value(slice.picParameterSetId);
    if (!pps.valid) {
        appendDiagnostic(
            QStringLiteral("pps_missing"),
            QStringLiteral("Slice references unavailable PPS id %1; remaining slice syntax was not parsed.")
                .arg(slice.picParameterSetId));
        slice.valid = false;
        return slice;
    }

    const SpsInfo sps = m_spsById.value(pps.seqParameterSetId);
    if (!sps.valid) {
        appendDiagnostic(
            QStringLiteral("sps_missing"),
            QStringLiteral("PPS id %1 references unavailable SPS id %2; remaining slice syntax was not parsed.")
                .arg(slice.picParameterSetId)
                .arg(pps.seqParameterSetId));
        slice.valid = false;
        return slice;
    }

    slice.frameNum = static_cast<int>(readBitsField(QStringLiteral("frame_num"), sps.log2MaxFrameNumMinus4 + 4));
    slice.picWidthInMbs = sps.picWidthInMbsMinus1 + 1;
    slice.picHeightInMbs = (2 - (sps.frameMbsOnlyFlag ? 1 : 0)) * (sps.picHeightInMapUnitsMinus1 + 1);
    if (!sps.frameMbsOnlyFlag) {
        slice.fieldPicFlag = readBitField(QStringLiteral("field_pic_flag"));
        if (slice.fieldPicFlag) {
            slice.bottomFieldFlag = readBitField(QStringLiteral("bottom_field_flag"));
        }
    }

    if (nalUnitType == 5) {
        slice.idrPicId = static_cast<int>(readUEField(QStringLiteral("idr_pic_id")));
    }

    if (sps.picOrderCntType == 0) {
        slice.picOrderCntLsb = static_cast<int>(readBitsField(QStringLiteral("pic_order_cnt_lsb"), sps.log2MaxPicOrderCntLsbMinus4 + 4));
        if (pps.bottomFieldPicOrderInFramePresentFlag && !slice.fieldPicFlag) {
            readSEField(QStringLiteral("delta_pic_order_cnt_bottom"));
        }
    } else if (sps.picOrderCntType == 1 && !sps.deltaPicOrderAlwaysZeroFlag) {
        readSEField(QStringLiteral("delta_pic_order_cnt[0]"));
        if (pps.bottomFieldPicOrderInFramePresentFlag && !slice.fieldPicFlag) {
            readSEField(QStringLiteral("delta_pic_order_cnt[1]"));
        }
    }

    const int normalizedSliceType = slice.sliceType % 5;
    if (pps.redundantPicCntPresentFlag) {
        readUEField(QStringLiteral("redundant_pic_cnt"));
    }

    if (normalizedSliceType == 1) {
        slice.directSpatialMvPredFlag = readBitField(QStringLiteral("direct_spatial_mv_pred_flag"));
    }

    int numRefIdxL0ActiveMinus1 = pps.numRefIdxL0DefaultActiveMinus1;
    int numRefIdxL1ActiveMinus1 = pps.numRefIdxL1DefaultActiveMinus1;
    if (normalizedSliceType == 0 || normalizedSliceType == 1 || normalizedSliceType == 3) {
        slice.numRefIdxActiveOverrideFlag = readBitField(QStringLiteral("num_ref_idx_active_override_flag"));
        if (slice.numRefIdxActiveOverrideFlag) {
            numRefIdxL0ActiveMinus1 = static_cast<int>(readUEField(QStringLiteral("num_ref_idx_l0_active_minus1")));
            if (normalizedSliceType == 1) {
                numRefIdxL1ActiveMinus1 = static_cast<int>(readUEField(QStringLiteral("num_ref_idx_l1_active_minus1")));
            }
        }
    }
    slice.numRefIdxL0ActiveMinus1 = numRefIdxL0ActiveMinus1;
    slice.numRefIdxL1ActiveMinus1 = numRefIdxL1ActiveMinus1;

    auto parseRefPicListModification = [&reader, &readBitField](const QString &flagName, bool *flag) {
        *flag = readBitField(flagName);
        if (!*flag) {
            return QStringLiteral("not present");
        }
        QStringList operations;
        while (reader.bitsRemaining() > 0) {
            const quint32 modificationOfPicNumsIdc = reader.readUE();
            if (modificationOfPicNumsIdc == 3) {
                break;
            }
            if (modificationOfPicNumsIdc == 0 || modificationOfPicNumsIdc == 1) {
                const quint32 absDiffPicNumMinus1 = reader.readUE();
                operations.append(QStringLiteral("idc=%1 abs_diff_pic_num_minus1=%2")
                                      .arg(modificationOfPicNumsIdc)
                                      .arg(absDiffPicNumMinus1));
            } else if (modificationOfPicNumsIdc == 2) {
                const quint32 longTermPicNum = reader.readUE();
                operations.append(QStringLiteral("idc=2 long_term_pic_num=%1").arg(longTermPicNum));
            } else {
                operations.append(QStringLiteral("unsupported idc=%1").arg(modificationOfPicNumsIdc));
                break;
            }
        }
        return operations.isEmpty() ? QStringLiteral("present, no operations") : operations.join(QStringLiteral("; "));
    };

    if (normalizedSliceType != 2 && normalizedSliceType != 4) {
        slice.refPicListModificationSummary =
            QStringLiteral("L0: %1").arg(parseRefPicListModification(QStringLiteral("ref_pic_list_modification_flag_l0"),
                                                                     &slice.refPicListModificationFlagL0));
    }
    if (normalizedSliceType == 1) {
        const QString l1Summary = parseRefPicListModification(QStringLiteral("ref_pic_list_modification_flag_l1"),
                                                             &slice.refPicListModificationFlagL1);
        slice.refPicListModificationSummary =
            slice.refPicListModificationSummary.isEmpty()
            ? QStringLiteral("L1: %1").arg(l1Summary)
            : QStringLiteral("%1; L1: %2").arg(slice.refPicListModificationSummary, l1Summary);
    }

    auto parsePredWeightTable = [&reader, &sps](int l0Count, int l1Count, bool hasList1) {
        const int chromaArrayType = sps.chromaFormatIdc == 0 ? 0 : sps.chromaFormatIdc;
        const quint32 lumaLog2WeightDenom = reader.readUE();
        quint32 chromaLog2WeightDenom = 0;
        if (chromaArrayType != 0) {
            chromaLog2WeightDenom = reader.readUE();
        }

        int lumaWeightFlags = 0;
        int chromaWeightFlags = 0;
        auto parseList = [&reader, chromaArrayType, &lumaWeightFlags, &chromaWeightFlags](int count) {
            for (int i = 0; i <= count; ++i) {
                if (reader.readBit()) {
                    ++lumaWeightFlags;
                    reader.readSE();
                    reader.readSE();
                }
                if (chromaArrayType != 0 && reader.readBit()) {
                    ++chromaWeightFlags;
                    for (int j = 0; j < 2; ++j) {
                        reader.readSE();
                        reader.readSE();
                    }
                }
            }
        };

        parseList(l0Count);
        if (hasList1) {
            parseList(l1Count);
        }
        return QStringLiteral("luma_denom=%1 chroma_denom=%2 luma_flags=%3 chroma_flags=%4")
            .arg(lumaLog2WeightDenom)
            .arg(chromaArrayType != 0 ? QString::number(chromaLog2WeightDenom) : QStringLiteral("-"))
            .arg(lumaWeightFlags)
            .arg(chromaWeightFlags);
    };

    const bool usePredWeightTable =
        (pps.weightedPredFlag && (normalizedSliceType == 0 || normalizedSliceType == 3))
        || (pps.weightedBipredIdc == 1 && normalizedSliceType == 1);
    if (usePredWeightTable) {
        slice.predWeightTablePresent = true;
        slice.predWeightTableSummary =
            parsePredWeightTable(numRefIdxL0ActiveMinus1, numRefIdxL1ActiveMinus1, normalizedSliceType == 1);
    }

    if (nalRefIdc != 0) {
        slice.decRefPicMarkingPresent = true;
        if (nalUnitType == 5) {
            slice.noOutputOfPriorPicsFlag = readBitField(QStringLiteral("no_output_of_prior_pics_flag"));
            slice.longTermReferenceFlag = readBitField(QStringLiteral("long_term_reference_flag"));
            slice.decRefPicMarkingSummary =
                QStringLiteral("IDR no_output=%1 long_term_reference=%2")
                    .arg(boolText(slice.noOutputOfPriorPicsFlag), boolText(slice.longTermReferenceFlag));
        } else {
            slice.adaptiveRefPicMarkingModeFlag = readBitField(QStringLiteral("adaptive_ref_pic_marking_mode_flag"));
            if (slice.adaptiveRefPicMarkingModeFlag) {
                QStringList operations;
                while (reader.bitsRemaining() > 0) {
                    const quint32 op = reader.readUE();
                    if (op == 0) {
                        break;
                    }
                    if (op == 1 || op == 3) {
                        operations.append(QStringLiteral("op=%1 difference_of_pic_nums_minus1=%2")
                                              .arg(op)
                                              .arg(reader.readUE()));
                    }
                    if (op == 2) {
                        operations.append(QStringLiteral("op=2 long_term_pic_num=%1").arg(reader.readUE()));
                    }
                    if (op == 3 || op == 6) {
                        operations.append(QStringLiteral("op=%1 long_term_frame_idx=%2")
                                              .arg(op)
                                              .arg(reader.readUE()));
                    }
                    if (op == 4) {
                        operations.append(QStringLiteral("op=4 max_long_term_frame_idx_plus1=%1").arg(reader.readUE()));
                    }
                    if (op > 6) {
                        operations.append(QStringLiteral("unsupported op=%1").arg(op));
                        break;
                    }
                }
                slice.decRefPicMarkingSummary =
                    operations.isEmpty() ? QStringLiteral("adaptive, no operations") : operations.join(QStringLiteral("; "));
            } else {
                slice.decRefPicMarkingSummary = QStringLiteral("not adaptive");
            }
        }
    }

    if (pps.entropyCodingModeFlag && normalizedSliceType != 2 && normalizedSliceType != 4) {
        slice.cabacInitIdc = static_cast<int>(readUEField(QStringLiteral("cabac_init_idc")));
    }
    slice.sliceQpDelta = readSEField(QStringLiteral("slice_qp_delta"));
    if (normalizedSliceType == 3 || normalizedSliceType == 4) {
        if (normalizedSliceType == 3) {
            readBitField(QStringLiteral("sp_for_switch_flag"));
        }
        readSEField(QStringLiteral("slice_qs_delta"));
    }
    if (pps.deblockingFilterControlPresentFlag) {
        const quint32 disableDeblockingFilterIdc = readUEField(QStringLiteral("disable_deblocking_filter_idc"));
        if (disableDeblockingFilterIdc != 1) {
            readSEField(QStringLiteral("slice_alpha_c0_offset_div2"));
            readSEField(QStringLiteral("slice_beta_offset_div2"));
        }
    }
    slice.derivedQp = std::clamp(26 + pps.picInitQpMinus26 + slice.sliceQpDelta, 0, 51);
    if (reader.hasError()) {
        appendHeaderTruncated();
    }
    slice.valid = !reader.hasError();

    if (slice.valid) {
        parseSliceData(reader, slice, pps, sps);
    }
    return slice;
}
