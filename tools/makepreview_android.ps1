# Builds docs/preview_android.png from emulator screenshots in previews/android (a1_pick_portrait, a2_game_portrait, a3_game_landscape).
# Capture them with: adb exec-out screencap -p > previews/android/<name>.png (see android/tools/emulator_test.md)
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root 'previews\android'
$names = 'a1_pick_portrait', 'a2_game_portrait', 'a3_game_landscape'
$imgs = $names | ForEach-Object { [System.Drawing.Image]::FromFile((Join-Path $dir "$_.png")) }
$H = 900
$pad = 24
$widths = @($imgs | ForEach-Object { [int]($_.Width * $H / $_.Height) })
$canvasW = [int](($widths | Measure-Object -Sum).Sum + $pad * (@($imgs).Count + 1))
$bmp = New-Object System.Drawing.Bitmap $canvasW, ([int]($H + 2 * $pad))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.Clear([System.Drawing.Color]::FromArgb(255, 14, 24, 52))
$x = $pad
for ($i = 0; $i -lt @($imgs).Count; ++$i) {
    $g.DrawImage($imgs[$i], $x, $pad, $widths[$i], $H)
    $x += $widths[$i] + $pad
}
$g.Dispose()
$out = Join-Path $root 'docs\preview_android.png'
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose(); $imgs | ForEach-Object { $_.Dispose() }
Write-Host "wrote $out"
