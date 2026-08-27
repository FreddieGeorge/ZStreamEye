# AI Roadmap Toward a StreamEye-Class Analyzer

This staged roadmap describes how to grow ZStreamEye from a H.264-focused
desktop analyzer into a broader professional bitstream analysis tool. Keep
current implementation details in `docs/ai-continuation-notes.md`; keep this
file focused on direction and sequencing.

Current foundation:

- Qt 6 desktop UI with frame/access-unit list, property tree, log dock, stats
  dock, bitstream hex dock, and OpenGL canvas.
- FFmpeg background decoding through `DecodeWorker`.
- Codec-neutral parser and analysis models: `IBitstreamParser`,
  `FrameAnalysis`, `MediaKind`, `AccessUnitKind`, and `StreamInfo`.
- Direct H.264 parser for Annex B and AVCC packets, with focused CAVLC coverage
  and a real-encoder-verified narrow CABAC P-slice macroblock/residual/MV path.
- Shallow HEVC, AAC ADTS, and MP3 parser skeletons that validate the generic
  access-unit path.
- JSON/CSV/screenshot exports, persisted UI settings, Windows CI, and release
  packaging.

## Guiding Principle

Do not chase StreamEye by adding isolated UI buttons. Build depth first:

1. Make H.264 analysis correct and trusted.
2. Keep generic UI/export/statistics paths codec-neutral.
3. Add bit/hex synchronization and useful statistics.
4. Add quality comparison.
5. Deepen HEVC and add more codecs.
6. Add explicit audio/container workflows.
7. Add automation, reports, and product hardening.

## Stage 1: Deepen H.264 Correctness

Goal: make H.264 overlays and macroblock data trustworthy instead of estimated.

Key work:

- Broaden CAVLC residual, P_8x8/P_8x8ref0, and B-slice motion-vector coverage.
- Add B_Direct, B_8x8, MBAFF/interlaced, and FMO support or precise diagnostics.
- Continue CABAC in layers. Context models, arithmetic decoding, syntax
  helpers, and narrow P-slice macroblock integration are in place; extend one
  real-stream boundary at a time without collapsing those layers.
- Keep H.264 entropy modules separated under `h264/cabac/` and `h264/cavlc/`;
  shared slice state, macroblock type helpers, and motion-vector helpers should
  stay outside those folders.
- Preserve structured diagnostics for malformed, truncated, and unsupported
  streams.

Done when:

- QP heatmap and MV overlay distinguish parsed data from unsupported data.
- Unsupported syntax reports stable diagnostics instead of generic notes.
- Regression fixtures cover I/P/B, residuals, P_8x8, B-slice modes, CABAC
  unsupported/supported paths, and truncation.

## Stage 2: Keep FrameAnalysis Codec-Neutral

Goal: avoid forcing all analysis through H.264-shaped structs.

Current status: mostly complete for the existing H.264, HEVC skeleton, AAC, and
MP3 paths.

Key work:

- Continue shrinking direct H.264 includes outside codec-specific UI/export
  code.
- Keep codec-specific details nested below stable generic `FrameAnalysis` data.
- Use new non-H.264 parser work to prove the generic UI/export path degrades
  gracefully.

Done when:

- `FFmpegDecoder` and `DecodeWorker` can hand off access units without
  UI-facing codec branches.
- H.264 still displays rich details, but generic views consume generic fields.

## Stage 3: Improve Bitstream Hex And Bit-Offset Navigation

Goal: provide StreamEye-style syntax-to-bitstream traceability.

Current status: `BitstreamHexView` exists and can highlight selected packet
byte ranges from `AnalysisBitField` metadata.

Key work:

- Expand exact bit offsets for macroblock, residual, and container-wrapper
  fields.
- Add clearer sub-byte visualization in the hex dock.
- Make property-tree selection and hex selection round-trip for common syntax
  fields.
- Include useful bit-offset metadata in exports.

Done when:

- SPS/PPS/slice fields navigate to exact bit ranges.
- Macroblock and residual syntax ranges are highlighted where parser coverage
  exists.
- Large streams remain responsive through paging or lazy loading.

## Stage 4: Add Analysis Charts And Statistics

Goal: provide useful encoder-analysis summaries before quality metrics.

Key work:

- Extend `AnalysisStats` / `StatsDock` from distribution rows to graphical
  charts.
- Add QP distribution and QP-over-time charts.
- Add bitrate, frame-type, macroblock/CTU/superblock type, MV magnitude, and
  residual summaries.
- Add GOP structure timeline.

Done when:

- Charts are derived from internal analysis data, not UI text.
- CSV/JSON exports expose the same aggregate statistics.
- Large streams stay responsive through bounded caches or background
  aggregation.

