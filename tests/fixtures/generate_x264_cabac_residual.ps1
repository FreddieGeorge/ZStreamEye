param(
    [string]$FfmpegPath = "ffmpeg"
)

$fixtureDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$outputPath = Join-Path $fixtureDir "x264_cabac_residual.h264"
$source = "nullsrc=size=128x128:rate=1,geq=lum='128+38*sin((X-N*if(mod(floor(X/32),2),1,0))/19)+31*cos((Y-N*if(mod(floor(Y/32),2),1,0))/23)+22*sin(((X-N*if(mod(floor(X/32),2),1,0))+(Y-N*if(mod(floor(Y/32),2),1,0)))/13)+N*4':cb=128:cr=128,scale=32:32:flags=bilinear"
$x264Params = "cabac=1:bframes=0:keyint=30:min-keyint=30:scenecut=0:ref=1:partitions=p8x8:8x8dct=0:subme=0:me=dia:merange=0:weightp=0:aud=1:repeat-headers=1:no-fast-pskip=1:aq-mode=0"

& $FfmpegPath -hide_banner -loglevel error -y `
    -f lavfi -i $source -frames:v 2 -pix_fmt yuv420p `
    -c:v libx264 -profile:v main -threads 1 -qp 18 -x264-params $x264Params `
    -bsf:v filter_units=remove_types=6 -f h264 $outputPath
if ($LASTEXITCODE -ne 0) {
    throw "FFmpeg failed with exit code $LASTEXITCODE"
}

$expected = "ffcc0f2b72b6b39427652246c2d40d5d1e93b885c9cdbe2e073bac33363ff367"
$actual = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne $expected) {
    throw "Fixture SHA-256 mismatch: expected $expected, got $actual"
}

Write-Host "Generated $outputPath ($actual)"
