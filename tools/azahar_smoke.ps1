# Launches the .3dsx in Azahar, optionally clicks MENU > LOAD STATE (state.ss must exist on the virtual SD),
# and saves a window screenshot. usage: powershell -File tools/azahar_smoke.ps1 [-LoadState] [-Out shot.png]
param([switch]$LoadState, [switch]$Pick, [int]$Game = 0, [string]$State = "", [switch]$Close, [string]$App = "", [int]$Tab = 0, [int]$WaitSeconds = 0, [string]$Keys = "", [string]$Clicks = "", [string]$Drags = "", [string]$PreClicks = "", [string]$Out = "$PSScriptRoot\..\build\azahar_smoke.png")
# -Game 1|2    pick Golden Sun (1) or The Lost Age (2) at the game picker (writes lastgame.txt, presses A); no need to move ROMs around.
# -State f.state  copy that emulator state into the chosen game's save slot 1 (state.ss) and load it; the previous slot-1 file and lastgame.txt are
#                 restored afterwards, so your own saves are never overwritten. Implies -LoadState.
# -PreClicks "x,y,wait;..."  taps (bottom-screen coordinates) done after loading but BEFORE -Keys, e.g. to switch fast forward on first.
# -Clicks "x,y,wait[,holdMs]": the optional 4th field holds the touch (e.g. 1500 for a long press).
# -Keys also take A, B, Y (X key), LS, RS (Q / W) for the GBA buttons.
# -Drags "x1,y1,x2,y2,wait;..."  press-slide-release in bottom-screen coordinates (scroll lists); screenshots <Out base>_d1.png ...
# -Close         stop Azahar when finished.
# -Tab N         tab to open after loading (0 Party .. 5 Menu); -1 leaves whatever the app chose (the battle panel opens by itself).
# -Keys "U:600,R:300;D:900": semicolon-separated groups of arrow-key holds (U/D/L/R:milliseconds). A screenshot is saved
# after each group as <Out base>_g1.png, _g2.png ... Every key press re-checks that Azahar is the foreground window.

