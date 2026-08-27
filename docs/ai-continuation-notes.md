# AI Continuation Notes

This is the short handoff note for future AI/coding agents working on
ZStreamEye.

Repository:

```text
git@github.com:FreddieGeorge/ZStreamEye.git
```

Typical local path:

```text
D:\Desktop\ZStreamEye
```

For the longer staged roadmap, see `docs/ai-streameye-roadmap.md`. Keep this
file focused on current state, rules, and the next practical work.

## Current State

ZStreamEye is a Qt 6 / FFmpeg desktop bitstream analysis tool. The app has a
frame/access-unit list, property tree, log dock, stats dock, bitstream hex dock,
OpenGL video canvas, playback controls, overlays, JSON/CSV/screenshot export,
and persisted UI settings.

Core architecture:

- `MainWindow` delegates decoded-frame/access-unit cache state to
  `AnalysisStore` and background thread lifecycle to `DecodeSession`.
- `FFmpegDecoder` owns FFmpeg contexts, but stream probing, parser creation,
  packet raw-data capture, and codec-neutral access-unit parsing are pushed
  into focused helpers (`FFmpegStreamInfoBuilder`, `ParserFactory`,
  `PacketRawDataBuilder`, `AccessUnitAnalyzer`).
- `DecodeWorker` is now a thin Qt bridge over `DecodeLoop`.
- `DecodeLoop` is an orchestration layer composed from small helpers
  (`DecodeEventSink`, `DecodeSeekPlanner`, `RebufferProgressTracker`,
  `SeekCheckpointEmitter`, `FramePacing`, `PendingAccessUnitDispatcher`,
  `FirstFramePauseController`, `DecodedFrameAnalysisBuilder`,
  `DecodedFrameDispatcher`).
  Video remains the selected playback path.
- `FrameAnalysis` is the codec-neutral handoff model used by UI, stats, and
  export.
- `StreamInfo` records discovered container streams and basic video/audio
  metadata.
- `AnalysisStats` aggregates access-unit, frame-type, macroblock, QP, motion
  vector, and diagnostic summaries.
- `BitstreamHexView` highlights selected packet byte ranges from
  `AnalysisBitField` metadata.
- Audio parser skeletons exist for AAC ADTS and MP3 frame headers. They emit
  access-unit metadata and diagnostics, but there is no audio playback or deep
  audio syntax parsing yet.
- HEVC has a shallow parser skeleton for NALU/VPS/SPS/PPS/VCL classification
  and graceful unsupported diagnostics. It does not parse full HEVC slice
  headers yet.
- `tests/corpus` contains a deterministic synthetic real-stream regression
  corpus: Baseline/CAVLC Annex B, Main/CABAC MP4 with B-frames, and
  High/CABAC Matroska with B-frames plus AAC. The corpus is generated from
  FFmpeg filters, carries a hash-locked manifest, and is exercised end to end
  through `FFmpegDecoder` by `ZStreamEyeRealStreamCorpusTests`.

H.264 is the deepest parser and is intentionally a direct bitstream parser, not
a wrapper around FFmpeg's H.264 parser.

## H.264 Parser Map

Important H.264 files:

- `H264Parser.cpp`: packet/NALU dispatch, decoder configuration parsing, parser
  state snapshots, SPS/PPS cache management, and top-level helpers.
- `H264ParameterSetParser.cpp`: SPS/PPS/VUI parsing.
- `H264SliceHeaderParser.cpp`: slice header parsing, reference-list summaries,
  prediction-weight summaries, and decoded-reference-picture marking summaries.
- `H264MacroblockParser.cpp`: `slice_data` flow, skip-run handling,
  CAVLC/CABAC dispatch, and top-level unsupported/truncation handling.
- `H264SliceDataContext.h`: internal shared macroblock parsing context,
  diagnostics helpers, and small bit-field readers.
- `cavlc/H264CavlcMacroblockParser.*`: CAVLC macroblock syntax orchestration,
  including macroblock type, intra/P/B syntax, and coded-block pattern flow.
