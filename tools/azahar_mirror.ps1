# Drives the menu mirror in Azahar: enable it, open the Djinn game screen from a tap, close it. Needs state.ss on the virtual SD.
# Launches the .3dsx in Azahar, optionally clicks MENU > LOAD STATE (state.ss must exist on the virtual SD),
# and saves a window screenshot. usage: powershell -File tools/azahar_smoke.ps1 [-LoadState] [-Out shot.png]
param([string]$OutPrefix = "$PSScriptRoot\..\build\azahar_mirror")

# The app persists its settings; start from defaults (menus option off) and restore the user's file afterwards.
$settings = Join-Path $env:APPDATA "Azahar/sdmc/3ds/GoldenSun3DS/settings.bin"
$settingsBak = "$settings.bak"
if (Test-Path $settings) { Copy-Item $settings $settingsBak -Force; Remove-Item $settings -Force }
function Restore-Settings {
    Get-Process azahar -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep 1
    if (Test-Path $settingsBak) { Move-Item $settingsBak $settings -Force } elseif (Test-Path $settings) { Remove-Item $settings -Force }
}

$app = Resolve-Path "$PSScriptRoot\..\build\app\GoldenSun3DS.3dsx"
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
function Click($x, $y) {
    Assert-Azahar
    [AzW]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 150
    [AzW]::mouse_event(2, 0, 0, 0, 0); Start-Sleep -Milliseconds 250
    [AzW]::mouse_event(4, 0, 0, 0, 0); Start-Sleep -Milliseconds 400
}
# Bottom screen is ~2.45x at 1200x1300; origin (207,700) in window coordinates.
function Bx($x) { $wx + 207 + [int]($x * 2.45) }
function By($y) { $wy + 700 + [int]($y * 2.45) }
function Shot($name) {
    Assert-Azahar
    $r = New-Object AzW+RECT; [AzW]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
    $bmp.Save("$OutPrefix`_$name.png"); Write-Output "$OutPrefix`_$name.png"
}
try {
Click (Bx 283) (By 24)          # MENU tab
Click (Bx 150) (By 102)         # LOAD STATE
Click (Bx 150) (By 178)         # Menus on bottom screen -> On
Click (Bx 134) (By 24)          # Djinn tab (select)
Click (Bx 134) (By 24)          # Djinn tab again -> opens the game's Djinn screen
Start-Sleep 3
Shot "open"
Click (Bx 92) (By 103)          # tap "Ember" on the mirrored Djinn page: the hand should move there
Start-Sleep 4
Shot "tapped"
Click (Bx 280) (By 188)         # CLOSE
Start-Sleep 3
Shot "closed"
} finally { Restore-Settings }
