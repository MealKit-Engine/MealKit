# Image file (PNG etc.) → .ico with multiple sizes (16~256). Called by mealkit_add_game() to make the game exe icon.
#   powershell -NoProfile -ExecutionPolicy Bypass -File cmake\MakeIcon.ps1 -Image icon.png -Out game.ico
# Scales with System.Drawing without external tools and stores each size as PNG (Windows Vista+ icon format, same as tools/make_editor_icon.py).
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
        # If not square, keep the aspect ratio and center it
        $scale = [Math]::Min($size / $source.Width, $size / $source.Height)
        $w = [Math]::Max(1, [int][Math]::Round($source.Width * $scale))
        $h = [Math]::Max(1, [int][Math]::Round($source.Height * $scale))
        $attributes = New-Object System.Drawing.Imaging.ImageAttributes
        $attributes.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY) # prevent edge bleeding
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

# ICONDIR(6) + ICONDIRENTRY(16) × n + PNG data
$file = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter($file)
$writer.Write([uint16]0)
$writer.Write([uint16]1)
$writer.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; ++$i) {
    $dim = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] } # 256 is written as 0
    $writer.Write([byte]$dim)
    $writer.Write([byte]$dim)
    $writer.Write([byte]0)     # palette color count
    $writer.Write([byte]0)
    $writer.Write([uint16]1)   # color planes
    $writer.Write([uint16]32)  # bits per pixel
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
