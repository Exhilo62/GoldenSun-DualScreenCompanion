# GoldenSun3DS patcher / installer. Two outputs:
#   * SD folder: the app, your own ROMs and a profile copied to 3ds\GoldenSun3DS on the SD card (start it from the Homebrew Launcher).
#   * CIA: one installable file that has your ROMs inside it (install with FBI or Azahar; start it from the HOME Menu).
#   GUI:  double-click Install.bat   (or powershell -ExecutionPolicy Bypass -STA -File GoldenSun3DS-Patcher.ps1)
#   CLI:  ... -NoGui -Output folder|cia -Console new|old -Rom1 "Golden Sun.gba" [-Rom2 "The Lost Age.gba"] [-SdRoot E:\] [-CiaPath out.cia]
#         [-Save1 gs.sps|gs.sav] [-Save2 tla.sps|tla.sav]   (optional: import a save made in another emulator)
# Profiles: new = New 3DS / New 2DS XL (two cores, 60 fps target); old = Old 3DS / 2DS (single core, roughly 25-30 fps).
# The ROMs are never bundled: you supply your own dumps. A CIA built here contains your ROM, so keep it for yourself.
param(
    [switch]$NoGui,
    [ValidateSet('folder', 'cia')][string]$Output = 'folder',
    [ValidateSet('new', 'old')][string]$Console = 'new',
    [string]$SdRoot = '',
    [string]$CiaPath = '',
    [string]$Rom1 = '',
    [string]$Rom2 = '',
    [string]$Save1 = '',
    [string]$Save2 = '',
    [switch]$Make3ds   # CLI (CIA output): also write a .3ds cartridge image next to the .cia, for emulators such as Azahar
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

function Find-File([string[]]$candidates) {
    foreach ($c in $candidates) { if (Test-Path $c) { return (Resolve-Path $c).Path } }
    return $null
}
function Find-App { Find-File @("$here\GoldenSun3DS.3dsx", "$here\..\..\build\app\GoldenSun3DS.3dsx", "$here\..\build\app\GoldenSun3DS.3dsx") }
function Find-Elf { Find-File @("$here\GoldenSun3DS.elf", "$here\..\..\build\app\GoldenSun3DS.elf") }
function Find-Asset([string]$name) { Find-File @("$here\$name", "$here\..\..\assets\$name", "$here\..\..\tools\patcher\$name") }
function Find-Tool([string]$name) {
    $p = Find-File @("$here\bin\$name.exe", "$here\$name.exe", "$here\..\bin\$name.exe")
    if ($p) { return $p }
    $c = Get-Command $name -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    return $null
}

function Get-RomCode([string]$path) {
    $fs = [System.IO.File]::OpenRead($path)
    try {
        if ($fs.Length -lt 0x200) { return $null }
        $b = New-Object byte[] 4
        $fs.Seek(0xAC, 'Begin') | Out-Null
        [void]$fs.Read($b, 0, 4)
        return [System.Text.Encoding]::ASCII.GetString($b)
    } finally { $fs.Close() }
}

# Converts a SharkPort (.sps) or raw (.sav) save into the 64 KB cartridge save the app expects. Returns the bytes.
function Convert-Save([string]$path) {
    if (-not (Test-Path $path)) { throw "Save not found: $path" }
    $d = [System.IO.File]::ReadAllBytes($path)
    if ($path.ToLower().EndsWith('.sps')) {
        $o = 0
        function U32 { $v = [System.BitConverter]::ToUInt32($d, $script:o); $script:o += 4; return $v }
        function Blob { $n = [int](U32); $b = $d[$script:o..($script:o + $n - 1)]; $script:o += $n; return ,$b }
        $script:o = 0
        $sig = [System.Text.Encoding]::ASCII.GetString((Blob))
        if ($sig -ne 'SharkPortSave') { throw "$path is not a SharkPort save." }
        [void](U32)
        foreach ($i in 1..3) { [void](Blob) }   # title, date, notes
        $n = [int](U32)
        $data = $d[$script:o..($script:o + $n - 1)]
        $mark = [System.Text.Encoding]::ASCII.GetBytes('CAMELOT')
        $hdr = -1
        for ($i = 0; $i -le $data.Length - $mark.Length; $i++) {
            $ok = $true
            for ($k = 0; $k -lt $mark.Length; $k++) { if ($data[$i + $k] -ne $mark[$k]) { $ok = $false; break } }
            if ($ok) { $hdr = $i; break }
        }
        if ($hdr -lt 0) { throw "$path does not look like a Golden Sun save (no CAMELOT header)." }
        $d = $data[$hdr..($data.Length - 1)]
    }
    $mark = [System.Text.Encoding]::ASCII.GetBytes('CAMELOT')
    $found = $false
    for ($i = 0; $i -le [math]::Min($d.Length - 7, 65536); $i++) {
        $ok = $true
        for ($k = 0; $k -lt 7; $k++) { if ($d[$i + $k] -ne $mark[$k]) { $ok = $false; break } }
        if ($ok) { $found = $true; break }
    }
    if (-not $found) { throw "$path does not look like a Golden Sun save (no CAMELOT header)." }
    $out = New-Object byte[] 65536
    for ($i = 0; $i -lt 65536; $i++) { $out[$i] = 0xFF }
    [System.Array]::Copy($d, 0, $out, 0, [math]::Min($d.Length, 65536))
    return ,$out
}

# Writes imported saves (if any) to $dir as golden_sun.sav / lost_age.sav, keeping a .bak of anything it replaces.
function Import-Saves([string]$dir, [string]$save1, [string]$save2, $log) {
    foreach ($j in @(@{ P = $save1; N = 'golden_sun.sav'; T = 'Golden Sun' }, @{ P = $save2; N = 'lost_age.sav'; T = 'The Lost Age' })) {
        if (-not $j.P) { continue }
        $bytes = Convert-Save $j.P
        $dest = Join-Path $dir $j.N
        if (Test-Path $dest) { Copy-Item $dest "$dest.bak" -Force; $log.Add("Save: kept the old $($j.N) as $($j.N).bak") }
        [System.IO.File]::WriteAllBytes($dest, $bytes)
        $log.Add("Save: $($j.T) imported -> $dest")
    }
}

# Checks the ROM files and returns @{ rom1 = path-or-$null; rom2 = ...; lines = log lines }.
function Test-Roms([string]$rom1, [string]$rom2) {
    if (-not $rom1 -and -not $rom2) { throw 'Choose at least one ROM.' }
    $lines = New-Object System.Collections.Generic.List[string]
    $jobs = @(
        @{ Path = $rom1; Prefix = 'AGS'; Name = 'golden_sun.gba'; Title = 'Golden Sun' },
        @{ Path = $rom2; Prefix = 'AGF'; Name = 'lost_age.gba'; Title = 'Golden Sun: The Lost Age' }
    )
    foreach ($j in $jobs) {
        if (-not $j.Path) { continue }
        if (-not (Test-Path $j.Path)) { throw "ROM not found: $($j.Path)" }
        if ((Get-Item $j.Path).Length -gt 16MB) { throw "$($j.Path) is too large for a GBA ROM." }
        $code = Get-RomCode $j.Path
        if (-not $code -or -not $code.StartsWith($j.Prefix)) { throw "$($j.Path) is not $($j.Title) (game code '$code', expected $($j.Prefix)*)." }
        $lines.Add("ROM: $($j.Title) ($code)")
    }
    return @{ jobs = $jobs; lines = $lines }
}

function Install-Folder([string]$console, [string]$sd, [string]$rom1, [string]$rom2, [string]$save1, [string]$save2) {
    $app = Find-App
    if (-not $app) { throw 'GoldenSun3DS.3dsx was not found next to the patcher (or in build\app).' }
    if (-not $sd -or -not (Test-Path $sd)) { throw 'Choose the SD card (or output) folder first.' }
    $chk = Test-Roms $rom1 $rom2
    $log = New-Object System.Collections.Generic.List[string]
    $dir = Join-Path $sd '3ds\GoldenSun3DS'
    New-Item -ItemType Directory -Force $dir | Out-Null
    Copy-Item $app (Join-Path $dir 'GoldenSun3DS.3dsx') -Force
    $log.Add("App: $dir\GoldenSun3DS.3dsx")
    foreach ($j in $chk.jobs) {
        if (-not $j.Path) { continue }
        Copy-Item $j.Path (Join-Path $dir $j.Name) -Force
        $log.Add("ROM -> $($j.Name)")
    }
    Set-Content -Path (Join-Path $dir 'profile.txt') -Value $console -NoNewline
    $log.Add("Profile: $console")
    Import-Saves $dir $save1 $save2 $log
    $settings = Join-Path $dir 'settings.bin'   # first-run defaults only, never over a player's own settings
    if (-not (Test-Path $settings)) {
        $top = if ($console -eq 'new') { 1 } else { 0 }
        [System.IO.File]::WriteAllBytes($settings, [byte[]]@($top, 0, 0, 1))
        $log.Add("Settings: top screen $(if ($top) { 'Fill' } else { '1x' }), autosave on")
    } else { $log.Add('Settings: kept your existing settings.bin') }
    $log.Add('Done. Start it from the Homebrew Launcher (3ds\GoldenSun3DS).')
    return $log
}

function Build-Cia([string]$console, [string]$cia, [string]$rom1, [string]$rom2, [string]$save1, [string]$save2, [bool]$make3ds = $false) {
    if ([string]::IsNullOrWhiteSpace($cia)) { throw 'Choose where to save the .cia (Browse...).' }
    if ((Test-Path -LiteralPath $cia -PathType Container) -or -not $cia.ToLower().EndsWith('.cia')) {   # a folder (or a name without .cia) picked: name the files automatically
        $cia = if (Test-Path -LiteralPath $cia -PathType Container) { Join-Path $cia 'Golden Sun - Dual Screen.cia' } else { $cia + '.cia' }
    }
    $makerom = Find-Tool 'makerom'
    $bannertool = Find-Tool 'bannertool'
    if (-not $makerom -or -not $bannertool) { throw 'makerom.exe and bannertool.exe are needed for a CIA: put them in a "bin" folder next to this script (or on PATH).' }
    $elf = Find-Elf
    if (-not $elf) { throw 'GoldenSun3DS.elf was not found next to the patcher (or in build\app).' }
    $rsf = Find-Asset 'GoldenSun3DS.rsf'; $icon = Find-Asset 'icon.png'; $banner = Find-Asset 'banner.png'; $wav = Find-Asset 'silence.wav'
    if (-not ($rsf -and $icon -and $banner -and $wav)) { throw 'CIA assets (GoldenSun3DS.rsf, icon.png, banner.png, silence.wav) are missing.' }
    if (-not $cia) { throw 'Choose where to save the .cia first.' }
    $chk = Test-Roms $rom1 $rom2
    $log = New-Object System.Collections.Generic.List[string]
    $work = Join-Path ([System.IO.Path]::GetTempPath()) ('gs3ds_cia_' + [guid]::NewGuid().ToString('N').Substring(0, 8))
    $romfs = Join-Path $work 'romfs'
    New-Item -ItemType Directory -Force $romfs | Out-Null
    try {
        foreach ($j in $chk.jobs) {
            if (-not $j.Path) { continue }
            Copy-Item $j.Path (Join-Path $romfs $j.Name)
            $log.Add("Embedded: $($j.Title)")
        }
        Set-Content -Path (Join-Path $romfs 'profile.txt') -Value $console -NoNewline
        $log.Add("Profile: $console")
        & $bannertool makebanner -i $banner -a $wav -o "$work\banner.bnr" | Out-Null
        if ($LASTEXITCODE) { throw 'bannertool could not make the banner.' }
        & $bannertool makesmdh -s 'Golden Sun - Dual Screen Companion' -l 'Golden Sun - Dual Screen Companion' -p 'GoldenSun3DS' -i $icon -o "$work\icon.smdh" | Out-Null
        if ($LASTEXITCODE) { throw 'bannertool could not make the icon.' }
        $outDir = Split-Path -Parent $cia
        if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Force $outDir | Out-Null }
        $mr = & $makerom -f cia -o $cia -target t -exefslogo -rsf $rsf -elf $elf -banner "$work\banner.bnr" -icon "$work\icon.smdh" "-DAPP_ROMFS=$romfs" -major 1 -minor 0 -micro 0 2>&1
        if ($LASTEXITCODE -or -not (Test-Path $cia)) { throw ("makerom failed:`n" + ($mr -join "`n")) }
        $mb = [math]::Round((Get-Item $cia).Length / 1MB, 1)
        $log.Add("CIA: $cia ($mb MB)")
        if ($make3ds) {   # the same program as a cartridge image (.3ds), which emulators open directly; not for a real console
            $c3 = [System.IO.Path]::ChangeExtension($cia, '.3ds')
            $mr3 = & $makerom -f cci -o $c3 -target t -exefslogo -rsf $rsf -elf $elf -banner "$work\banner.bnr" -icon "$work\icon.smdh" "-DAPP_ROMFS=$romfs" -major 1 -minor 0 -micro 0 2>&1
            if ($LASTEXITCODE -or -not (Test-Path $c3)) { throw ("makerom could not make the .3ds:`n" + ($mr3 -join "`n")) }
            $log.Add("3DS image: $c3 ($([math]::Round((Get-Item $c3).Length / 1MB, 1)) MB) - open it in an emulator such as Azahar (File > Open); it does not run on a console")
        }
        $log.Add('Install it with FBI on the console (or File > Install CIA in Azahar), then start Golden Sun - Dual Screen Companion from the HOME Menu.')
        $log.Add('Saves and settings go to sdmc:/3ds/GoldenSun3DS/ on the SD card.')
        if ($save1 -or $save2) {
            Import-Saves (Split-Path -Parent $cia) $save1 $save2 $log
            $log.Add('Copy the .sav file(s) next to the .cia to sdmc:/3ds/GoldenSun3DS/ on the SD card before the first start.')
        }
    } finally { Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue }
    return $log
}

