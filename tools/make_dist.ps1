# Builds dist\GoldenSun3DS-Setup\ : the patcher, its launcher and everything it needs for both outputs (SD folder and .cia),
# then one zip per version: dist\releases\v<version>-<stage>\GoldenSun3DS_Patcher_v<version>-<stage>.zip (attach it to the GitHub release). It contains no ROMs.
# bin\ holds makerom and bannertool (3DSGuy / Steveice10, MIT licensed) for the CIA output. The version comes from src\version.h.
$root = Resolve-Path "$PSScriptRoot\.."
$out = "$root\dist\GoldenSun3DS-Setup"
if (-not (Test-Path "$root\build\app\GoldenSun3DS.3dsx")) { throw 'Build the app first (make in /d/gs3ds).' }
New-Item -ItemType Directory -Force "$out\bin" | Out-Null
Copy-Item "$root\build\app\GoldenSun3DS.3dsx" $out -Force
$strip = @('C:\devkitPro\devkitARM\bin\arm-none-eabi-strip.exe', '/opt/devkitpro/devkitARM/bin/arm-none-eabi-strip.exe') | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($strip) { & $strip -o "$out\GoldenSun3DS.elf" "$root\build\app\GoldenSun3DS.elf" } else { Copy-Item "$root\build\app\GoldenSun3DS.elf" $out -Force }
Copy-Item "$root\tools\patcher\GoldenSun3DS-Patcher.ps1", "$root\tools\patcher\Install.bat", "$root\tools\patcher\GoldenSun3DS.rsf" $out -Force
Copy-Item "$root\assets\icon.png", "$root\assets\banner.png", "$root\assets\silence.wav" $out -Force
Copy-Item "$root\tools\patcher\bin\*.exe" "$out\bin" -Force -ErrorAction SilentlyContinue
& "$root\tools\patcher\MakeLauncher.ps1" -OutDir $out   # GoldenSun3DS-Patcher.exe, the double-click launcher
$vh = Get-Content "$root\src\version.h" -Raw
$ver = [regex]::Match($vh, 'APP_VERSION "([^"]+)"').Groups[1].Value
$stage = [regex]::Match($vh, 'APP_STAGE "([^"]+)"').Groups[1].Value
@"
GoldenSun3DS setup (v$ver $stage)
1. Double-click GoldenSun3DS-Patcher.exe (or Install.bat if the .exe is blocked).
2. Pick New 3DS (two cores, 60 fps) or Old 3DS (single core, about 25-30 fps).
3. Choose what to make: files for the SD card, or one installable .cia with your ROMs inside.
4. Choose your own Golden Sun / The Lost Age ROM files and press Build. ROMs are not included.
CIA: install it with FBI (or Azahar: File > Install CIA) and start it from the HOME Menu. Saves go to sdmc:/3ds/GoldenSun3DS/.
This is a beta: please report obvious problems (see the project page).
"@ | Set-Content "$out\README.txt"
"$ver $stage" | Set-Content "$out\version.txt"
Copy-Item "$root\CHANGELOG.md" "$out\PATCHNOTES.txt" -Force
Write-Output "wrote $out"

$rel = Join-Path (Join-Path "$root\dist" "releases") "v$ver-$stage"
New-Item -ItemType Directory -Force $rel | Out-Null
$zip = Join-Path $rel "GoldenSun3DS_Patcher_v$ver-$stage.zip"
Remove-Item $zip -ErrorAction SilentlyContinue
Compress-Archive -Path $out -DestinationPath $zip -Force
Write-Output "wrote $zip"
