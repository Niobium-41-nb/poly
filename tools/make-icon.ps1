# Generates assets/icon.ico (multi-size PNG-based ICO) for poly.exe / poly-gui.exe
# and the installer. ASCII-only: PowerShell 5.1 reads .ps1 as ANSI.
#
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools/make-icon.ps1
param(
    [string]$Out = "assets/icon.ico"
)

Add-Type -AssemblyName System.Drawing

function New-PolyBitmap([int]$size) {
    $bmp = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic

    if ($size -ge 48) {
        # rounded square with a vertical blue gradient
        $pad = [double]$size * 0.055
        $side = [double]$size - 2 * $pad
        $rect = [System.Drawing.RectangleF]::new([single]$pad, [single]$pad, [single]$side, [single]$side)
        $radius = [double]$size * 0.22
        $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
        $d = $radius * 2
        $path.AddArc($rect.X, $rect.Y, [single]$d, [single]$d, 180, 90)
        $path.AddArc($rect.Right - $d, $rect.Y, [single]$d, [single]$d, 270, 90)
        $path.AddArc($rect.Right - $d, $rect.Bottom - $d, [single]$d, [single]$d, 0, 90)
        $path.AddArc($rect.X, $rect.Bottom - $d, [single]$d, [single]$d, 90, 90)
        $path.CloseFigure()
        $brush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
            [System.Drawing.PointF]::new($rect.X, $rect.Y),
            [System.Drawing.PointF]::new($rect.X, $rect.Bottom),
            [System.Drawing.Color]::FromArgb(255, 0x3d, 0x7e, 0xd8),
            [System.Drawing.Color]::FromArgb(255, 0x1c, 0x3f, 0x8c))
        $g.FillPath($brush, $path)
        $brush.Dispose()
        $path.Dispose()
    } else {
        # tiny sizes: flat fill reads better than a gradient
        $brush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 0x2c, 0x5f, 0xb4))
        $g.FillRectangle($brush, 0, 0, $size, $size)
        $brush.Dispose()
    }

    # letter "P" (readable even at 16px)
    $font = [System.Drawing.Font]::new("Segoe UI", [single]([double]$size * 0.62),
        [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $white = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::White)
    $fmt = [System.Drawing.StringFormat]::new()
    $fmt.Alignment = [System.Drawing.StringAlignment]::Center
    $fmt.LineAlignment = [System.Drawing.StringAlignment]::Center
    $dy = -[double]$size * 0.02
    $g.DrawString("P", $font, $white,
        [System.Drawing.RectangleF]::new(0, [single]$dy, [single]$size, [single]$size), $fmt)

    # three "test case" dots under the letter
    if ($size -ge 32) {
        $r = [double]$size * 0.045
        $cy = [double]$size * 0.775
        $gap = [double]$size * 0.135
        $cx = [double]$size / 2 - $gap
        for ($i = 0; $i -lt 3; $i++) {
            $g.FillEllipse($white, [single]($cx + $i * $gap - $r), [single]($cy - $r),
                [single](2 * $r), [single](2 * $r))
        }
    }

    $white.Dispose(); $font.Dispose(); $fmt.Dispose(); $g.Dispose()
    return $bmp
}

# 传统 DIB 帧（BITMAPINFOHEADER + 32bpp XOR 位图 + 空 AND 掩码）。
# 只有 256 尺寸用 PNG 帧：部分旧 API（包括 .NET 的 Icon 类）读不了 PNG 帧。
function Get-DibFrame([System.Drawing.Bitmap]$bmp) {
    $w = $bmp.Width
    $h = $bmp.Height
    $ms = [System.IO.MemoryStream]::new()
    $bw = [System.IO.BinaryWriter]::new($ms)
    $bw.Write([UInt32]40)                     # biSize
    $bw.Write([Int32]$w)                      # biWidth
    $bw.Write([Int32]($h * 2))                # biHeight: XOR + AND
    $bw.Write([UInt16]1)                      # biPlanes
    $bw.Write([UInt16]32)                     # biBitCount
    $bw.Write([UInt32]0)                      # biCompression = BI_RGB
    $bw.Write([UInt32]($w * $h * 4))          # biSizeImage
    $bw.Write([Int32]0); $bw.Write([Int32]0)
    $bw.Write([UInt32]0); $bw.Write([UInt32]0)

    $rect = [System.Drawing.Rectangle]::new(0, 0, $w, $h)
    $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                          [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stride = $data.Stride
    $row = New-Object byte[] $stride
    for ($y = $h - 1; $y -ge 0; $y--) {       # bottom-up
        [System.Runtime.InteropServices.Marshal]::Copy(
            [IntPtr]::Add($data.Scan0, $y * $stride), $row, 0, $stride)
        $bw.Write($row)
    }
    $bmp.UnlockBits($data)

    $maskRow = New-Object byte[] ([int]([math]::Floor(($w + 31) / 32) * 4))
    for ($y = 0; $y -lt $h; $y++) { $bw.Write($maskRow) }

    $bw.Flush()
    $bytes = $ms.ToArray()
    $bw.Dispose(); $ms.Dispose()
    return , $bytes
}

# 256 is drawn first, smaller sizes are downscaled from it for consistent shapes
$master = New-PolyBitmap 256
$sizes = @(256, 128, 64, 48, 32, 16)
$frames = @()
foreach ($s in $sizes) {
    if ($s -eq 256) {
        $bmp = $master
    } else {
        $bmp = [System.Drawing.Bitmap]::new($s, $s, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.DrawImage($master, 0, 0, $s, $s)
        $g.Dispose()
    }
    if ($s -ge 256) {
        $ms = [System.IO.MemoryStream]::new()
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $payload = [byte[]]$ms.ToArray()
        $ms.Dispose()
    } else {
        $payload = [byte[]](Get-DibFrame $bmp)
    }
    $frames += , @{ size = $s; bytes = $payload }
    if ($s -ne 256) { $bmp.Dispose() }
}
$master.Dispose()

# ICONDIR + ICONDIRENTRY[] + PNG payloads
$outStream = [System.IO.MemoryStream]::new()
$bw = [System.IO.BinaryWriter]::new($outStream)
$bw.Write([UInt16]0)                 # reserved
$bw.Write([UInt16]1)                 # type: icon
$bw.Write([UInt16]$frames.Count)
$offset = 6 + 16 * $frames.Count
foreach ($f in $frames) {
    $payload = [byte[]]$f.bytes
    $dim = if ($f.size -ge 256) { 0 } else { $f.size }
    $bw.Write([Byte]$dim)            # width (0 = 256)
    $bw.Write([Byte]$dim)            # height
    $bw.Write([Byte]0)               # palette
    $bw.Write([Byte]0)               # reserved
    $bw.Write([UInt16]1)             # color planes
    $bw.Write([UInt16]32)            # bits per pixel
    $bw.Write([UInt32]$payload.Length)
    $bw.Write([UInt32]$offset)
    $offset += $payload.Length
}
foreach ($f in $frames) { $bw.Write([byte[]]$f.bytes) }
$bw.Flush()

$dir = Split-Path -Parent $Out
if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null }
[System.IO.File]::WriteAllBytes((Resolve-Path -LiteralPath (Split-Path -Parent $Out)).Path + "\" +
    (Split-Path -Leaf $Out), $outStream.ToArray())
$bw.Dispose(); $outStream.Dispose()
Write-Host ("wrote {0} ({1} bytes, {2} sizes)" -f $Out, (Get-Item $Out).Length, $frames.Count)
