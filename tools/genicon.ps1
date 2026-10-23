# Makes the 48x48 HOME menu icon (assets/icon.png) from assets/reference/icon_source.png (artwork on a black background -> transparent).
# Run: powershell -ExecutionPolicy Bypass -File tools/genicon.ps1
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path "$PSScriptRoot\.."
. "$PSScriptRoot\keyblack.ps1"
$src = Load-KeyedBitmap "$root\assets\reference\icon_source.png"
$dst = New-Object System.Drawing.Bitmap 48, 48, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'Half'; $g.SmoothingMode = 'HighQuality'
$g.DrawImage($src, 0, 0, 48, 48); $g.Dispose()
$dst.Save("$root\assets\icon.png")
Write-Host "wrote assets/icon.png"
