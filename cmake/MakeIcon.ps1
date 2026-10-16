# 그림 파일(PNG 등) → 여러 크기(16~256)가 든 .ico. mealkit_add_game()이 게임 exe 아이콘을 만들 때 부른다.
#   powershell -NoProfile -ExecutionPolicy Bypass -File cmake\MakeIcon.ps1 -Image icon.png -Out game.ico
# 외부 도구 없이 System.Drawing으로 줄이고, 각 크기를 PNG로 넣는다 (Windows Vista 이후 아이콘 형식, tools/make_editor_icon.py와 같다).
param(
    [Parameter(Mandatory = $true)][string]$Image,
    [Parameter(Mandatory = $true)][string]$Out
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$sizes = 16, 20, 24, 32, 40, 48, 64, 128, 256
try {
    $source = [System.Drawing.Image]::FromFile((Resolve-Path -LiteralPath $Image).Path)
} catch {
    Write-Error "MakeIcon: cannot read image '$Image': $($_.Exception.Message)"
    exit 1
}

$blobs = @()
try {
    foreach ($size in $sizes) {
        $bitmap = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($bitmap)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.Clear([System.Drawing.Color]::Transparent)
        # 정사각형이 아니면 비율을 지켜 가운데에 놓는다
        $scale = [Math]::Min($size / $source.Width, $size / $source.Height)
        $w = [Math]::Max(1, [int][Math]::Round($source.Width * $scale))
        $h = [Math]::Max(1, [int][Math]::Round($source.Height * $scale))
        $attributes = New-Object System.Drawing.Imaging.ImageAttributes
        $attributes.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY) # 가장자리 번짐 방지
        $dest = New-Object System.Drawing.Rectangle([int](($size - $w) / 2), [int](($size - $h) / 2), $w, $h)
        $g.DrawImage($source, $dest, 0, 0, $source.Width, $source.Height, [System.Drawing.GraphicsUnit]::Pixel, $attributes)
        $g.Dispose()
        $stream = New-Object System.IO.MemoryStream
        $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
        $blobs += , $stream.ToArray()
        $stream.Dispose()
    }
} finally {
    $source.Dispose()
}

# ICONDIR(6) + ICONDIRENTRY(16) × n + PNG 데이터
$file = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter($file)
$writer.Write([uint16]0)
$writer.Write([uint16]1)
$writer.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; ++$i) {
    $dim = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] } # 256은 0으로 적는다
    $writer.Write([byte]$dim)
    $writer.Write([byte]$dim)
    $writer.Write([byte]0)     # 팔레트 색 수
    $writer.Write([byte]0)
    $writer.Write([uint16]1)   # 색 평면
    $writer.Write([uint16]32)  # 픽셀당 비트
    $writer.Write([uint32]$blobs[$i].Length)
    $writer.Write([uint32]$offset)
    $offset += $blobs[$i].Length
}
foreach ($blob in $blobs) { $writer.Write($blob) }
$writer.Flush()

$outDir = Split-Path -Parent $Out
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) { New-Item -ItemType Directory -Force $outDir | Out-Null }
[System.IO.File]::WriteAllBytes([System.IO.Path]::GetFullPath($Out), $file.ToArray())
$writer.Dispose()
