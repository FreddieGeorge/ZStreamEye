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
  tables/helpers. The covered subset currently reaches ctxIdx 252, including
  the coded-block-pattern, `mb_qp_delta`, luma4x4/chroma DC
  coded-block-flag contexts, and the luma4x4
  `significant_coeff_flag` ctxIdx 134-148 and
  `last_significant_coeff_flag` ctxIdx 166-180 contexts plus the
  first through sixth luma4x4 `coeff_abs_level_minus1` prefix bins used
  by the narrow CABAC paths and near-term residual work.
- `cabac/H264CabacDecoder.*`: CABAC arithmetic-decoder foundation.
- `cabac/H264CabacSyntaxTypes.h`: shared result structs for CABAC syntax
  readers.
- `cabac/H264CabacMacroblockSyntaxReader.*`: CABAC macroblock-level syntax
  readers. It currently covers `mb_skip_flag`, `mb_type` prefix bins, I-slice
  `I_NxN`/`I_16x16`/`I_PCM`, and P-slice `P_L0_16x16`,
  `P_L0_L0_16x8`, `P_L0_L0_8x16`, plus `P_8x8` detection. It also has a
  narrow `coded_block_pattern == 0` reader with luma ctxIdx derivation over
  contexts 73-76 and first chroma context 77; non-zero luma/chroma CBP still
  returns incomplete. It also has a narrow `mb_qp_delta == 0` reader for future
  residual-bearing paths; non-zero `mb_qp_delta` returns incomplete.
- `cabac/H264CabacSubMacroblockSyntaxReader.*`: focused P sub-macroblock
  readers for `sub_mb_type`, narrow `ref_idx_l0 == 0`, and narrow
  `mvd_l0` scaffolding. MVD now covers zero and small non-zero components
  (`abs(mvd_l0) <= 3` with bypass sign); larger absolute values still return
  incomplete.
