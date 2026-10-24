# Builds GoldenSun3DS-Patcher.exe (the double-click launcher) with the C# compiler that ships with Windows (.NET Framework 4), no extra downloads.
# usage: MakeLauncher.ps1 -OutDir <folder> ; the icon comes from assets\icon.png (as a multi-size .ico).
param([Parameter(Mandatory)][string]$OutDir)
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path "$PSScriptRoot\..\.."
$csc = @("$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe", "$env:WINDIR\Microsoft.NET\Framework\v4.0.30319\csc.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $csc) { Write-Warning 'csc.exe (.NET Framework 4) not found: skipping GoldenSun3DS-Patcher.exe (Install.bat still works).'; return }
$src = [System.Drawing.Bitmap]::new("$root\assets\icon.png")
$ico = Join-Path ([System.IO.Path]::GetTempPath()) 'gs3ds_launcher.ico'
$sizes = 256, 48, 32, 16
$pngs = foreach ($s in $sizes) {
    $b = New-Object System.Drawing.Bitmap $s, $s, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($b); $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'Half'
    $g.DrawImage($src, 0, 0, $s, $s); $g.Dispose()
    $m = New-Object System.IO.MemoryStream; $b.Save($m, [System.Drawing.Imaging.ImageFormat]::Png); ,($m.ToArray())
}
$fs = [System.IO.File]::Create($ico); $w = New-Object System.IO.BinaryWriter $fs
$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $d = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
    $w.Write([byte]$d); $w.Write([byte]$d); $w.Write([byte]0); $w.Write([byte]0); $w.Write([uint16]1); $w.Write([uint16]32)
    $w.Write([uint32]$pngs[$i].Length); $w.Write([uint32]$offset); $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $w.Write($p) }
$w.Close(); $fs.Close()
New-Item -ItemType Directory -Force $OutDir | Out-Null
$exe = Join-Path $OutDir 'GoldenSun3DS-Patcher.exe'
& $csc /nologo /target:winexe /optimize+ /platform:anycpu "/win32icon:$ico" "/out:$exe" /reference:System.Windows.Forms.dll "$PSScriptRoot\Launcher.cs"
if ($LASTEXITCODE -or -not (Test-Path $exe)) { throw 'csc failed' }
Remove-Item $ico -ErrorAction SilentlyContinue
Write-Output "wrote $exe"