- `cavlc/H264CavlcMacroblockResidualParser.*`: macroblock-level CAVLC residual
  prediction/dispatch and coefficient-state updates.
- `cavlc/H264CavlcResidualParser.*`: CAVLC residual block parsing and coefficient
  summaries.
- `H264MotionVectorParser.*`: motion-vector prediction, MV state updates, and
  supported P/B partition mapping.
- `H264MacroblockTypes.*`: macroblock type naming and coded-block-pattern
  mapping.
- `cabac/H264CabacContextModel.*`: CABAC context-model initialization
  tables/helpers. The covered subset currently reaches ctxIdx 266, including
  the coded-block-pattern, `mb_qp_delta`, luma4x4/chroma DC
  coded-block-flag contexts, and the luma4x4
  `significant_coeff_flag` ctxIdx 134-148 and
  `last_significant_coeff_flag` ctxIdx 166-180 contexts plus the
  first through fourteenth luma4x4 `coeff_abs_level_minus1` prefix bins and
  chroma DC coefficient-level contexts 257-266 used by the narrow CABAC paths.
- `cabac/H264CabacDecoder.*`: CABAC arithmetic-decoder foundation.
- `cabac/H264CabacSyntaxTypes.h`: shared result structs for CABAC syntax
  readers.
- `cabac/H264CabacMacroblockSyntaxReader.*`: CABAC macroblock-level syntax
  readers. It currently covers `mb_skip_flag`, `mb_type` prefix bins, I-slice
  `I_NxN`/`I_16x16`/`I_PCM`, and P-slice `P_L0_16x16`,
  `P_L0_L0_16x8`, `P_L0_L0_8x16`, plus `P_8x8` detection. It also has a
  coded-block-pattern reader with luma ctxIdx derivation over contexts 73-76
  and chroma contexts beginning at 77. Narrow non-zero luma and 4:2:0 chroma
  paths are integrated with the residual reader. It has a complete
  `mb_qp_delta == 0` path; non-zero `mb_qp_delta` remains the current explicit
  incomplete boundary.
- `cabac/H264CabacSubMacroblockSyntaxReader.*`: focused P sub-macroblock
  readers for `sub_mb_type`, narrow `ref_idx_l0 == 0`, and complete
  `mvd_l0` component binarization. MVD uses context offsets `+3`, `+4`, `+5`,
  then saturated `+6` through absolute value 8; absolute values from 9 onward
  decode the order-3 bypass Exp-Golomb suffix and bypass sign. UEG3 growth and
  integer reconstruction are bounded for malformed-stream safety.