if ($NoGui) {
    if ($Output -eq 'cia') { Build-Cia $Console $CiaPath $Rom1 $Rom2 $Save1 $Save2 $Make3ds | ForEach-Object { Write-Output $_ } }
    else { Install-Folder $Console $SdRoot $Rom1 $Rom2 $Save1 $Save2 | ForEach-Object { Write-Output $_ } }
    return
}

Add-Type -AssemblyName System.Windows.Forms, System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$navy = [System.Drawing.Color]::FromArgb(12, 24, 56)
$card = [System.Drawing.Color]::FromArgb(20, 38, 86)
$gold = [System.Drawing.Color]::FromArgb(236, 188, 68)
$text = [System.Drawing.Color]::FromArgb(244, 246, 255)
$dim = [System.Drawing.Color]::FromArgb(150, 160, 196)

$form = New-Object System.Windows.Forms.Form
$form.Text = 'Golden Sun - Dual Screen Companion - Patcher'
$form.Size = New-Object System.Drawing.Size(640, 840)
$form.StartPosition = 'CenterScreen'
$form.BackColor = $navy
$form.ForeColor = $text
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.Font = New-Object System.Drawing.Font('Segoe UI', 10)

function New-Label($t, $x, $y, $w, $h, $color, $size, $bold) {
    $l = New-Object System.Windows.Forms.Label
    $l.Text = $t; $l.Location = New-Object System.Drawing.Point($x, $y); $l.Size = New-Object System.Drawing.Size($w, $h)
    $l.ForeColor = $color; $l.BackColor = [System.Drawing.Color]::Transparent
    $style = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }
    $l.Font = New-Object System.Drawing.Font('Segoe UI', $size, $style)
    $form.Controls.Add($l); return $l
}
function New-Button($t, $x, $y, $w, $h) {
    $b = New-Object System.Windows.Forms.Button
    $b.Text = $t; $b.Location = New-Object System.Drawing.Point($x, $y); $b.Size = New-Object System.Drawing.Size($w, $h)
    $b.FlatStyle = 'Flat'; $b.BackColor = $card; $b.ForeColor = $gold
    $b.FlatAppearance.BorderColor = $gold
    $form.Controls.Add($b); return $b
}
function New-Box($x, $y, $w) {
    $t = New-Object System.Windows.Forms.TextBox
    $t.Location = New-Object System.Drawing.Point($x, $y); $t.Size = New-Object System.Drawing.Size($w, 26)
    $t.BackColor = $card; $t.ForeColor = $text; $t.BorderStyle = 'FixedSingle'
    $form.Controls.Add($t); return $t
}
function New-Radio($t, $x, $y, $checked, $parent) {
    $r = New-Object System.Windows.Forms.RadioButton
    $r.Text = $t; $r.Location = New-Object System.Drawing.Point($x, $y); $r.Size = New-Object System.Drawing.Size(560, 26)
    $r.Checked = $checked; $r.ForeColor = $text; $parent.Controls.Add($r); return $r
}
# Radio buttons that share a parent form one group, so each question gets its own panel (otherwise only one of the four could be selected).
function New-Group($x, $y, $w, $h) {
    $p = New-Object System.Windows.Forms.Panel
    $p.Location = New-Object System.Drawing.Point($x, $y); $p.Size = New-Object System.Drawing.Size($w, $h); $p.BackColor = $form.BackColor
    $form.Controls.Add($p); return $p
}

