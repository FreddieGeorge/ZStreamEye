# ZStreamEye

[![Windows MSYS2](https://github.com/FreddieGeorge/ZStreamEye/actions/workflows/windows-msys2.yml/badge.svg)](https://github.com/FreddieGeorge/ZStreamEye/actions/workflows/windows-msys2.yml)

English | [简体中文](README.zh-CN.md)

ZStreamEye is a cross-platform desktop application framework for inspecting H.264 bitstreams. It is built with C++17, Qt 6 Widgets, `QOpenGLWidget`, FFmpeg, and CMake.

The project currently supports video loading/decoding, controllable playback, H.264 syntax parsing, macroblock-level overlays, export tools, a dockable analyzer UI, and OpenGL-based video rendering.

## Features

- Open local H.264/video files through `File -> Open` or drag and drop.
- Decode video streams in a background thread with FFmpeg.
- Discover container streams, including basic video/audio metadata, while still
  decoding the best video stream by default.
- Play, pause, stop, and step frame-by-frame.
- Rebuffer evicted frames from indexed keyframe/IDR seek checkpoints when available.
- Render decoded frames through `QOpenGLWidget`.
- Route bitstream parsing through a codec-neutral parser interface so additional codecs can be added without coupling them to `FFmpegDecoder`.
- Provide an HEVC/H.265 parser skeleton that identifies NAL units, VPS/SPS/PPS,
  VCL access units, and graceful unsupported slice diagnostics.
  HEVC frame-list `Type` values are currently coarse parser labels: `IRAP`
  means an intra random access point such as IDR/CRA/BLA, while `VCL` means a
  video-coding-layer access unit whose detailed slice type has not yet been
  parsed.
- Provide an AAC ADTS parser skeleton for audio access units, including header
  bit fields and graceful diagnostics for malformed packets.
- Provide an MP3 frame-header parser skeleton for MPEG audio frame access units.
- List parsed audio access units from discovered AAC/MP3 audio streams alongside
  video frames, and show their generic fields in the property tree.
- Filter the access-unit list by discovered container stream, by video/audio
  media kind, or to access units that carry diagnostics.
- Preserve packet metadata and raw packet bytes on parsed access units so later
  hex/bit views can trace fields back to packet evidence.
- Show a read-only bitstream hex dock for the selected access unit's packet
  bytes, with byte-range highlighting driven by syntax bit-field selection.
- Show an analysis statistics dock with access-unit counts, frame-type counts,
  frame-type distribution, macroblock/QP summaries, QP bucket distribution,
  motion-vector L0/L1/Other list counts, motion-vector magnitude summaries, and
  diagnostic distribution derived from internal `FrameAnalysis` data.
- Parse H.264 NALU, SPS, PPS, Slice Header, selected CAVLC macroblock fields,
  residual block/coefficient summaries, QP, P-slice L0 motion vectors, focused
  non-direct B-slice L0/L1/Bi motion vectors, and a real-encoder-verified
  narrow CABAC P-slice path with the custom `H264Parser`.
- Show frame syntax information in a dockable property tree.
- Show overlay availability in the property tree, including QP range/constant
  notes and motion-vector support diagnostics for the current frame.
- Show frame list information such as frame type, POC, and `frame_num`.
- Overlay analysis data on the video canvas:
  - 16x16 macroblock grid
  - macroblock QP heatmap
  - parsed P-slice motion vectors
- Show an optional playback-information OSD over the video with readable
  current/total time, current access-unit bitrate, decoded resolution, and
  codec. A leading `~` marks bitrate estimated from packet size and frame rate
  when packet duration is unavailable.
- Toggle grid/QP/MV/playback-information overlays and adjust analysis-overlay
  opacity. Playback information can be toggled from the overlay toolbar or
  `View -> Show Playback Info`.
- Persist window layout, dock positions, overlay toggles, opacity, and recent open/export directories.
- Export selected access-unit syntax JSON, access-unit list CSV, screenshots
  with overlays, and all decoded access-unit syntax JSON with schema/version
  and stream/packet metadata.
- Generate a Windows portable package that includes Qt, FFmpeg, and runtime DLLs.
- Check GitHub Releases for updates from `Help -> Check for Updates`.

Current limitations:

- CAVLC residual parsing exposes block summaries and non-zero coefficient
  summaries with scan index, level, and run-before values. It is still a
  focused CAVLC path, not a full inverse-scan/transform visualization.
- HEVC frame types are currently reported as coarse `IRAP` / `VCL` labels from
  NAL-unit classification. Full I/P/B-style HEVC slice typing requires deeper
  HEVC slice-header parsing.
- CABAC parsing is intentionally narrow: real x264 P-slice coverage currently
  includes P_Skip, P_L0_16x16, selected P_8x8 syntax, complete `mvd_l0`,
  luma4x4 residuals, and 4:2:0 chroma DC residuals. Non-zero CABAC
  `mb_qp_delta`, chroma AC, broader P partitions, CABAC intra/B macroblocks,
  B_Direct/B_8x8 motion vectors, and MBAFF/FMO remain unsupported or
  diagnostic-only paths.
- The property tree limits displayed macroblocks to keep playback responsive; overlay data still uses the parsed macroblock list.
- Text OSD elements are regular Qt child widgets above the OpenGL canvas rather
  than `QPainter::drawText()` calls inside `paintGL()`, avoiding known text
  rendering problems on some Windows/OpenGL deployments.

## Project Structure

```text
ZStreamEye/
+-- docs/
+-- scripts/
+-- src/
|   +-- app/
|   |   +-- CMakeLists.txt
|   +-- core/
|   |   +-- CMakeLists.txt
|   |   +-- analysis/
|   |   +-- decode/
|   |   +-- export/
|   |   +-- model/
|   |   +-- parser/
|   |   |   +-- audio/
|   |   |   +-- video/
|   |   |   |   +-- h264/
|   |   |   |   +-- hevc/
|   |   +-- util/
|   |   +-- buffering/
|   +-- platform/
|   |   +-- windows/
|   +-- ui/
|-- tests/
|   +-- CMakeLists.txt
+-- CMakeLists.txt
+-- README.md
+-- README.zh-CN.md
+-- vcpkg.json
```

Folder responsibilities:

- `src/app`: application window, menu, toolbar, dock layout, file opening, export/update controllers, `AnalysisStore`, `DecodeSession`, and workflow wiring.
- `src/core/model`: stream metadata, media types, decoded frame/seek models, frame analysis data, and document state.
- `src/core/analysis`: codec-neutral playback metrics, aggregation, and statistics over parsed access units.
- `src/core/parser`: codec-neutral parser interface plus audio and video parser modules.
- `src/core/util`: codec-neutral low-level helpers for bit reading, bytestreams, and RBSP/packet bit-range mapping.
- `src/core/decode`: FFmpeg decoder wrapper plus staged decode helpers for stream probing, parser creation, packet raw-data capture, decode-loop orchestration, event sinks, seek planning, rebuffer tracking, frame pacing, checkpoint emission, pending-access-unit dispatch, first-frame pause control, and decoded-frame dispatch.
- `src/core/export`: analysis export serialization.
- `src/core/buffering`: buffering and seek rebuffer state helpers.
- `src/platform`: platform-specific launchers and integration code.
- `src/ui`: reusable Qt widgets such as frame list, property tree, log dock, and video canvas.
- `scripts`: build/deployment helper scripts.
- `docs`: developer notes, deployment notes, and AI continuation roadmap.

## Requirements

- CMake 3.21 or later.
- Qt 6 with `Core`, `Widgets`, `Network`, `OpenGL`, and `OpenGLWidgets`.
- FFmpeg development libraries:
  - `avformat`
  - `avcodec`
  - `avutil`
  - `swscale`
- C++17 compiler:
  - Windows: MSVC 2022, MinGW-w64, or MSYS2 UCRT64.
  - macOS: Apple Clang / Xcode Command Line Tools.
  - Linux: GCC or Clang.

Official references:

- Qt installation: https://doc.qt.io/qt-6/get-and-install-qt.html
- Qt with CMake: https://doc.qt.io/qt-6/cmake-get-started.html
- CMake downloads: https://cmake.org/download/
- FFmpeg API documentation: https://ffmpeg.org/doxygen/trunk/
- vcpkg FFmpeg package: https://vcpkg.io/en/package/ffmpeg.html

## Windows Development Environment

The current verified Windows development setup uses MSYS2 UCRT64:

- GCC/G++ 16.1.0
- CMake 4.3.2
- Ninja 1.13.2
- Qt 6.11.0
- FFmpeg 8.1.1

See [installEnv.md](installEnv.md) for the installation log and verification results.

Configure and build:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake --build build-msys2-ucrt"
```

Run from the development environment:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ./build-msys2-ucrt/ZStreamEye.exe"
```

Do not distribute `build-msys2-ucrt/ZStreamEye.exe` alone. It depends on DLLs from the MSYS2 UCRT64 environment.

## Windows Portable Package

Create a portable package:

```powershell
.\scripts\deploy-windows-msys2.ps1
```

The deployment script configures and builds its own release directory,
`build-deploy-msys2-ucrt`, before collecting runtime DLLs. It does not depend
on the development `build-msys2-ucrt` directory.

Output:

```text
dist/ZStreamEye-windows-ucrt64/
dist/ZStreamEye-windows-ucrt64.zip
```

The portable package includes:

- `ZStreamEye.exe`
- Qt runtime DLLs
- Qt plugins, such as `platforms/qwindows.dll`
- FFmpeg DLLs, such as `avcodec-62.dll`
- MSYS2 UCRT64/GCC runtime DLLs

End users only need to unzip the package and run `ZStreamEye.exe`. Runtime DLLs
and Qt plugins are grouped under `runtime/`.

## Windows Installer

Create a Windows installer after the portable package has been generated:

```powershell
.\scripts\deploy-windows-msys2.ps1
.\scripts\package-windows-installer.ps1 -Version "0.1.8"
```

Output:

```text
dist/ZStreamEye-0.1.8-windows-ucrt64-setup.exe
```

The installer is built with Inno Setup 6 and installs the same self-contained
Qt, FFmpeg, and MSYS2 UCRT64 runtime files as the portable package. GitHub
release builds publish both the portable `.zip` and the installer `.exe`.
Tag releases as `vX.Y.Z`; the release workflow verifies that the tag matches
the CMake project version before publishing GitHub Release assets.

For more details, see [docs/windows-deployment.md](docs/windows-deployment.md).

## macOS

Install dependencies:

```bash
xcode-select --install
brew install cmake qt ffmpeg pkg-config
```

If CMake cannot find Qt:

```bash
export CMAKE_PREFIX_PATH="$(brew --prefix qt)"
```

Build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

Run:

```bash
open build/ZStreamEye.app
```

## Linux

Ubuntu / Debian:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config qt6-base-dev qt6-base-dev-tools libgl1-mesa-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

Fedora:

```bash
sudo dnf install gcc-c++ cmake ninja-build pkgconf-pkg-config qt6-qtbase-devel mesa-libGL-devel ffmpeg-devel
```

Build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Run:

```bash
./build/ZStreamEye
```

## Development Notes

Key modules:

- `AnalysisStore`: keeps recent decoded frames, access-unit analyses, and seek checkpoints out of `MainWindow`.
- `DecodeSession`: owns decode thread/worker lifetime and generation-guarded signal forwarding.
- `FFmpegDecoder`: wraps `AVFormatContext`, `AVCodecContext`, `AVPacket`, and `AVFrame`; delegates stream probing and codec-neutral access-unit parsing to focused helpers.
- `MediaTypes`: media/access-unit identifiers used to keep future video and
  audio analysis paths from being forced into video-only fields.
- `BitstreamParser`: codec-neutral parser interface and codec kind identifiers.
- `AacAdtsParser`: thin audio access-unit skeleton for ADTS header fields and
  diagnostics.
- `Mp3FrameParser`: thin audio access-unit skeleton for MPEG audio frame
  headers, bitrate/sample-rate fields, and malformed-header diagnostics.
- `DecodeWorker`: Qt-facing bridge that forwards play/pause/step/stop and delegates the decode run to `DecodeLoop`.
- `DecodeLoop`: playback/rebuffer orchestration layer built from focused helpers such as `DecodeEventSink`, `DecodeSeekPlanner`, `RebufferProgressTracker`, `SeekCheckpointEmitter`, `FramePacing`, `PendingAccessUnitDispatcher`, `FirstFramePauseController`, `DecodedFrameAnalysisBuilder`, and `DecodedFrameDispatcher`.
- `H264Parser`: directly parses H.264 syntax without relying on FFmpeg's parser.
- `ParserFactory` and `PacketRawDataBuilder`: keep parser instantiation and packet evidence capture out of `FFmpegDecoder`.
- `H264SyntaxJsonWriter` and `H264PropertyTreeBuilder`: keep JSON export and property-tree presentation out of decoder/parser internals.
- `VideoCanvas`: renders video frames and shape overlays, and hosts the stable
  widget-based playback/status OSD above the OpenGL surface.
- `FrameListView` and `PropertyTreeView`: display parsed access units, packet
  metadata, and syntax information.
- `BitstreamHexView`: displays selected access-unit packet bytes in bounded
  pages, highlights byte ranges for selected bit fields, shows a bit-level
  mask preview for the selected range, and can select a covering syntax field
  when the user clicks a hex byte. Bit fields carry an `offset_basis`; packet
  byte highlighting follows packet-relative fields directly and uses normalized
  packet bit ranges for H.264 RBSP-relative SPS/PPS/slice header and selected
  macroblock syntax fields. H.264 summary rows in `PropertyTreeView` drive
  hex selection directly, so users do not need to open a separate bit-position
  sub-tree for common fields.
- `AnalysisStats` and `StatsDock`: aggregate `FrameAnalysis` records into
  codec-neutral stream summaries for access units, frame types, macroblocks,
  QP, motion vectors, and diagnostics. The dock shows simple count/percentage
  distribution rows for frame types, QP buckets, motion-vector L0/L1/Other list
  counts, and diagnostic code/severity groups.

Recommended next work:

1. Implement complete non-zero CABAC `mb_qp_delta` decoding, QP state updates,
   and real-fixture assertions; this is the current first boundary in
   `x264_cabac_residual.h264`.
2. Continue the real CABAC fixture to its next actual unsupported syntax, then
   add narrow 4:2:0 chroma AC and P_16x8/P_8x16 paths as encountered.
3. Smoke-test the latest Windows installer/portable package and upgrade path.
4. Add richer packet/access-unit browsing on top of the AAC/MP3 skeletons; keep
   audio analysis separate from `VideoCanvas`. The current stream selector
   filters the decoded access-unit list; it does not yet switch the FFmpeg
   playback stream or provide audio-only analysis.
5. Expand the statistics dock with graphical histograms/charts and exportable aggregate
   summaries, keeping aggregation in `src/core/analysis`.
6. Expand the bitstream hex dock with graphical sub-byte decoration and broader
   offset normalization for macroblock fields and container/elementary-stream
   wrappers.
7. Broaden CABAC real-encoder fixtures before starting full intra/B-slice
   macroblock families.

For future AI/coding agents, see [docs/ai-continuation-notes.md](docs/ai-continuation-notes.md).
For the longer-term StreamEye-class roadmap, see [docs/ai-streameye-roadmap.md](docs/ai-streameye-roadmap.md).

## Repository Hygiene

Do not commit generated build or release outputs:

```text
build*/
dist/
```

These paths are ignored by `.gitignore`.