$app = if ($App) { (Resolve-Path $App).Path } else { Resolve-Path "$PSScriptRoot\..\build\app\GoldenSun3DS.3dsx" }   # -App x.cia boots a CIA instead of the .3dsx
$sd = Join-Path $env:APPDATA 'Azahar\sdmc\3ds\GoldenSun3DS'
$undo = @()   # (path, backup) pairs restored in the finally block
function Backup-File($path) {
    if (Test-Path $path) { Copy-Item $path "$path.smokebak" -Force; $script:undo += ,@($path, "$path.smokebak") } else { $script:undo += ,@($path, $null) }
}
if ($Game) { Backup-File "$sd\lastgame.txt"; Set-Content -Path "$sd\lastgame.txt" -Value ($Game - 1) -Encoding ASCII; $Pick = $true }
if ($State) {
    $dest = if ($Game -eq 2) { "$sd\lost_age\state.ss" } else { "$sd\state.ss" }
    $thb = [System.IO.Path]::ChangeExtension($dest, '.thb')
    Backup-File $dest; Backup-File $thb
    New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
    Copy-Item (Resolve-Path $State) $dest -Force
    Remove-Item $thb -Force -ErrorAction SilentlyContinue
    $LoadState = $true
}
try {
Get-Process azahar -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2
Start-Process "C:\Program Files\Azahar\azahar.exe" -ArgumentList "`"$app`""
Start-Sleep 14

Add-Type @"
using System; using System.Runtime.InteropServices;
public class AzW { [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
[DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h,int x,int y,int w,int ht,bool r);
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);
[DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
[DllImport("user32.dll")] public static extern void mouse_event(int f,int dx,int dy,int d,int e);
[DllImport("user32.dll")] public static extern void keybd_event(byte vk,byte sc,int flags,int extra);
public struct RECT{public int L,T,R,B;} }
"@
Add-Type -AssemblyName System.Drawing
[AzW]::SetProcessDPIAware() | Out-Null
$p = Get-Process azahar | Select-Object -First 1
$wx = 1300; $wy = 40
[AzW]::MoveWindow($p.MainWindowHandle, $wx, $wy, 1200, 1300, $true) | Out-Null
[AzW]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
Start-Sleep 3

function Assert-Azahar {
    # Never click or capture unless Azahar is the foreground window (otherwise input and screenshots hit other apps).
    if ([AzW]::GetForegroundWindow() -ne $p.MainWindowHandle) {
        [AzW]::SetForegroundWindow($p.MainWindowHandle) | Out-Null; Start-Sleep -Milliseconds 500
        if ([AzW]::GetForegroundWindow() -ne $p.MainWindowHandle) { Get-Process azahar -ErrorAction SilentlyContinue | Stop-Process -Force; throw "Azahar is not the foreground window; aborting without clicking." }
    }
}
function Click($x, $y, $hold = 250) {
    Assert-Azahar
    [AzW]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 150
    [AzW]::mouse_event(2, 0, 0, 0, 0); Start-Sleep -Milliseconds $hold
    [AzW]::mouse_event(4, 0, 0, 0, 0); Start-Sleep -Milliseconds 400
}
function Press($vk, $ms) {
    Assert-Azahar
    [AzW]::keybd_event($vk, 0, 0, 0); Start-Sleep -Milliseconds $ms; [AzW]::keybd_event($vk, 0, 2, 0); Start-Sleep -Milliseconds 150
}
if ($Pick) {   # the game picker is up when both ROMs are present: A chooses Golden Sun
    Press 0x41 120
    Start-Sleep 5
}
if ($LoadState) {
    # Bottom screen is ~2.45x at 1200x1300; origin (207,700) in window coordinates.
    Click ($wx + 273 + 130 * 5) ($wy + 761)                       # MENU tab
    Click ($wx + 207 + [int](80 * 2.45)) ($wy + 700 + [int](117 * 2.45))    # Menu tab: Save slots (third button of the left column; six rows of 25 px from y=56)
    Click ($wx + 207 + [int](80 * 2.45)) ($wy + 700 + [int](184 * 2.45))    # slot 1: Load
    if ($Tab -ge 0) { Click ($wx + 273 + 130 * $Tab) ($wy + 761) }   # chosen tab (0 Party .. 5 Menu); -Tab -1 leaves the app's own tab (e.g. the battle panel)
    Start-Sleep 3
}
function Snap($path) {
    Assert-Azahar
    $r = New-Object AzW+RECT; [AzW]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
    $bmp.Save($path); Write-Output $path
}
if ($PreClicks) {
    foreach ($c in ($PreClicks -split ';')) {
        $x, $y, $w = $c.Trim() -split ','
        Click ($wx + 207 + [int]([int]$x * 2.45)) ($wy + 700 + [int]([int]$y * 2.45))
        Start-Sleep ([int]$w)
    }
}
if ($Keys) {
    $codes = @{ U = 0x26; D = 0x28; L = 0x25; R = 0x27; A = 0x41; B = 0x53; Y = 0x58; LS = 0x51; RS = 0x57 }   # arrows = D-pad; A/S = A/B; X key = Y; Q/W = L/R shoulders
    $base = [System.IO.Path]::ChangeExtension($Out, $null).TrimEnd('.')
    $n = 0
    foreach ($group in ($Keys -split ';')) {
        foreach ($k in ($group -split ',')) {
            $name, $ms = $k.Trim() -split ':'
            if (-not $codes.ContainsKey($name)) { throw "bad key '$name' (use U, D, L, R, A, B, Y, LS or RS)" }
            Press $codes[$name] ([int]$ms)
        }
        Start-Sleep 3     # let a map transition and the bottom screen settle
        $n++
        Snap "${base}_g$n.png"
    }
}
if ($Clicks) {   # -Clicks "x,y,wait;x,y,wait": taps in bottom-screen UI coordinates, a screenshot <Out base>_c1.png ... after each
    $base = [System.IO.Path]::ChangeExtension($Out, $null).TrimEnd('.')
    $n = 0
    foreach ($c in ($Clicks -split ';')) {
        $x, $y, $w, $hold = $c.Trim() -split ','
        if (-not $hold) { $hold = 250 }
        Click ($wx + 207 + [int]([int]$x * 2.45)) ($wy + 700 + [int]([int]$y * 2.45)) ([int]$hold)
        Start-Sleep ([int]$w)
        $n++
        Snap "${base}_c$n.png"
    }
}
if ($Drags) {   # -Drags "x1,y1,x2,y2,wait;...": press at (x1,y1), slide to (x2,y2) in 12 steps, release; a screenshot <Out base>_d1.png ... after each (scrolls the bestiary / message log)
    $base = [System.IO.Path]::ChangeExtension($Out, $null).TrimEnd('.')
    $n = 0
    foreach ($c in ($Drags -split ';')) {
        $x1, $y1, $x2, $y2, $w = $c.Trim() -split ','
        Assert-Azahar
        $px = { param($x) $wx + 207 + [int]([int]$x * 2.45) }; $py = { param($y) $wy + 700 + [int]([int]$y * 2.45) }
        [AzW]::SetCursorPos((& $px $x1), (& $py $y1)) | Out-Null; Start-Sleep -Milliseconds 150
        [AzW]::mouse_event(2, 0, 0, 0, 0); Start-Sleep -Milliseconds 120
        for ($i = 1; $i -le 12; ++$i) {
            [AzW]::SetCursorPos([int]((& $px $x1) + ((& $px $x2) - (& $px $x1)) * $i / 12), [int]((& $py $y1) + ((& $py $y2) - (& $py $y1)) * $i / 12)) | Out-Null
            Start-Sleep -Milliseconds 60
        }
        [AzW]::mouse_event(4, 0, 0, 0, 0); Start-Sleep -Milliseconds 400
        Start-Sleep ([int]$w)
        $n++
        Snap "${base}_d$n.png"
    }
}
if ($WaitSeconds -gt 0) { Start-Sleep $WaitSeconds }
Snap $Out
} finally {
    if ($Close) { Get-Process azahar -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 2 }
    foreach ($u in $undo) {   # put every file back the way it was (or delete the ones the run created)
        if ($u[1]) { Move-Item $u[1] $u[0] -Force } else { Remove-Item $u[0] -Force -ErrorAction SilentlyContinue }
    }
}
