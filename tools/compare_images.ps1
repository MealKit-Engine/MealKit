# Compares two PNGs pixel by pixel (image comparison between backends, check.ps1 "Vulkan = DX12"). They must be the same size.
#   powershell -ExecutionPolicy Bypass -File tools\compare_images.ps1 -A a.png -B b.png [-Threshold 2] [-MaxBadPercent 0.1] [-MaxDiff 16] [-IgnoreBottom 0] [-DiffOut d.png]
# Exit code 1 if pixels with channel difference over Threshold exceed MaxBadPercent (%), or the largest difference exceeds MaxDiff.
# IgnoreBottom: excludes that many bottom rows (places that differ per run, like the fps text in the editor status bar).
# DiffOut: writes a difference image if they differ (A darkened, pixels over the threshold in red. Graphics regression tests).
param(
    [Parameter(Mandatory)] [string]$A,
    [Parameter(Mandatory)] [string]$B,
    [int]$Threshold = 2,
    [double]$MaxBadPercent = 0.1,
    [int]$MaxDiff = 16,
    [int]$IgnoreBottom = 0,
    [string]$DiffOut = ""
)
$ErrorActionPreference = "Stop"
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class MealKitImageDiff {
    // Returns: [largest channel difference, number of pixels over the threshold, number of pixels compared]
    public static long[] Compare(string a, string b, int threshold, int ignoreBottom, string diffOut) {
        using (var ia = new Bitmap(a)) using (var ib = new Bitmap(b)) {
            if (ia.Width != ib.Width || ia.Height != ib.Height)
                throw new Exception("size differs: " + ia.Width + "x" + ia.Height + " vs " + ib.Width + "x" + ib.Height);
            int w = ia.Width, h = Math.Max(0, ia.Height - ignoreBottom);
            var rect = new Rectangle(0, 0, w, h);
            var da = ia.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var db = ib.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            var pa = new byte[da.Stride * h];
            var pb = new byte[db.Stride * h];
            Marshal.Copy(da.Scan0, pa, 0, pa.Length);
            Marshal.Copy(db.Scan0, pb, 0, pb.Length);
            ia.UnlockBits(da);
            ib.UnlockBits(db);
            long max = 0, bad = 0;
            var diff = new byte[pa.Length];
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    int o = y * da.Stride + x * 4, d = 0;
                    for (int c = 0; c < 3; ++c) d = Math.Max(d, Math.Abs(pa[o + c] - pb[o + c]));
                    if (d > max) max = d;
                    if (d > threshold) ++bad;
                    // Diff image (BGRA): differing pixels are red, the rest at 1/4 brightness of A
                    if (d > threshold) { diff[o] = 0; diff[o + 1] = 0; diff[o + 2] = 255; }
                    else for (int c = 0; c < 3; ++c) diff[o + c] = (byte)(pa[o + c] / 4);
                    diff[o + 3] = 255;
                }
            }
            if (bad > 0 && diffOut.Length > 0) {
                using (var od = new Bitmap(w, h, PixelFormat.Format32bppArgb)) {
                    var dd = od.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
                    Marshal.Copy(diff, 0, dd.Scan0, diff.Length);
                    od.UnlockBits(dd);
                    od.Save(diffOut, ImageFormat.Png);
                }
            }
            return new long[] { max, bad, (long)w * h };
        }
    }
}
"@
$diffPath = if ($DiffOut) { [IO.Path]::GetFullPath($DiffOut) } else { "" }
$r = [MealKitImageDiff]::Compare((Resolve-Path $A).Path, (Resolve-Path $B).Path, $Threshold, $IgnoreBottom, $diffPath)
$percent = if ($r[2] -gt 0) { 100.0 * $r[1] / $r[2] } else { 0 }
$ok = $r[0] -le $MaxDiff -and $percent -le $MaxBadPercent
Write-Output ("max channel diff {0}, {1} of {2} pixels over {3} ({4:N3}%) - limits: max {5}, {6}%" -f $r[0], $r[1], $r[2], $Threshold, $percent, $MaxDiff, $MaxBadPercent)
Write-Output $(if ($ok) { "SAME" } else { "DIFFERENT" })
if (-not $ok) { exit 1 }
exit 0