## Stage 5: Add Reference Comparison And Quality Metrics

Goal: move toward StreamEye Studio-style quality analysis.

Key work:

- Add reference YUV loading and frame synchronization by index/PTS.
- Add PSNR, SSIM, frame difference view, and heatmap/split comparison modes.
- Later add VMAF/libvmaf, MS-SSIM, VIF, and batch quality reports.

Done when:

- Users can load encoded stream plus reference YUV and inspect per-frame
  metrics.
- Metrics export to CSV/JSON.
- Pixel format, bit depth, resolution, and frame-count mismatches are reported
  clearly.

## Stage 6: Deepen HEVC/H.265

Goal: become a multi-codec analyzer instead of a H.264-only tool.

Prerequisite: Stage 2 should stay solid enough that HEVC does not add
codec-specific shortcuts to generic UI/export code.

Key work:

- Extend the existing `HevcParser` from NALU/VPS/SPS/PPS/VCL classification to
  SPS dimensions, slice segment headers, and slice-type labels.
- Add CTU grid, QP map, basic CU/PU/TU structure, L0/L1 MVs, reference picture
  sets, and DPB summaries.

Done when:

- HEVC streams show basic frame/syntax info through generic UI/export paths.
- CTU-level grid/QP overlay works for supported streams.
- H.264 behavior remains unchanged.

## Stage 7: Add Container And Stream-Level Analysis

Goal: connect elementary-stream analysis with container and transport context.

Current status: parsed access units already preserve packet index, stream
index, PTS/DTS, duration, byte position, size, keyframe flag, media kind, codec
kind, and raw packet bytes.

Key work:

- Add a container timeline and decoded-frame-to-packet navigation.
- Add MP4/MKV/TS packet mapping, extraction helpers, and TS/PES continuity or
  timestamp diagnostics where practical.
- Add explicit audio/container workflows without tying them to `VideoCanvas`.

Done when:

- A decoded frame can be traced back to container packets.
- Container and decoded-frame timelines are both visible.
- Timestamp discontinuities or missing keyframe indexes are diagnosed.

## Stage 8: Add CLI, Batch Reports, And Automation

Goal: make the analyzer useful in CI, encoder regression, and batch workflows.

Key work:

- Add command-line analysis mode.
- Export JSON/CSV/HTML reports without opening the GUI.
- Add compare mode for two encodes and batch quality metrics.
- Add deterministic exit codes for pass/warn/fail.

Example future commands:

```bash
ZStreamEyeCLI analyze input.264 --json report.json
ZStreamEyeCLI compare encoded.mp4 reference.yuv --metrics psnr,ssim
```

Done when:

- Parser tests can run against a fixture corpus from the CLI.
- CI can fail on parse crashes or configured conformance errors.
- GUI and CLI share parser/export code.

## Stage 9: Product Hardening

Goal: improve robustness, performance, and release confidence.

Key work:

- Add large-file indexing cache and bounded syntax caches.
- Harden seek/rebuffer cancellation, progress, and stale-callback handling.
- Add background parse/aggregation queues.
- Add crash-safe malformed-stream handling and a redistributable regression
  corpus.
- Add Linux CI and release signing later when practical.

Done when:

- Long streams remain usable.
- Repeated seek/click workflows do not queue stale work.
- Malformed streams produce diagnostics rather than crashes.
- CI covers parser, packaging, and representative fixture smoke tests.

## Suggested Next Commit Themes

Use focused commits. Current high-value themes:

```text
Complete CABAC non-zero mb_qp_delta and QP state updates
Advance the real x264 CABAC fixture to its next syntax boundary
Add narrow 4:2:0 CABAC chroma AC residuals
Complete CABAC P_16x8 and P_8x16 motion paths
Add more real-encoder CABAC P-slice fixtures
Add CABAC P-slice intra fallback
Parse H264 B_Direct motion vectors
Parse H264 B_8x8 sub-macroblock motion vectors
Improve macroblock/residual bit-offset highlighting
Add QP and frame-type charts
Add repeated old-frame rebuffer UI smoke coverage
Add packaged Windows layout smoke validation
Deepen HEVC parser skeleton
```

## Notes For Future Agents

- Preserve current H.264 behavior while refactoring.
- Keep heavy parsing and seeking off the UI thread.
- Add fixtures before or alongside parser behavior changes.
- Do not rely on FFmpeg's H.264 parser for syntax analysis.
- Prefer codec-neutral UI/export paths, with codec-specific details nested
  below.
- If a stream is unsupported, log structured diagnostics and keep playback
  usable.
