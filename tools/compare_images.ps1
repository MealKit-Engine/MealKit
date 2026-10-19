# 두 PNG를 픽셀마다 비교한다 (백엔드 사이 그림 비교, check.ps1 "Vulkan = DX12"). 같은 크기여야 한다.
#   powershell -ExecutionPolicy Bypass -File tools\compare_images.ps1 -A a.png -B b.png [-Threshold 2] [-MaxBadPercent 0.1] [-MaxDiff 16] [-IgnoreBottom 0]
# 채널 차이가 Threshold보다 큰 픽셀이 MaxBadPercent(%)를 넘거나, 가장 큰 차이가 MaxDiff를 넘으면 종료 코드 1.
# IgnoreBottom: 아래쪽 줄 수를 뺀다 (에디터 상태 표시줄의 fps 글자 등 실행마다 다른 곳).
param(
    [Parameter(Mandatory)] [string]$A,
    [Parameter(Mandatory)] [string]$B,
    [int]$Threshold = 2,
    [double]$MaxBadPercent = 0.1,
    [int]$MaxDiff = 16,
    [int]$IgnoreBottom = 0
)
$ErrorActionPreference = "Stop"
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class MealKitImageDiff {
    // 돌려주는 값: [가장 큰 채널 차이, 문턱을 넘은 픽셀 수, 비교한 픽셀 수]
    public static long[] Compare(string a, string b, int threshold, int ignoreBottom) {
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
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    int o = y * da.Stride + x * 4, d = 0;
                    for (int c = 0; c < 3; ++c) d = Math.Max(d, Math.Abs(pa[o + c] - pb[o + c]));
                    if (d > max) max = d;
                    if (d > threshold) ++bad;
                }
            }
            return new long[] { max, bad, (long)w * h };
        }
    }
}
"@
$r = [MealKitImageDiff]::Compare((Resolve-Path $A).Path, (Resolve-Path $B).Path, $Threshold, $IgnoreBottom)
$percent = if ($r[2] -gt 0) { 100.0 * $r[1] / $r[2] } else { 0 }
$ok = $r[0] -le $MaxDiff -and $percent -le $MaxBadPercent
Write-Output ("max channel diff {0}, {1} of {2} pixels over {3} ({4:N3}%) - limits: max {5}, {6}%" -f $r[0], $r[1], $r[2], $Threshold, $percent, $MaxDiff, $MaxBadPercent)
Write-Output $(if ($ok) { "SAME" } else { "DIFFERENT" })
if (-not $ok) { exit 1 }
exit 0
