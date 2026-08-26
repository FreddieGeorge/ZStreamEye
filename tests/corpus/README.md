# Real Stream Regression Corpus

This directory contains tiny synthetic media files produced with FFmpeg and
libx264. Unlike the hexadecimal parser fixtures in `tests/fixtures`, these
files exercise container probing, packet extraction, H.264 parsing, and video
decoding together through `FFmpegDecoder`.

All visual and audio content is generated from FFmpeg's `testsrc2` and `sine`
filters. No third-party media is included.

| File | Container | H.264 profile | Entropy | B-frames | Audio |
| --- | --- | --- | --- | --- | --- |
| `h264_cavlc_baseline_annexb.h264` | Annex B | Baseline | CAVLC | No | No |
| `h264_cabac_main_bframes.mp4` | MP4 | Main | CABAC | Yes | No |
| `h264_cabac_high_aac.mkv` | Matroska | High | CABAC | Yes | AAC mono |

Each video is 96x64, 12 fps, and 12 frames long. The small dimensions and
fixed encoder settings keep the committed corpus fast and deterministic.

Regenerate the files from the repository root with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate-regression-corpus.ps1
```

The script requires an FFmpeg build with `libx264` and the native AAC encoder.
After regeneration, run the full test suite. Hash changes are intentional only
when the corpus generation contract or encoder output is being updated.