- `cabac/H264CabacResidualSyntaxReader.*`: focused residual CABAC syntax
  skeleton. It currently reads luma 4x4 `coded_block_flag` using ctxIdx 85 for
  luma 8x8 groups selected by the luma `coded_block_pattern_luma` bits, and
  chroma DC `coded_block_flag == 0` using ctxIdx 97 for 4:2:0
  `coded_block_pattern_chroma == 1`. For luma4x4 `coded_block_flag == 1`, it
  reads the 15 explicit `significant_coeff_flag` bins with ctxIdx 134-148.
  If one of those flags is one, it reads the matching
  `last_significant_coeff_flag` with ctxIdx 166-180, stores the partial scan
  indices/flags, continues the significant map when the last flag is zero, and
  if the last flag is one reads the first
  `coeff_abs_level_minus1` prefix bin with ctxIdx 248. If that first prefix
  bin is one, it reads one additional prefix bin with ctxIdx 252, third and
  fourth prefix bins with ctxIdx 253-254, plus fifth and sixth prefix bins that
  reuse ctxIdx 255. If any
  covered prefix step reaches a zero terminal bin, it reads one bypass
  `coeff_sign_flag` and then stops at `residual_coefficients`; if the covered
  prefix bins do not terminate, the next unsupported stage remains the rest of
  `coeff_abs_level_minus1`. If no last significant
  coefficient is found in the 15 explicit bins, the reader treats scan position
  15 as the inferred final coefficient and reads the same narrow
  `coeff_abs_level_minus1` prefix skeleton before stopping. Coefficient-level
  partial results now carry reverse-scan coefficient order plus an explicit
  inferred-final flag, so consumers do not need to infer that state from scan
  index 15. They also carry whether the covered
  `coeff_abs_level_minus1` prefix terminated and the number of one bins seen in
  the prefix, which keeps the current partial result useful without starting
  suffix parsing yet; the tests now lock those suffix-preparation inputs to the
  coefficient scan index for a terminated fifth-bin prefix. A terminated prefix
  with more than three one bins now stops before `coeff_sign_flag` at
  `coeff_abs_level_minus1`, preserving the boundary for future suffix/remaining
  level parsing. That stop is routed through a remaining-level skeleton that
  receives the parsed prefix one-count, reads and records the first four suffix
  bypass bins, and then still returns incomplete without computing
  `coeff_abs_level_minus1`; macroblock-level partial results propagate those
  suffix bins and per-coefficient suffix-bin counts alongside the prefix state.
  Once that remaining-level skeleton has all four currently covered suffix
  bypass bins for a coefficient, it marks the per-coefficient
  `coeffAbsLevelReadyForValueFlags` entry as ready; this is only value-compute
  preparation and still does not calculate a real `coeff_abs_level_minus1`.
  Ready entries also snapshot the prefix one-count in
  `coeffAbsLevelReadyPrefixOneCounts`, so later value computation does not have
  to reconstruct that input from the prefix-bin arrays. The same ready entries
  snapshot the currently covered suffix bins as grouped
  `coeffAbsLevelReadySuffixBins`, while the flat suffix-bin stream remains
  available for diagnostics and compatibility. They also set
  `coeffAbsLevelValueInputCompleteFlags` to mark that the current narrow-path
  value inputs are complete; this still does not mean
  `coeff_abs_level_minus1` has been computed. The fixed ready input currently
  covered by tests (`prefixOneCount == 4` and four zero suffix bins) is marked
  with `coeffAbsLevelFixedInputRecognizedFlags`; it is only an input-shape
  recognition flag, not a decoded coefficient level.
  A pure `h264CabacCoeffAbsLevelMinus1UsesUeg0Suffix()` helper now locks the
  UEG0 `uCoff == 14` cutoff; the currently covered `prefixOneCount == 4`
  remaining-level skeleton input is intentionally below that cutoff, so the
  existing recorded bypass bins have not been reclassified or used to compute a
  value. The next step should first tighten the naming/flag semantics for this
  pre-UEG0 remaining-level input before introducing any real
  `coeff_abs_level_minus1` value helper.
  That tightening has started with
  `coeffAbsLevelPreUeg0RemainingInputFlags`, which marks the currently covered
  fixed input as pre-UEG0 remaining-level input while preserving the older flat
  and grouped suffix-bin diagnostic fields for compatibility. The flag now uses
  the pure `h264CabacCoeffAbsLevelMinus1IsPreUeg0RemainingInput()` helper, which
  is true for non-negative prefix one-counts below the UEG0 cutoff and false at
  or above `uCoff == 14`. Do not rename the older suffix-bin fields in the next
  step; treat them as compatibility/diagnostic bypass-bin recordings until a
  separate remaining-input bin field can be introduced without churn. Parallel
  `coeffAbsLevelRemainingInputBins`, per-coefficient remaining-input counts, and
  grouped ready remaining-input bins now carry that more accurate pre-UEG0
  meaning while preserving the older suffix-bin fields. A pure
  `H264CabacCoeffAbsLevelRemainingInput` helper input plus
  `h264CabacCoeffAbsLevelMinus1HasPreUeg0RemainingInput()` now lock the
  pre-UEG0 remaining-input shape, and the reader uses that helper when setting
  the pre-UEG0 remaining-input flag. A separate pure
  `h264CabacCoeffAbsLevelMinus1CanComputeFromUeg0Suffix()` guard makes the
  current pre-UEG0 input explicitly non-computable by the UEG0 suffix path.
  `h264CabacCoeffAbsLevelMinus1ReadUeg0SuffixValue()` can read a binary UEG0
  suffix value for cutoff-or-later inputs, but it is still a pure helper and is
  not connected to coefficient reconstruction. A paired pure
  `h264CabacCoeffAbsLevelMinus1ComputeFromUeg0Suffix()` helper computes a
  `coeff_abs_level_minus1` value only for cutoff-or-later UEG0 inputs and
  rejects the current pre-UEG0 reader input; it also remains disconnected from
  the reader pipeline. Its output parameter is only the pure helper result; no
  reader result field carries a computed coefficient level yet. A pure
  `h264CabacCoeffAbsLevelMinus1NeedsAdditionalPreUeg0Parsing()` helper marks
  current pre-UEG0 remaining inputs such as `prefixOneCount == 4` plus four bins
  as needing more pre-UEG0 parsing rather than value computation. The residual
  reader now records that helper result in
  `coeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags`, and the macroblock layer
  propagates it as
  `residualCoeffAbsLevelNeedsAdditionalPreUeg0ParsingFlags`; the current ready
  path records `[1]`, while direct-sign and covered-prefix-not-terminated paths
  leave the flag list empty. A parallel ready-group field,
  `coeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags`, records the same
  helper result alongside the ready prefix one-count and ready remaining-input
  bins; macroblock syntax exposes it as
  `residualCoeffAbsLevelReadyNeedsAdditionalPreUeg0ParsingFlags`. A pure
  `h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingTargetPrefixOneCount()`
  helper returns the UEG0 cutoff (`14`) for those pre-UEG0 ready inputs and
  `-1` otherwise; the reader records that target in
  `coeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts`, with
  macroblock propagation through
  `residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingTargetPrefixOneCounts`.
  This target is only a parsing boundary marker, not a computed coefficient
  value. A paired pure
  `h264CabacCoeffAbsLevelMinus1AdditionalPreUeg0ParsingRemainingPrefixBins()`
  helper reports how many pre-UEG0 prefix bins are still needed to reach that
  target (`10` for the current `prefixOneCount == 4` input, `1` for
  `prefixOneCount == 13`, and `-1` when the target does not apply). The reader
  now records the applicable ready-input count in
  `coeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts`, with
  macroblock propagation through
  `residualCoeffAbsLevelReadyAdditionalPreUeg0ParsingRemainingPrefixBinCounts`;
  this still does not read those additional prefix bins. A pure
  `h264CabacCoeffAbsLevelMinus1CanContinuePreUeg0PrefixParsing()` helper now
  combines the existing needs-additional, target, and remaining-prefix-bin
  checks into one guard. The reader records it in
  `coeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags`, with macroblock
  propagation through
  `residualCoeffAbsLevelReadyCanContinuePreUeg0PrefixParsingFlags`; the current
  ready path records `[1]`, while non-ready paths remain empty.
  Direct-sign paths and covered-prefix-not-terminated paths keep the aligned
  ready flag at zero and do not create suffix bins, ready prefix one-counts, or
  ready suffix-bin groups, and they do not set value-input-complete or
  fixed-input-recognized/pre-UEG0 remaining-input flags.
  Prefix-bin context checks, bin decoding, and diagnostic
  messages are centralized in the residual reader so adding the next prefix
  step does not duplicate the first/next/third/fourth/fifth/sixth-bin plumbing.
  Reader-level and macroblock-level regression tests now lock that large
  terminated-prefix boundary before sign-flag parsing.
  The additional covered prefix contexts are table-driven after the first bin.
  The sixth step reuses ctxIdx 255 and has reader tests for both zero
  termination (`prefixOneCount == 5`, nine pre-UEG0 prefix bins remaining) and
  one continuation (`prefixOneCount == 6`). The next prefix expansion should
  add a seventh ctxIdx 255 step plus tests. The reader consumes only the first
  two coefficients in that reverse
  order. Chroma non-zero CBF, complete
  significant/last maps, complete
  coefficient level parsing, suffix parsing, and non-zero coefficient completion are
  not implemented.
