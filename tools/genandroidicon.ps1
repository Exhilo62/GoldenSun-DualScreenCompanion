# Makes the Android launcher icons from the same art as the 3DS HOME icon (tools/genicon.ps1): the artwork (black keyed out) on the navy blue gradient tile.
#  - mipmap-<density>/ic_launcher.png: the finished tile, for Android 7 and older
#  - mipmap-<density>/ic_launcher_foreground.png + drawable/ic_launcher_background.xml + mipmap-anydpi-v26/ic_launcher.xml: the adaptive icon (Android 8+),
#    where the launcher supplies the tile shape; the artwork sits in the central safe zone
# Run: powershell -ExecutionPolicy Bypass -File tools/genandroidicon.ps1
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot\keyblack.ps1"
$src = Load-KeyedBitmap (Join-Path $root 'assets\reference\icon_source.png')
$res = Join-Path $root 'android\app\src\main\res'
$navyTop = [System.Drawing.Color]::FromArgb(255, 24, 46, 104)
$navyBottom = [System.Drawing.Color]::FromArgb(255, 8, 16, 46)

function New-Canvas([int]$n) {
    $bmp = New-Object System.Drawing.Bitmap $n, $n, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    return @($bmp, $g)
}

# legacy icons: 48 dp tile, art with a 2/48 margin like the 3DS icon
$legacy = @{ 'mdpi' = 48; 'hdpi' = 72; 'xhdpi' = 96; 'xxhdpi' = 144; 'xxxhdpi' = 192 }
foreach ($k in $legacy.Keys) {
    $n = $legacy[$k]
    $bmp, $g = New-Canvas $n
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush ((New-Object System.Drawing.Rectangle 0, 0, $n, $n), $navyTop, $navyBottom, 90.0)
    $g.FillRectangle($brush, 0, 0, $n, $n)
    $m = [int][Math]::Round($n * 2 / 48)
    $g.DrawImage($src, $m, $m, $n - 2 * $m, $n - 2 * $m)
    $g.Dispose()
    $dir = Join-Path $res "mipmap-$k"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $bmp.Save((Join-Path $dir 'ic_launcher.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

# adaptive foreground: 108 dp canvas, transparent, artwork in the middle 62%
$fore = @{ 'mdpi' = 108; 'hdpi' = 162; 'xhdpi' = 216; 'xxhdpi' = 324; 'xxxhdpi' = 432 }
foreach ($k in $fore.Keys) {
    $n = $fore[$k]
    $bmp, $g = New-Canvas $n
    $a = [int][Math]::Round($n * 0.62)
    $o = [int][Math]::Round(($n - $a) / 2)
    $g.DrawImage($src, $o, $o, $a, $a)
    $g.Dispose()
    $bmp.Save((Join-Path (Join-Path $res "mipmap-$k") 'ic_launcher_foreground.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

New-Item -ItemType Directory -Force (Join-Path $res 'drawable'), (Join-Path $res 'mipmap-anydpi-v26') | Out-Null
$utf8 = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText((Join-Path $res 'drawable\ic_launcher_background.xml'), @'
<?xml version="1.0" encoding="utf-8"?>
<shape xmlns:android="http://schemas.android.com/apk/res/android" android:shape="rectangle">
    <gradient android:angle="270" android:startColor="#182E68" android:endColor="#08102E" />
</shape>
'@, $utf8)
[System.IO.File]::WriteAllText((Join-Path $res 'mipmap-anydpi-v26\ic_launcher.xml'), @'
<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@drawable/ic_launcher_background" />
    <foreground android:drawable="@mipmap/ic_launcher_foreground" />
</adaptive-icon>
'@, $utf8)
$src.Dispose()
Write-Host 'wrote launcher icons (legacy + adaptive)'
