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
# Windows uses System.Drawing; Linux/macOS use a portable PNG reader (same results, same output image)
$onWindows = ($PSVersionTable.PSEdition -eq "Desktop") -or $IsWindows
if ($onWindows) {
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
} else {
Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.IO.Compression;
public static class MealKitImageDiff {
    // Portable PNG reader/writer (8-bit RGB or RGBA, not interlaced: what MkRender and the editor write), no System.Drawing
    static uint[] crcTable;
    static uint Crc(byte[] d, int o, int n) {
        if (crcTable == null) {
            var t = new uint[256];
            for (uint i = 0; i < 256; ++i) { uint c = i; for (int k = 0; k < 8; ++k) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1; t[i] = c; }
            crcTable = t;
        }
        uint r = 0xFFFFFFFFu;
        for (int i = 0; i < n; ++i) r = crcTable[(r ^ d[o + i]) & 0xFF] ^ (r >> 8);
        return r ^ 0xFFFFFFFFu;
    }
    static int Be(byte[] d, int o) { return (d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]; }
    // Returns RGBA bytes, width * height * 4
    static byte[] Read(string path, out int w, out int h) {
        var f = File.ReadAllBytes(path);
        if (f.Length < 33 || f[0] != 0x89 || f[1] != 'P') throw new Exception("not a PNG: " + path);
        w = 0; h = 0; int type = 0;
        var idat = new MemoryStream();
        for (int o = 8; o + 8 <= f.Length; ) {
            int len = Be(f, o); string name = System.Text.Encoding.ASCII.GetString(f, o + 4, 4);
            if (name == "IHDR") {
                w = Be(f, o + 8); h = Be(f, o + 12); type = f[o + 17];
                if (f[o + 16] != 8 || (type != 2 && type != 6) || f[o + 20] != 0) throw new Exception("unsupported PNG (need 8-bit RGB/RGBA, not interlaced): " + path);
            } else if (name == "IDAT") idat.Write(f, o + 8, len);
            else if (name == "IEND") break;
            o += 12 + len;
        }
        int bpp = type == 6 ? 4 : 3, stride = w * bpp;
        var raw = new byte[(stride + 1) * h];
        idat.Position = 0;
        using (var z = new ZLibStream(idat, CompressionMode.Decompress)) {
            int got = 0, n;
            while (got < raw.Length && (n = z.Read(raw, got, raw.Length - got)) > 0) got += n;
            if (got != raw.Length) throw new Exception("PNG data too short: " + path);
        }
        var px = new byte[w * h * 4];
        var prev = new byte[stride]; var cur = new byte[stride];
        for (int y = 0; y < h; ++y) {
            int ft = raw[y * (stride + 1)];
            Buffer.BlockCopy(raw, y * (stride + 1) + 1, cur, 0, stride);
            for (int i = 0; i < stride; ++i) {
                int a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0, v = cur[i];
                switch (ft) {
                    case 1: v += a; break;
                    case 2: v += b; break;
                    case 3: v += (a + b) / 2; break;
                    case 4: { int p = a + b - c, pa = Math.Abs(p - a), pb = Math.Abs(p - b), pc = Math.Abs(p - c); v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c); break; }
                    case 0: break;
                    default: throw new Exception("bad PNG filter: " + path);
                }
                cur[i] = (byte)v;
            }
            for (int x = 0; x < w; ++x) {
                int o = (y * w + x) * 4;
                px[o] = cur[x * bpp]; px[o + 1] = cur[x * bpp + 1]; px[o + 2] = cur[x * bpp + 2]; px[o + 3] = bpp == 4 ? cur[x * bpp + 3] : (byte)255;
            }
            var t = prev; prev = cur; cur = t;
        }
        return px;
    }
    static void Chunk(Stream s, string name, byte[] data, int len) {
        var b = new byte[len + 8];
        b[0] = (byte)(len >> 24); b[1] = (byte)(len >> 16); b[2] = (byte)(len >> 8); b[3] = (byte)len;
        for (int i = 0; i < 4; ++i) b[4 + i] = (byte)name[i];
        Buffer.BlockCopy(data, 0, b, 8, len);
        s.Write(b, 0, b.Length);
        uint c = Crc(b, 4, len + 4);
        s.Write(new byte[] { (byte)(c >> 24), (byte)(c >> 16), (byte)(c >> 8), (byte)c }, 0, 4);
    }
    static void Write(string path, byte[] rgba, int w, int h) {
        using (var s = File.Create(path)) {
            s.Write(new byte[] { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A }, 0, 8);
            var ihdr = new byte[13];
            ihdr[0] = (byte)(w >> 24); ihdr[1] = (byte)(w >> 16); ihdr[2] = (byte)(w >> 8); ihdr[3] = (byte)w;
            ihdr[4] = (byte)(h >> 24); ihdr[5] = (byte)(h >> 16); ihdr[6] = (byte)(h >> 8); ihdr[7] = (byte)h;
            ihdr[8] = 8; ihdr[9] = 6;
            Chunk(s, "IHDR", ihdr, 13);
            var ms = new MemoryStream();
            using (var z = new ZLibStream(ms, CompressionLevel.Fastest, true)) {
                var zero = new byte[1];
                for (int y = 0; y < h; ++y) { z.Write(zero, 0, 1); z.Write(rgba, y * w * 4, w * 4); }
            }
            var d = ms.ToArray();
            Chunk(s, "IDAT", d, d.Length);
            Chunk(s, "IEND", new byte[0], 0);
        }
    }
    // Returns: [largest channel difference, number of pixels over the threshold, number of pixels compared]
    public static long[] Compare(string a, string b, int threshold, int ignoreBottom, string diffOut) {
        int wa, ha, wb, hb;
        var pa = Read(a, out wa, out ha); var pb = Read(b, out wb, out hb);
        if (wa != wb || ha != hb) throw new Exception("size differs: " + wa + "x" + ha + " vs " + wb + "x" + hb);
        int w = wa, h = Math.Max(0, ha - ignoreBottom);
        long max = 0, bad = 0;
        var diff = new byte[w * h * 4];
        for (int i = 0; i < w * h; ++i) {
            int o = i * 4, d = 0;
            for (int c = 0; c < 3; ++c) d = Math.Max(d, Math.Abs(pa[o + c] - pb[o + c]));
            if (d > max) max = d;
            if (d > threshold) { ++bad; diff[o] = 255; diff[o + 1] = 0; diff[o + 2] = 0; }
            else for (int c = 0; c < 3; ++c) diff[o + c] = (byte)(pa[o + c] / 4);
            diff[o + 3] = 255;
        }
        if (bad > 0 && diffOut.Length > 0) Write(diffOut, diff, w, h);
        return new long[] { max, bad, (long)w * h };
    }
}
"@
}
$diffPath = if ($DiffOut) { [IO.Path]::GetFullPath($DiffOut) } else { "" }
$r = [MealKitImageDiff]::Compare((Resolve-Path $A).Path, (Resolve-Path $B).Path, $Threshold, $IgnoreBottom, $diffPath)
$percent = if ($r[2] -gt 0) { 100.0 * $r[1] / $r[2] } else { 0 }
$ok = $r[0] -le $MaxDiff -and $percent -le $MaxBadPercent
Write-Output ("max channel diff {0}, {1} of {2} pixels over {3} ({4:N3}%) - limits: max {5}, {6}%" -f $r[0], $r[1], $r[2], $Threshold, $percent, $MaxDiff, $MaxBadPercent)
Write-Output $(if ($ok) { "SAME" } else { "DIFFERENT" })
if (-not $ok) { exit 1 }
exit 0
