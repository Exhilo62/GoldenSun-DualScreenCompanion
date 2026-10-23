# Automated Azahar regression checks for features that the headless self-test cannot see. Each scenario drives tools/azahar_smoke.ps1 and
# inspects the screenshot pixels. Needs build\app\GoldenSun3DS.3dsx and the state files in build\ (taf_2575.state, ta_title.state).
# usage: powershell -ExecutionPolicy Bypass -File tools\azahar_regress.ps1 [-Only flip|equip|ff]
param([string]$Only = "")
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path "$PSScriptRoot\.."
$smoke = "$root\tools\azahar_smoke.ps1"
$fails = 0
function Count-Pixels($png, $x0, $y0, $x1, $y1, [scriptblock]$test) {
    $bmp = New-Object System.Drawing.Bitmap $png; $n = 0
    for ($y = $y0; $y -lt $y1; ++$y) { for ($x = $x0; $x -lt $x1; ++$x) { $c = $bmp.GetPixel($x, $y); if (& $test $c) { ++$n } } }
    $bmp.Dispose(); return $n
}
function Check($name, $ok, $detail) {
    if ($ok) { Write-Output "ok    $name ($detail)" } else { Write-Output "FAIL  $name ($detail)"; $script:fails++ }
}
function Run-Smoke([string[]]$extra) { & powershell -NoProfile -ExecutionPolicy Bypass -File $smoke @extra | Out-Null }
$red = { param($c) $c.R -gt 200 -and $c.G -lt 110 -and $c.B -lt 110 }
$gold = { param($c) $c.R -gt 200 -and $c.G -gt 140 -and $c.B -lt 90 }
$white = { param($c) $c.R -gt 215 -and $c.G -gt 215 -and $c.B -gt 215 }

if (-not $Only -or $Only -eq 'flip') {   # Lost Age: tap a Djinn, it flips to Standby (its name turns red)
    Run-Smoke @('-Game', '2', '-State', "$root\build\taf_2575.state", '-Tab', '2', '-WaitSeconds', '1', '-Close', '-Out', "$root\build\rg_flip0.png")
    Run-Smoke @('-Game', '2', '-State', "$root\build\taf_2575.state", '-Tab', '2', '-WaitSeconds', '1', '-Clicks', '77,98,25', '-Close', '-Out', "$root\build\rg_flip.png")
    $before = Count-Pixels "$root\build\rg_flip0.png" 350 930 440 955 $red
    $after = Count-Pixels "$root\build\rg_flip_c1.png" 350 930 440 955 $red
    Check 'tla djinn flip (Steel changes colour)' ([math]::Abs($after - $before) -gt 30) "red pixels $before -> $after"
}
if (-not $Only -or $Only -eq 'equip') {   # Lost Age: tap Piers's Lightning Sword, the gold marker appears on its row
    Run-Smoke @('-Game', '2', '-State', "$root\build\taf_2575.state", '-Tab', '4', '-WaitSeconds', '1', '-Clicks', '272,65,1;79,85,10;90,189,16', '-Close', '-Out', "$root\build\rg_equip.png")
    $before = Count-Pixels "$root\build\rg_equip_c1.png" 230 895 250 925 $gold
    $after = Count-Pixels "$root\build\rg_equip_c3.png" 230 895 250 925 $gold
    Check 'tla single equip' ($before -lt 5 -and $after -gt 20) "marker pixels $before -> $after"
}
if ($Only -eq 'sets') {   # Lost Age Djinn sets (slow, about five minutes): save the layout, All Standby, apply the saved set; the red (Standby) count must return
    Run-Smoke @('-Game', '2', '-State', "$root\build\taf_2575.state", '-Tab', '2', '-WaitSeconds', '1', '-Clicks', '131,63,2;223,124,3;285,64,2;131,63,2;235,98,100;131,63,2;282,124,100', '-Close', '-Out', "$root\build\rg_sets.png")
    $base = Count-Pixels "$root\build\rg_sets_c3.png" 215 830 985 1210 $red
    $all = Count-Pixels "$root\build\rg_sets_c5.png" 215 830 985 1210 $red
    $back = Count-Pixels "$root\build\rg_sets_c7.png" 215 830 985 1210 $red
    Check 'tla djinn sets round trip' ($all -gt $base * 2 -and [math]::Abs($back - $base) -lt 50) "standby pixels $base -> $all -> $back"
}
if ($fails) { Write-Output "$fails check(s) failed"; exit 1 } else { Write-Output "all Azahar checks passed" }
