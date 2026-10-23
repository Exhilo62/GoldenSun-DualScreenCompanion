# One command to check everything: the headless self-test (tools/selftest.py), then the Azahar regression checks (tools/azahar_regress.ps1).
# usage: powershell -ExecutionPolicy Bypass -File tools\check_all.ps1 [-WithSets]   (-WithSets adds the five-minute Djinn sets round trip)
param([switch]$WithSets)
$root = Resolve-Path "$PSScriptRoot\.."
& python "$root\tools\selftest.py"; $a = $LASTEXITCODE
& powershell -NoProfile -ExecutionPolicy Bypass -File "$root\tools\azahar_regress.ps1"; $b = $LASTEXITCODE
$c = 0
if ($WithSets) { & powershell -NoProfile -ExecutionPolicy Bypass -File "$root\tools\azahar_regress.ps1" -Only sets; $c = $LASTEXITCODE }
Get-Process azahar -ErrorAction SilentlyContinue | Stop-Process -Force
if ($a -or $b -or $c) { Write-Output "CHECKS FAILED (selftest $a, azahar $b, sets $c)"; exit 1 } else { Write-Output "ALL CHECKS PASSED" }