- `cabac/H264CabacResidualSyntaxReader.*`: focused residual CABAC syntax
  skeleton. It currently reads luma 4x4 `coded_block_flag` using ctxIdx 85 for
  luma 8x8 groups selected by the luma `coded_block_pattern_luma` bits, and
  chroma DC `coded_block_flag` using ctxIdx 97 for 4:2:0
  `coded_block_pattern_chroma == 1`. For luma4x4 `coded_block_flag == 1`, it
  reads the 15 explicit `significant_coeff_flag` bins with ctxIdx 134-148.
  If one of those flags is one, it reads the matching
  `last_significant_coeff_flag` with ctxIdx 166-180, stores the partial scan
  indices/flags, continues the significant map when the last flag is zero, and
  if the last flag is one reads the first
  `coeff_abs_level_minus1` prefix bin with a state-derived context. The first
  coefficient starts at ctxIdx 248; later first bins select ctxIdx 247-251 from
  the previously decoded magnitude-one and greater-than-one coefficient counts.
  If a first prefix bin is one, every continuation bin for that coefficient
  reuses one state-derived context at ctxIdx 252-256 instead of advancing by
  prefix position. The continuation context saturates at ctxIdx 256 after four
  previously decoded greater-than-one coefficients. If any
  covered prefix step before the cutoff reaches a zero terminal bin, it reads
  one bypass `coeff_sign_flag` and then stops at `residual_coefficients`. If all
  fourteen context-coded bins are one, it decodes the following bypass UEG0
  codeword before the sign flag. If no last significant
  coefficient is found in the 15 explicit bins, the reader treats scan position
  15 as the inferred final coefficient and reads the same narrow
  `coeff_abs_level_minus1` prefix skeleton before stopping. Coefficient-level
  partial results now carry reverse-scan coefficient order plus an explicit
  inferred-final flag, so consumers do not need to infer that state from scan
  index 15. They also carry whether the covered
  `coeff_abs_level_minus1` prefix terminated and the number of one bins seen.
  A covered prefix that terminates below the UEG0 cutoff (`uCoff == 14`) now
  produces a real `coeff_abs_level_minus1` value, reads the immediately
  following bypass bin as `coeff_sign_flag`, and stores the signed coefficient
  level. Reader results expose these as `coeffAbsLevelMinus1Values` and
  `coefficientLevels`; macroblock syntax propagates them as
  `residualCoeffAbsLevelMinus1Values` and `residualCoefficientLevels`.
  Terminated prefixes at one-count 4 and 5 therefore no longer consume four
  incorrect suffix/remaining-input bypass bins.
  The suffix/remaining-input result fields remain empty when a pre-UEG0 prefix
  terminates, because no suffix exists on that path. When all fourteen
  context-coded prefix bins are one, the reader switches to bypass-coded UEG0,
  records the complete variable-length codeword in those fields, computes the
  remaining value through the existing helper, and then reads the sign flag.
  `coeffAbsLevelReadyForValueFlags` is one only for this complete UEG0 path;
  the real coefficient vectors remain the authoritative decoded output.
  `h264CabacCoeffAbsLevelMinus1UsesUeg0Suffix()` and the related pure helpers
  document and test the `uCoff == 14` boundary and are now wired into the
  reader pipeline.
  `h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue()` can read a binary UEG0
  suffix value for cutoff-or-later inputs. It validates real UEG0 codewords
  (`0`, `100`, `101`, `11000`, and so on), rather than treating the suffix as a
  fixed-width integer. A paired pure
  `h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix()` helper computes a
  `coeff_abs_level_minus1` value only for cutoff-or-later UEG0 inputs and
  rejects pre-UEG0 input; the cutoff reader uses it for coefficient reconstruction. A pure
  `h264CabacCoeffAbsLevelMinus1NeedsAdditionalPreUeg0Parsing()` helper describes
  legacy remaining-input shapes but is not used by the corrected terminated-prefix
  reader path. A pure
  `h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingTargetPrefixOneCount()`
  helper returns the UEG0 cutoff (`14`) for those legacy pre-UEG0 inputs and
  `-1` otherwise. A paired pure
  `h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingRemainingPrefixBins()`
  helper reports how many pre-UEG0 prefix bins are still needed to reach that
  target (`10` for `prefixOneCount == 4`, `1` for
  `prefixOneCount == 13`, and `-1` when the target does not apply). A pure
  `h264CabacCoeffAbsLevelMinus1CanContinuePreUeg0PrefixParsing()` helper now
  combines the existing needs-additional, target, and remaining-prefix-bin
  checks into one guard.
  Covered terminated-prefix paths keep the legacy ready flag at zero and do
  not create suffix bins, ready prefix one-counts, or ready suffix-bin groups.
  Prefix-bin context checks, bin decoding, and diagnostic messages are
  centralized in the residual reader. Results preserve the actual first and
  continuation context indices for regression tests and macroblock-level
  propagation.
  Reader-level and macroblock-level regression tests lock the corrected
  terminated-prefix value reconstruction and verify that no false suffix bins
  are consumed.
  The additional covered prefix steps are table-driven after the first bin and
  all reuse the context derived once for that coefficient. Reader and macroblock
  tests cover reaching one-count 14, decoding the UEG0 zero codeword, producing
  `coeff_abs_level_minus1 == 14`, and reconstructing coefficient level 15.
  UEG0 prefix growth is bounded to 29 one bins for malformed-stream safety.
  The reader now consumes every coefficient in the saved reverse-scan order,
  preserving adaptive state across the full covered luma4x4 coefficient loop.
  Successfully decoded non-zero-CBF blocks continue with the remaining selected
  luma4x4 coded-block flags. Per-block significant and reverse-scan offsets
  prevent coefficients from leaking between blocks. For 4:2:0 chroma DC
  `coded_block_flag == 1`, the reader decodes up to four coefficients using
  `significant_coeff_flag` contexts 149-151,
  `last_significant_coeff_flag` contexts 181-183, an inferred final coefficient
  at scan index 3, and adaptive coefficient-level contexts 257-266. It reuses
  the UEG0 cutoff/safety rules, resets adaptive level state per chroma component,
  and propagates signed levels into per-component `ResidualBlockInfo` entries.
  Chroma AC and broader residual categories remain unsupported.