- `cabac/H264CabacSyntaxReader.h`: aggregate include for CABAC syntax readers;
  keep it thin.
- `cabac/H264CabacMacroblockParser.*`: CABAC macroblock entry point. It currently
  initializes CABAC state, collects supported syntax into
  `H264CabacMacroblockSyntaxResult`, can append a partial P_8x8
  `MacroblockInfo` skeleton for decoded sub-macroblock syntax, then reports
  structured unsupported/incomplete diagnostics.

Current H.264 coverage:

- SPS/PPS/slice headers with bit-field metadata where practical.
- Common CAVLC I/P macroblock parsing, QP, coded-block pattern, residual block
  counts, and focused non-zero coefficient summaries.
- P-slice L0 motion vectors for supported partition paths, including focused
  P_8x8/P_8x8ref0 fixtures.
- Focused non-direct B-slice L0/L1/Bi motion vectors for 16x16/16x8/8x16.
- Structured diagnostics for unsupported CABAC, B_Direct, B_8x8, MBAFF/FMO,
  malformed/truncated SPS/PPS/slice data, and malformed AVCC lengths.

Current H.264 limitations:

- CABAC macroblock model parsing is not implemented. Groundwork includes
  `cabac_init_idc` on `SliceInfo`, `H264SliceDataContext`, a context-based
  CABAC unsupported entry point, `cabac/H264CabacContextModel.*`, and
  `cabac/H264CabacDecoder.*` bin-decoding primitives. CABAC context-model
  initialization currently covers ctxIdx 0-252, including B-slice skip/type
  starter contexts, P-slice `ref_idx_l0` starter contexts, coded-block-pattern
  contexts, `mb_qp_delta`, luma4x4/chroma DC residual `coded_block_flag`
  contexts, and luma4x4 `significant_coeff_flag` ctxIdx 134-148 and
  `last_significant_coeff_flag` ctxIdx 166-180 contexts plus the first through
  sixth luma4x4 `coeff_abs_level_minus1` prefix bins.
  The CABAC macroblock entry point has a syntax-result boundary for supported
  I/P `mb_type` and narrow P_8x8
  `sub_mb_type`/`ref_idx_l0 == 0`/small `mvd_l0` scaffolding. The P_8x8 path
  now reads narrow `coded_block_pattern` after MVD syntax. It appends a parsed
  macroblock when CBP is zero, and also for one deliberately narrow CBP-nonzero
  case: luma-only CBP with one or more luma CBP bits and all selected luma4x4
  `coded_block_flag` values equal zero, luma4x4 CBF-one with 15 explicit
  `significant_coeff_flag` bins and matching covered
  `last_significant_coeff_flag` bins preserved as an incomplete result, or
  4:2:0 `coded_block_pattern_chroma` equal to 1 with both chroma DC
  `coded_block_flag` values equal zero. Both non-zero paths require
  `mb_qp_delta == 0`. If a covered luma4x4 coded-block flag is one, parsing now
  preserves partial CBF indices, CBF values, significant scan indices/flags,
  last-significant scan indices/flags, coefficient reverse-scan order,
  coefficient-level scan indices, inferred-final flags, first and next prefix
  bins, covered-prefix terminated flags, covered-prefix one counts, and sign
  flags, incomplete block, incomplete scan index, category, and next unsupported
  stage on the syntax result before returning
  `cabac_residual_incomplete`. Chroma DC
  non-zero CBF remains an incomplete boundary without significant-flag parsing.
  For
  `coded_block_pattern_chroma == 2`, the narrow path now preserves chroma DC
  CBF-zero state, then returns `cabac_residual_incomplete` with category
  `chroma_ac` and next stage `coded_block_flag`; chroma AC CBF parsing is still
  not implemented. Non-4:2:0 chroma residual, MVD absolute values greater than
  three, non-zero `mb_qp_delta`, and coefficient parsing remain
  unsupported/incomplete at the macroblock entry point. For inter CBP-zero
  macroblocks, `mb_qp_delta` is not present and is deliberately not consumed.
