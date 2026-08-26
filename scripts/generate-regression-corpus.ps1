param(
    [string]$FfmpegPath = "C:\msys64\ucrt64\bin\ffmpeg.exe",
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot "tests\corpus"
}

if (-not (Test-Path -LiteralPath $FfmpegPath -PathType Leaf)) {
    throw "FFmpeg was not found at '$FfmpegPath'. Pass -FfmpegPath explicitly."
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$commonVideoArgs = @(
    "-hide_banner", "-loglevel", "error", "-y",
    "-f", "lavfi", "-i", "testsrc2=size=96x64:rate=12",
    "-frames:v", "12", "-pix_fmt", "yuv420p",
    "-c:v", "libx264", "-threads", "1",
    "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:v", "+bitexact"
)

function Invoke-CorpusFfmpeg {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & $FfmpegPath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "FFmpeg failed with exit code $LASTEXITCODE."
    }
}

$baselineOutput = Join-Path $OutputDirectory "h264_cavlc_baseline_annexb.h264"
Invoke-CorpusFfmpeg -Arguments ($commonVideoArgs + @(
    "-profile:v", "baseline",
    "-x264-params", "cabac=0:bframes=0:keyint=6:min-keyint=6:scenecut=0:ref=1:aud=1:repeat-headers=1",
    "-bsf:v", "filter_units=remove_types=6",
    "-f", "h264", $baselineOutput
))

$mainOutput = Join-Path $OutputDirectory "h264_cabac_main_bframes.mp4"
Invoke-CorpusFfmpeg -Arguments ($commonVideoArgs + @(
    "-profile:v", "main",
    "-x264-params", "cabac=1:bframes=2:keyint=6:min-keyint=6:scenecut=0:ref=2:threads=1",
    "-bsf:v", "filter_units=remove_types=6",
    "-movflags", "+faststart", $mainOutput
))

$highOutput = Join-Path $OutputDirectory "h264_cabac_high_aac.mkv"
$highArgs = @(
    "-hide_banner", "-loglevel", "error", "-y",
    "-f", "lavfi", "-i", "testsrc2=size=96x64:rate=12",
    "-f", "lavfi", "-i", "sine=frequency=880:sample_rate=48000:duration=1",
    "-map", "0:v:0", "-map", "1:a:0", "-frames:v", "12", "-shortest",
    "-pix_fmt", "yuv420p", "-c:v", "libx264", "-profile:v", "high", "-threads", "1",
    "-x264-params", "cabac=1:bframes=2:keyint=6:min-keyint=6:scenecut=0:ref=2:threads=1",
    "-bsf:v", "filter_units=remove_types=6",
    "-c:a", "aac", "-b:a", "64k", "-ac", "1",
    "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:v", "+bitexact",
    $highOutput
)
Invoke-CorpusFfmpeg -Arguments $highArgs

Get-FileHash -Algorithm SHA256 -LiteralPath $baselineOutput, $mainOutput, $highOutput |
    Sort-Object Path |
    ForEach-Object { "{0}  {1}" -f $_.Hash.ToLowerInvariant(), (Split-Path -Leaf $_.Path) }