[void](New-Label 'GOLDEN SUN 3DS' 20 14 380 36 $gold 20 $true)   # header controls must not overlap: transparent labels paint over what is below them
# Version (version.txt sits next to this script in the release zip; a source checkout reads src\version.h) and the patch notes.
$verText = 'dev build'
$vf = Find-File @("$here\version.txt")
if ($vf) { $verText = (Get-Content $vf -Raw).Trim() }
else {
    $vh = Find-File @("$here\..\..\src\version.h")
    if ($vh) {
        $h = Get-Content $vh -Raw
        $verText = ([regex]::Match($h, 'APP_VERSION "([^"]+)"').Groups[1].Value + ' ' + [regex]::Match($h, 'APP_STAGE "([^"]+)"').Groups[1].Value).Trim()
    }
}
$lblVer = New-Label ("Version " + $verText) 380 54 148 22 $gold 10 $true
$lblVer.TextAlign = [System.Drawing.ContentAlignment]::MiddleRight
$btnNotes = New-Button 'Patch notes' 412 14 114 30
# Round badge in the top right corner (badge.png sits next to this script in the release zip, or in assets\ in a source checkout).
$badgeFile = Find-Asset 'badge.png'
if ($badgeFile) {
    $pic = New-Object System.Windows.Forms.PictureBox
    $pic.Location = New-Object System.Drawing.Point(536, 8); $pic.Size = New-Object System.Drawing.Size(80, 80)
    $pic.SizeMode = 'Zoom'; $pic.BackColor = [System.Drawing.Color]::Transparent
    $badgeMs = New-Object System.IO.MemoryStream (, [System.IO.File]::ReadAllBytes($badgeFile))   # from memory, so the file is not locked while the patcher is open
    $pic.Image = [System.Drawing.Image]::FromStream($badgeMs)
    $form.Controls.Add($pic)
}
$btnNotes.Add_Click({
    $nf = Find-File @("$here\PATCHNOTES.txt", "$here\..\..\CHANGELOG.md")
    $notes = if ($nf) { (Get-Content $nf -Raw) -replace "`r?`n", "`r`n" } else { 'No patch notes were found next to the patcher.' }
    $nfm = New-Object System.Windows.Forms.Form
    $bf = Find-File @("$here\build.txt")
    $nfm.Text = 'Golden Sun - Dual Screen Companion - patch notes - version ' + $verText + $(if ($bf) { ' (built ' + (Get-Content $bf -Raw).Trim() + ')' } else { '' })
    $nfm.Size = New-Object System.Drawing.Size(640, 560); $nfm.StartPosition = 'CenterParent'
    $nfm.BackColor = $navy; $nfm.ForeColor = $text; $nfm.FormBorderStyle = 'FixedDialog'; $nfm.MaximizeBox = $false; $nfm.MinimizeBox = $false
    $tb = New-Object System.Windows.Forms.TextBox
    $tb.Multiline = $true; $tb.ReadOnly = $true; $tb.ScrollBars = 'Vertical'; $tb.WordWrap = $true
    $tb.Location = New-Object System.Drawing.Point(16, 16); $tb.Size = New-Object System.Drawing.Size(592, 496)
    $tb.BackColor = $card; $tb.ForeColor = $text; $tb.BorderStyle = 'FixedSingle'; $tb.Font = New-Object System.Drawing.Font('Consolas', 9.5)
    $tb.Text = $notes; $tb.SelectionStart = 0; $tb.SelectionLength = 0
    $nfm.Controls.Add($tb)
    [void]$nfm.ShowDialog($form)
})

