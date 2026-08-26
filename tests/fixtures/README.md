# H.264 Parser Fixtures

These tiny text fixtures store H.264 packets as hexadecimal bytes so they remain
reviewable in source control.

Full media files used for container, packet, parser, and decoder integration
coverage live in the sibling `tests/corpus` directory.

They are intentionally minimal parser fixtures, not visual-quality video clips:

- `annexb_sps_pps_idr_i.hex`: Annex B SPS/PPS/IDR I-slice with one 16x16 macroblock.
- `avcc_sps_pps.hex`: AVCC/length-prefixed SPS/PPS packet.
- `avcc_length_exceeds_packet.hex`: AVCC/length-prefixed packet whose declared NALU length exceeds the available bytes.
- `cavlc_i_qp_delta.hex`: CAVLC I-slice that exposes a non-zero `mb_qp_delta`.
- `cavlc_p_skip.hex`: CAVLC P-slice skip macroblock.
- `cavlc_p_motion_vector.hex`: CAVLC P-slice with a non-zero L0 motion vector delta.
- `cavlc_p_residual_then_motion_vector.hex`: CAVLC P-slice that verifies residual parsing can continue into a later motion-vector macroblock.
- `unsupported_cabac_p.hex`: CABAC P-slice used to assert graceful unsupported diagnostics.
- `truncated_sps.hex`: SPS NALU cut off before required fields are complete.
- `truncated_pps.hex`: PPS NALU cut off before required fields are complete.
- `truncated_slice_header.hex`: P-slice NALU cut off before the slice header is complete.
- `truncated_p_slice_data.hex`: P-slice cut off during `slice_data`, used to assert structured truncation diagnostics.

Additional H.264 parser tests build tiny packets in memory with `BitWriter`
when that is clearer than storing hex. These currently cover CAVLC
P_8x8/P_8x8ref0 sub-macroblock L0 motion vectors and B-slice unsupported
diagnostics.