- CAVLC residual summaries are focused analysis data, not full inverse-scan,
  dequantized, or transformed residual visualization.
- B_Direct, B_8x8 sub-macroblock prediction, MBAFF/interlaced, and FMO remain
  unsupported or diagnostic-only paths.

## Build And Verification

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

1. Broaden the newly wired residual-CABAC path one step at a time. Current
   macroblock coverage is limited to P_8x8 with luma-only CBF-zero residuals,
   luma4x4 CBF-one partial 15-bin `significant_coeff_flag` /
   `last_significant_coeff_flag` skeleton results, inferred final scan-position
   coefficient prefix routing with explicit reverse-scan and inferred-final
   result flags, plus the first `coeff_abs_level_minus1` prefix bin, one
   additional prefix bin when the first prefix bin is one, third through sixth
   prefix bins when the covered prefix bins keep returning one, and one bypass
   `coeff_sign_flag` when the covered prefix bins terminate,
   4:2:0 chroma DC CBF-zero residuals for `coded_block_pattern_chroma == 1`,
   plus a structured chroma AC incomplete boundary for
   `coded_block_pattern_chroma == 2`, all behind `mb_qp_delta == 0`;
   coefficient syntax is still out of scope. Keep syntax-result structs
   separate from final model mutation.
2. Wire only one narrow CABAC macroblock path at a time, preserving structured
   unsupported diagnostics for the first unimplemented syntax element.
3. Keep CABAC modules under `h264/cabac/` and CAVLC modules under `h264/cavlc/`.
   Reuse shared slice state via `H264SliceDataContext`, but keep
   entropy-specific state and tables out of `H264MacroblockParser.cpp`.
4. Next residual step should likely add one very small
   `coeff_abs_level_minus1` prefix/suffix boundary, or continue walking the
   saved reverse-scan coefficient order beyond the first two entries. Do not
   jump directly to full coefficient reconstruction.
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