- `cabac/H264CabacSyntaxReader.h`: aggregate include for CABAC syntax readers;
  keep it thin.
- `cabac/H264CabacMacroblockParser.*`: CABAC macroblock entry point. It currently
  initializes one decoder/context set per slice, iterates P-slice macroblocks
  in the required skip/syntax/end order, collects supported syntax into
  `H264CabacMacroblockSyntaxResult`, and appends completed P_Skip, narrow
  P_L0_16x16, and selected P_8x8 macroblocks. It preserves structured
  unsupported/incomplete diagnostics at the first unimplemented syntax.

Current H.264 coverage:

- SPS/PPS/slice headers with bit-field metadata where practical.
- Common CAVLC I/P macroblock parsing, QP, coded-block pattern, residual block
  counts, and focused non-zero coefficient summaries.
- P-slice L0 motion vectors for supported partition paths, including focused
  P_8x8/P_8x8ref0 fixtures.
- Focused non-direct B-slice L0/L1/Bi motion vectors for 16x16/16x8/8x16.
- Narrow CABAC P-slice parsing verified against a deterministic x264 fixture,
  including two consecutive P_L0_16x16 macroblocks, cross-macroblock MVD
  context state, luma4x4 residuals, and 4:2:0 chroma DC residuals.
- Structured diagnostics for unsupported CABAC syntax, B_Direct, B_8x8,
  MBAFF/FMO, malformed/truncated SPS/PPS/slice data, and malformed AVCC lengths.

Current H.264 limitations:

- CABAC macroblock model parsing remains intentionally narrow. Groundwork includes
  `cabac_init_idc` on `SliceInfo`, `H264SliceDataContext`, a context-based
  CABAC unsupported entry point, `cabac/H264CabacContextModel.*`, and
  `cabac/H264CabacDecoder.*` bin-decoding primitives. CABAC context-model
  initialization currently covers ctxIdx 0-266, including B-slice skip/type
  starter contexts, P-slice `ref_idx_l0` starter contexts, coded-block-pattern
  contexts, `mb_qp_delta`, luma4x4/chroma DC residual `coded_block_flag`
  contexts, and luma4x4 `significant_coeff_flag` ctxIdx 134-148 and
  `last_significant_coeff_flag` ctxIdx 166-180 contexts plus the first through
  fourteenth luma4x4 `coeff_abs_level_minus1` prefix bins, chroma DC
  significant/last contexts 149-151 and 181-183, and chroma DC level contexts
  257-266.
  The CABAC P-slice entry point now iterates macroblocks with the required
  `mb_skip_flag` before each non-skipped macroblock and `end_of_slice_flag`
  after every completed macroblock, while reusing one decoder and context set.
  It supports P_Skip plus narrow P_L0_16x16 and P_8x8
  `ref_idx_l0 == 0`/complete `mvd_l0` syntax. P_L0_16x16 keeps explicit
  cross-macroblock MVD state for the first-bin ctxIdxInc derivation. The inter path
  now reads narrow `coded_block_pattern` after MVD syntax. It appends a parsed
  macroblock when CBP is zero, and also for one deliberately narrow CBP-nonzero
  case: luma-only CBP with one or more luma CBP bits and selected luma4x4
  `coded_block_flag` values, including CBF-one blocks with 15 explicit
  `significant_coeff_flag` bins, matching `last_significant_coeff_flag` bins,
  and complete signed coefficient levels, or
  4:2:0 `coded_block_pattern_chroma` equal to 1 with zero or non-zero chroma DC
  `coded_block_flag` values and up to four decoded coefficients per component.
  Both non-zero paths require
  `mb_qp_delta == 0`. If a covered luma4x4 coded-block flag is one, parsing now
  preserves partial CBF indices, CBF values, significant scan indices/flags,
  last-significant scan indices/flags, coefficient reverse-scan order,
  coefficient-level scan indices, inferred-final flags, first and next prefix
  bins, covered-prefix terminated flags, covered-prefix one counts, sign flags,
  decoded pre-UEG0 `coeff_abs_level_minus1` values, and signed coefficient
  levels, plus incomplete block, incomplete scan index, category, and next
  unsupported stage when parsing stops. Completed blocks are written to
  `ResidualBlockInfo`; macroblock coefficient totals and neighbor coefficient
  state are updated. Chroma DC CBF-one parsing is complete for the narrow
  4:2:0 path, including significant/last flags and up to four signed
  coefficients per component. For `coded_block_pattern_chroma == 2`, the
  narrow path preserves chroma DC state, then returns
  `cabac_residual_incomplete` with category
  `chroma_ac` and next stage `coded_block_flag`; chroma AC CBF parsing is still
  not implemented. Non-4:2:0 chroma residual and non-zero `mb_qp_delta` remain
  unsupported/incomplete at the
  macroblock entry point. For inter CBP-zero
  macroblocks, `mb_qp_delta` is not present and is deliberately not consumed.