[void](New-Label 'Pick your console, point to your own ROMs, and install.' 22 52 356 22 $dim 10 $false)

[void](New-Label '1.  Which 3DS do you have?' 22 88 400 24 $text 11 $true)
$grp1 = New-Group 22 112 600 66
$rNew = New-Radio 'New 3DS / New 3DS XL / New 2DS XL   (two cores, targets 60 fps)' 18 4 $true $grp1
$rOld = New-Radio 'Old 3DS / 3DS XL / 2DS   (single core, about 25-30 fps)' 18 32 $false $grp1

[void](New-Label '2.  What should it make?' 22 182 560 24 $text 11 $true)
$grp2 = New-Group 22 206 600 62
$rFolder = New-Radio 'Files for the SD card (Homebrew Launcher, a 3ds folder)' 18 32 $false $grp2
$rCia = New-Radio 'One installable .cia with the ROMs inside (HOME Menu)  - recommended' 18 4 $true $grp2

[void](New-Label '3.  Where to put it' 22 276 300 24 $text 11 $true)
$chk3ds = New-Object System.Windows.Forms.CheckBox
$chk3ds.Text = 'Also make a .3ds file for emulators'; $chk3ds.Location = New-Object System.Drawing.Point(330, 274); $chk3ds.Size = New-Object System.Drawing.Size(290, 24)   # ends above the Browse button (y 302) so it cannot cover its border
$chk3ds.ForeColor = $text; $chk3ds.BackColor = $navy
$form.Controls.Add($chk3ds)
$boxOut = New-Box 40 304 440
$btnOut = New-Button 'Browse...' 490 302 110 30
$btnOut.Add_Click({
    if ($rCia.Checked) {
        $d = New-Object System.Windows.Forms.SaveFileDialog
        $d.Filter = 'CIA (*.cia)|*.cia'; $d.FileName = 'Golden Sun - Dual Screen.cia'
        if ($d.ShowDialog() -eq 'OK') { $boxOut.Text = $d.FileName }
    } else {
        $d = New-Object System.Windows.Forms.FolderBrowserDialog
        $d.Description = 'Select the root of the 3DS SD card'
        if ($d.ShowDialog() -eq 'OK') { $boxOut.Text = $d.SelectedPath }
    }
})
$rCia.Add_CheckedChanged({ $boxOut.Text = ''; $chk3ds.Enabled = $rCia.Checked; if (-not $rCia.Checked) { $chk3ds.Checked = $false } })

