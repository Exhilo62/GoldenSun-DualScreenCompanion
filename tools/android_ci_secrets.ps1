# Prepares the four GitHub Actions secrets that let CI build and attach a SIGNED release APK (.github/workflows/android.yml).
# Reads android\keystore.properties and the keystore it names (both stay out of git), writes android\ci-secrets.txt (gitignored) with the four values,
# and opens it in Notepad. Copy each value into  GitHub > repository > Settings > Secrets and variables > Actions > New repository secret.
# Delete ci-secrets.txt afterwards. The values are never printed to this console.
$root = Split-Path -Parent $PSScriptRoot
$propsPath = Join-Path $root 'android\keystore.properties'
if (-not (Test-Path $propsPath)) { Write-Host 'android\keystore.properties not found (see android/README.md).'; exit 1 }
$p = @{}
Get-Content $propsPath | ForEach-Object { if ($_ -match '^\s*([^#=\s]+)\s*=\s*(.*)$') { $p[$Matches[1]] = $Matches[2].Trim() } }
$ks = $p['storeFile']
if (-not (Test-Path $ks)) { Write-Host "keystore file not found: $ks"; exit 1 }
$b64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes($ks))
$out = Join-Path $root 'android\ci-secrets.txt'
@(
    'Create these four repository secrets (name = value):',
    '',
    'ANDROID_KEYSTORE_B64',
    $b64,
    '',
    'ANDROID_KEYSTORE_PASSWORD',
    $p['storePassword'],
    '',
    'ANDROID_KEY_ALIAS',
    $p['keyAlias'],
    '',
    'ANDROID_KEY_PASSWORD',
    $p['keyPassword']
) | Set-Content -Path $out -Encoding ASCII
Write-Host "wrote $out (four secrets). Add them on GitHub, then delete the file."
Start-Process notepad.exe $out