- CAVLC residual summaries are focused analysis data, not full inverse-scan,
  dequantized, or transformed residual visualization.
- B_Direct, B_8x8 sub-macroblock prediction, MBAFF/interlaced, and FMO remain
  unsupported or diagnostic-only paths.

## Build And Verification

The automated suite includes both small syntax-focused fixtures under
`tests/fixtures` and real container/decode integration cases under
`tests/corpus`. Regenerate the real-stream corpus only when intentionally
changing its generation contract:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate-regression-corpus.ps1
```

Both CABAC container cases decode and analyze all 12 video frames. The corpus
also locks the framing regression where a four-byte AVCC length such as
`00 00 01 ef` must not be mistaken for a three-byte Annex B start code. For
B-frame streams, decoded frames are paired with pending packet analyses by PTS
instead of decode-order FIFO, with FIFO retained as the no-timestamp fallback.
The indexed MP4 and Matroska cases also run repeated real checkpoint seeks in
the order `8 -> 3 -> 9`. `ZStreamEyeRealStreamSeekTests` compares each
rebuffered target with its sequential-decode baseline, including decoded pixel
hash, frame and packet PTS, packet evidence, frame type, and progress events.
`ZStreamEyeRealStreamCorruptionTests` derives malformed inputs from all three
corpus files at runtime. It covers header and tail truncation, damaged AVCC
lengths in indexed containers, truncated packets, missing PPS/SPS diagnostics,
bounded frame output, and a test timeout. Video streams with unresolved zero
dimensions are rejected during open instead of being exposed as valid streams.

For normal Codex/parser work, use the existing utility build:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake --build build-codex-util"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ctest --test-dir build-codex-util --output-on-failure"
```

For a fresh MSYS2 UCRT64 configure/build/test:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64 -DBUILD_TESTING=ON"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake --build build-msys2-ucrt"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ctest --test-dir build-msys2-ucrt --output-on-failure"
```

Run the app from the development environment:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ./build-msys2-ucrt/ZStreamEye.exe"
```

