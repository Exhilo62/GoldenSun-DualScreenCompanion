# Builds the preview collage (previews\collage.png, and a 1600 px wide copy) from screenshots in the previews folder.
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path "$PSScriptRoot\.."
$dir = "$root\previews"
# row, file, caption
$rows = @(
  @(@('Screenshot 2026-10-24 202140.png', 'Choose your game'), @('Screenshot 2026-10-24 202328.png', 'Dialogue with the party below'), @('Screenshot 2026-10-24 202745.png', 'Menus mirrored on the touch screen')),
  @(@('Screenshot 2026-10-24 202653.png', 'Party'), @('Screenshot 2026-10-24 202515.png', 'Psynergy: tap to cast'), @('Screenshot 2026-10-24 202552.png', 'Djinn and saved sets')),
  @(@('Screenshot 2026-10-24 202618.png', 'Items: equip with preview'), @('Screenshot 2026-10-24 202432.png', 'Whole-area map'), @('Screenshot 2026-10-24 202405.png', 'Save slots'))
)
$CW = 2400; $MAR = 40; $GAP = 24; $CAPH = 44; $HEAD = 170
$imgs = @(); foreach ($r in $rows) { $row = @(); foreach ($c in $r) { $row += , @([System.Drawing.Image]::FromFile("$dir\$($c[0])"), $c[1]) }; $imgs += , $row }
# each row is scaled so that it fills the width
$heights = @(); foreach ($row in $imgs) { $ar = 0.0; foreach ($c in $row) { $ar += $c[0].Width / $c[0].Height }; $heights += (($CW - 2 * $MAR - ($row.Count - 1) * $GAP) / $ar) }
$CH = [int]($HEAD + ($heights | Measure-Object -Sum).Sum + ($GAP + $CAPH) * ($rows.Count) + $MAR)
$bmp = New-Object System.Drawing.Bitmap $CW, $CH
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = 'HighQualityBicubic'; $g.SmoothingMode = 'HighQuality'; $g.TextRenderingHint = 'AntiAliasGridFit'; $g.PixelOffsetMode = 'HighQuality'
$bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush ((New-Object System.Drawing.Rectangle 0, 0, $CW, $CH), [System.Drawing.Color]::FromArgb(12, 24, 58), [System.Drawing.Color]::FromArgb(6, 12, 30), 90)
$g.FillRectangle($bg, 0, 0, $CW, $CH)
$gold = [System.Drawing.Color]::FromArgb(238, 190, 72)
$f1 = New-Object System.Drawing.Font 'Georgia', 62, ([System.Drawing.FontStyle]::Bold), 'Pixel'
$f2 = New-Object System.Drawing.Font 'Segoe UI', 24, ([System.Drawing.FontStyle]::Regular), 'Pixel'
$sf = New-Object System.Drawing.StringFormat; $sf.Alignment = 'Center'
$g.DrawString('Golden Sun - Dual Screen Companion', $f1, (New-Object System.Drawing.SolidBrush $gold), ($CW / 2), 22, $sf)
$g.DrawString('Golden Sun and The Lost Age on New 3DS, with a touch companion on the bottom screen   |   v0.1.0 beta   |   unofficial fan project, bring your own ROMs', $f2, [System.Drawing.Brushes]::LightSteelBlue, ($CW / 2), 112, $sf)
$pen = New-Object System.Drawing.Pen $gold, 3
$cap = New-Object System.Drawing.Font 'Segoe UI', 20, ([System.Drawing.FontStyle]::Bold), 'Pixel'
$y = $HEAD
for ($ri = 0; $ri -lt $imgs.Count; ++$ri) {
    $x = $MAR; $rh = [int]$heights[$ri]
    foreach ($c in $imgs[$ri]) {
        $iw = [int][Math]::Round($c[0].Width * $rh / $c[0].Height)
        $g.DrawImage($c[0], $x, $y, $iw, $rh)
        $g.DrawRectangle($pen, $x, $y, $iw, $rh)
        $csf = New-Object System.Drawing.StringFormat; $csf.Alignment = 'Center'
        $g.DrawString($c[1], $cap, (New-Object System.Drawing.SolidBrush $gold), ($x + $iw / 2), ($y + $rh + 8), $csf)
        $x += $iw + $GAP
    }
    $y += $rh + $CAPH + $GAP
}
$g.Dispose()
$out = "$dir\collage.png"
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$sw = 1600; $sh = [int]($CH * $sw / $CW)
$small = New-Object System.Drawing.Bitmap $sw, $sh
$g2 = [System.Drawing.Graphics]::FromImage($small); $g2.InterpolationMode = 'HighQualityBicubic'; $g2.DrawImage($bmp, 0, 0, $sw, $sh); $g2.Dispose()
$small.Save("$dir\collage_1600.png", [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "collage ${CW}x${CH}, small ${sw}x${sh}"