function Add-RomRow($label, $y) {
    [void](New-Label $label 22 $y 560 24 $text 11 $true)
    $b = New-Box 40 ($y + 28) 440
    $btn = New-Button 'Browse...' 490 ($y + 26) 110 30
    $btn.Tag = $b
    $btn.Add_Click({
        $d = New-Object System.Windows.Forms.OpenFileDialog
        $d.Filter = 'GBA ROM (*.gba)|*.gba|All files|*.*'
        if ($d.ShowDialog() -eq 'OK') { $this.Tag.Text = $d.FileName }
    })
    return $b
}
$boxR1 = Add-RomRow '4.  Golden Sun ROM  (optional if you only want The Lost Age)' 344
$boxR2 = Add-RomRow '5.  Golden Sun: The Lost Age ROM  (optional)' 412

$boxS1 = Add-RomRow '6.  Golden Sun save to import  (optional: .sps or .sav)' 480
$boxS2 = Add-RomRow '7.  The Lost Age save to import  (optional)' 548
$btnGo = New-Button 'Build' 40 626 560 40
$btnGo.Font = New-Object System.Drawing.Font('Segoe UI', 12, [System.Drawing.FontStyle]::Bold)

$logBox = New-Object System.Windows.Forms.TextBox
$logBox.Multiline = $true; $logBox.ReadOnly = $true; $logBox.ScrollBars = 'Vertical'
$logBox.Location = New-Object System.Drawing.Point(40, 678); $logBox.Size = New-Object System.Drawing.Size(560, 112)
$logBox.BackColor = $card; $logBox.ForeColor = $text; $logBox.BorderStyle = 'FixedSingle'
$logBox.Font = New-Object System.Drawing.Font('Consolas', 9)
$form.Controls.Add($logBox)

$btnGo.Add_Click({
    $logBox.Text = ''
    $btnGo.Enabled = $false
    try {
        $console = if ($rOld.Checked) { 'old' } else { 'new' }
        if ($rCia.Checked) { $lines = Build-Cia $console $boxOut.Text.Trim() $boxR1.Text.Trim() $boxR2.Text.Trim() $boxS1.Text.Trim() $boxS2.Text.Trim() $chk3ds.Checked }
        else { $lines = Install-Folder $console $boxOut.Text.Trim() $boxR1.Text.Trim() $boxR2.Text.Trim() $boxS1.Text.Trim() $boxS2.Text.Trim() }
        $logBox.Text = ($lines -join "`r`n")
    } catch {
        $logBox.Text = 'Problem: ' + $_.Exception.Message
    } finally { $btnGo.Enabled = $true }
})

[void]$form.ShowDialog()