Create a portable package:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\deploy-windows-msys2.ps1
```

The deployment script writes its own release build under
`build-deploy-msys2-ucrt` and package output under `dist/`. Do not distribute a
`ZStreamEye.exe` directly from any build directory.

### User-Facing Windows Verification Rule

- Always give users the deployed executable below for manual verification:
  `D:\Desktop\ZStreamEye\dist\ZStreamEye-windows-ucrt64\ZStreamEye.exe`.
- After a user-facing change, regenerate the portable package with
  `scripts/deploy-windows-msys2.ps1` before asking the user to verify it. Do not
  point users at an executable whose deployment directory has not been
  refreshed for the current change.
- `build-codex-util` is an incremental development build used for compilation
  and automated tests. Its root `ZStreamEye.exe` is only the launcher and
  expects `runtime\ZStreamEyeApp.exe`, which that build directory does not
  provide in the deployed layout. Do not use it as the user verification app.
- `build-deploy-msys2-ucrt` is the deployment script's Release build cache. It
  supplies the binaries that are assembled into `dist`, but is not itself a
  portable package or a user verification directory.
- Both build directories may be kept to speed up later incremental builds and
  may be regenerated if deleted. Only `dist\ZStreamEye-windows-ucrt64` contains
  the launcher, `runtime\ZStreamEyeApp.exe`, Qt plugins, and required runtime
  DLLs in the layout intended for launching and distribution.

## Important Rules

- Do not commit `build*/`, `build-msys2-ucrt/`, `dist/`, or generated package
  artifacts.
- Do not use FFmpeg's H.264 parser as the main syntax parser.
- Keep decoding, seeking, and heavy parsing off the UI thread.
- Parser failures must be fault tolerant: add structured diagnostics or mark
  unsupported syntax; do not crash.
- Keep parser and general code modular and decoupled.
- CABAC must stay layered: context models, arithmetic decoding, syntax
  dispatch, residual parsing, motion-vector updates, and shared slice state
  should remain separate helpers/files with focused tests.
- Reuse `VideoCanvas` coordinate helpers: `videoDisplayRect()`,
  `mapVideoPointToWidget()`, and `macroblockWidgetRect()`.
- After meaningful code changes, run build and tests. If deployment scripts
  change, run the deployment script too.
- For every release tag, add `docs/releases/<tag>.md` before pushing the tag.

## Parser Change Checklist

- Define module boundaries before implementing complex syntax.
- Keep parser functions narrow. If a function grows past roughly 120-150 lines,
  check whether it mixes syntax reading, derived state, diagnostics, shared
  mutation, and model population.
- For new syntax fields, carry data through the relevant public layers by
  default: parser model, JSON export, `PropertyTreeView`, and tests.
- Unsupported syntax should have a stable diagnostic code, clear message,
  parsed/estimated state, and fixture coverage when practical.
- Test small layers first. For CABAC, test context initialization, arithmetic
  decoder state, and narrow syntax helpers before macroblock integration.
- Keep internal parser plumbing private unless UI/export genuinely needs it.
- Do not couple parser internals to UI code.

## General Code Structure Checklist

- Keep layer ownership clear: UI presents state, decode owns FFmpeg/threading,
  parsers own syntax analysis, models carry stable data, export/statistics
  consume models.
- Prefer explicit adapters or model fields at cross-layer boundaries.
- Keep shared state explicit with small context/state structs instead of long
  parameter lists or hidden globals.
- Add abstractions only when they reduce real coupling or repeated logic.
- Test at the lowest useful layer, then add broader regression coverage.

## Next Parser Work

Recommended next H.264 direction:

1. Implement complete non-zero CABAC `mb_qp_delta`. Decode its context-coded
   unary value and signed mapping, derive the first context from previous
   macroblock delta state, update `currentQp` with the H.264 QP wrap rule, and
   propagate both `mbQpDelta` and resulting QP into `MacroblockInfo`.
2. Cover zero, positive, negative, larger, previous-non-zero, truncated, and
   malformed cases in syntax-reader tests. Then advance
   `x264_cabac_residual.h264` and lock the later macroblock address, delta, and
   resulting QP before choosing the next syntax feature.
3. After that real-stream boundary is known, continue one narrow macroblock
   path at a time. The likely residual follow-up is 4:2:0 chroma AC; keep its
   coded-block/significant/last contexts and coefficient category separate
   from chroma DC and luma4x4.
4. Keep CABAC modules under `h264/cabac/` and CAVLC modules under `h264/cavlc/`.
   Reuse shared slice state via `H264SliceDataContext`, but keep
   entropy-specific state and tables out of `H264MacroblockParser.cpp`.
5. Preserve structured unsupported diagnostics for paths that are not ready.

Useful H.264 test areas:

- `tests/test_h264_parser.cpp`
- `tests/test_h264_cabac_decoder.cpp`
- `tests/test_h264_cabac_syntax_reader.cpp`
- `tests/fixtures/`
- `tests/test_analysis_stats.cpp`

## Other Useful Context

Playback/seek:

- Recent decoded frames are cached.
- Old-frame cache misses rebuffer from the nearest keyframe/IDR checkpoint.
- Checkpoints preserve enough parser state to resume syntax parsing after seek.
- Rebuffer generation guards prevent stale callbacks from replacing newer UI
  state.

Export/UI:

- `PropertyTreeView` and JSON export should consume stable model data rather
  than scraping UI text or parser internals.
- `VideoCanvas` playback/status OSD text uses regular child widgets above the
  `QOpenGLWidget`, not `QPainter::drawText()` inside `paintGL()`. Keep text out
  of the OpenGL painter path because that path rendered unreliably on some
  Windows deployments. Shape overlays (grid, QP, and motion vectors) remain in
  the painter path.
- The optional playback OSD shows the current timestamp, current access-unit
  bitrate, decoded resolution, and codec. Timestamp conversion uses the
  selected video stream time base. Bitrate uses packet size and packet duration
  when available, with a visibly approximate frame-rate fallback.
- `PropertyTreeView` caps displayed macroblocks for responsiveness; overlay
  data still uses the parsed macroblock list.

Release/deployment:

- Windows packaging uses a root launcher and places runtime files under
  `runtime/`.
- Release workflow expects `docs/releases/<tag>.md` for tagged releases.
- The installer is not code signed yet.

## Quick Orientation

Most important files:

```text
src/app/MainWindow.*
src/app/AnalysisStore.*
src/app/DecodeSession.*
src/core/decode/DecodeWorker.*
src/core/decode/DecodeLoop.*
src/core/decode/FFmpegDecoder.*
src/core/model/FrameAnalysis.*
src/core/analysis/AnalysisStats.*
src/core/export/AnalysisExportWriter.*
src/core/parser/BitstreamParser.*
src/core/parser/video/h264/H264*.*
src/core/parser/video/hevc/HevcParser.*
src/core/parser/audio/*
src/core/util/*
src/ui/BitstreamHexView.*
src/ui/FrameListView.*
src/ui/PropertyTreeView.*
src/ui/StatsDock.*
src/ui/VideoCanvas.*
tests/*
scripts/deploy-windows-msys2.ps1
.github/workflows/windows-msys2.yml
```

Before parser changes, read the relevant parser module plus `SliceInfo`,
`MacroblockInfo`, `FrameAnalysis`, `AnalysisExportWriter`, and
`PropertyTreeView`.

Before playback/seek changes, read `MainWindow::handleFrameReady`,
`MainWindow::showFrameFromCache`, `MainWindow::seekToFrame`, and
`DecodeSession::start`, `DecodeWorker::decodeFileFromCheckpoint`, and
`DecodeLoop::run`.

# Real CABAC residual fixture

`tests/fixtures/x264_cabac_residual.h264` is the first deterministic real-encoder
CABAC residual fixture. It exposed two integration gaps that synthetic context
tests did not cover: CABAC alignment was not consumed before decoder
initialization, and the P `sub_mb_type` bin directions were reversed. The
fixture now follows the correct P-slice ordering and completes two consecutive
P_L0_16x16 macroblocks. It preserves cross-macroblock MVD context state and
asserts stable non-zero luma4x4 coefficients: the first macroblock has luma CBP
15 and 38 coefficients, and the second has luma CBP 12 and 21 coefficients.
Parsing then reaches the next explicit boundary, a later macroblock with
non-zero `mb_qp_delta`.
