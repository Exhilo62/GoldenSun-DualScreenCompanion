# Makes the 48x48 HOME menu icon (assets/icon.png) from assets/reference/icon_source.png (artwork on a black background -> transparent),
# placed on a navy blue gradient tile like the app's own panels.
# Run: powershell -ExecutionPolicy Bypass -File tools/genicon.ps1
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path "$PSScriptRoot\.."
. "$PSScriptRoot\keyblack.ps1"
$src = Load-KeyedBitmap "$root\assets\reference\icon_source.png"
$dst = New-Object System.Drawing.Bitmap 48, 48, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'Half'; $g.SmoothingMode = 'HighQuality'
$bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush ((New-Object System.Drawing.Rectangle 0, 0, 48, 48), [System.Drawing.Color]::FromArgb(255, 24, 46, 104), [System.Drawing.Color]::FromArgb(255, 8, 16, 46), 90.0)
$g.FillRectangle($bg, 0, 0, 48, 48)
$g.DrawImage($src, 2, 2, 44, 44)   # a small margin so the sunburst points are not cut by the rounded HOME tile
$g.Dispose()
$dst.Save("$root\assets\icon.png")
Write-Host "wrote assets/icon.png"
