# usage: runshot.ps1 name "touch spec" frames  -> build/<name>.png (test helper for the desktop build)
param([string]$Name, [string]$Touch, [int]$Frames, [string]$Game = "1")
$b = "D:\Game Development\GoldenSunDS\desktop\build"
$env:GS_GAME = $Game; $env:GS_DATA = "$b\data"; $env:GS_TOUCH = $Touch; $env:GS_SHOT = "$b\$Name.bmp"; $env:GS_FRAMES = $Frames
& "$b\app\gs_desktop.exe"
Add-Type -AssemblyName System.Drawing
$i = [System.Drawing.Image]::FromFile("$b\$Name.bmp"); $i.Save("$b\$Name.png", [System.Drawing.Imaging.ImageFormat]::Png); $i.Dispose()
