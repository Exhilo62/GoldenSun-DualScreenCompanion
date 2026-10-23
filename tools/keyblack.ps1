# Shared helper (dot-sourced by genlogo.ps1 / genicon.ps1): loads an image that sits on a black background and returns a 32-bit ARGB bitmap
# in which the background is transparent. Near-black pixels connected to the border are removed (dark outline pixels inside the artwork stay), and
# a thin band of dark pixels next to the removed area is faded out so no black fringe is left on light backgrounds.
Add-Type -AssemblyName System.Drawing
function Load-KeyedBitmap([string]$path) {
    $raw = [System.Drawing.Bitmap]::new($path)
    $w = $raw.Width; $h = $raw.Height
    $src = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($src); $g.DrawImage($raw, 0, 0, $w, $h); $g.Dispose(); $raw.Dispose()
    $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
    $bd = $src.LockBits($rect, 'ReadWrite', [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $buf = New-Object byte[] ($w * $h * 4)
    [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0, $buf, 0, $buf.Length)
    $mx = New-Object 'byte[]' ($w * $h)
    for ($i = 0; $i -lt $w * $h; $i++) { $o = $i * 4; $mx[$i] = [math]::Max($buf[$o], [math]::Max($buf[$o + 1], $buf[$o + 2])) }
    $seen = New-Object 'bool[]' ($w * $h)
    $q = New-Object System.Collections.Generic.Queue[int]
    $T = 40
    for ($x = 0; $x -lt $w; $x++) { foreach ($y in @(0, ($h - 1))) { $i = $y * $w + $x; if (-not $seen[$i] -and $mx[$i] -lt $T) { $seen[$i] = $true; $q.Enqueue($i) } } }
    for ($y = 0; $y -lt $h; $y++) { foreach ($x in @(0, ($w - 1))) { $i = $y * $w + $x; if (-not $seen[$i] -and $mx[$i] -lt $T) { $seen[$i] = $true; $q.Enqueue($i) } } }
    while ($q.Count -gt 0) {
        $i = $q.Dequeue(); $x = $i % $w; $y = [int][math]::Floor($i / $w)
        foreach ($n in @(@(($x - 1), $y), @(($x + 1), $y), @($x, ($y - 1)), @($x, ($y + 1)))) {
            if ($n[0] -lt 0 -or $n[0] -ge $w -or $n[1] -lt 0 -or $n[1] -ge $h) { continue }
            $j = $n[1] * $w + $n[0]
            if (-not $seen[$j] -and $mx[$j] -lt $T) { $seen[$j] = $true; $q.Enqueue($j) }
        }
    }
    # fade band: dark-ish pixels touching the removed area get partial alpha
    $alpha = New-Object 'byte[]' ($w * $h)
    for ($i = 0; $i -lt $w * $h; $i++) { $alpha[$i] = if ($seen[$i]) { 0 } else { 255 } }
    for ($y = 1; $y -lt $h - 1; $y++) {
        for ($x = 1; $x -lt $w - 1; $x++) {
            $i = $y * $w + $x
            if ($seen[$i] -or $mx[$i] -ge 90) { continue }
            if ($seen[$i - 1] -or $seen[$i + 1] -or $seen[$i - $w] -or $seen[$i + $w]) { $alpha[$i] = [byte][math]::Min(255, [math]::Max(0, ($mx[$i] - $T) * 255 / 50)) }
        }
    }
    for ($i = 0; $i -lt $w * $h; $i++) {
        $o = $i * 4
        if ($alpha[$i] -eq 0) { $buf[$o] = 0; $buf[$o + 1] = 0; $buf[$o + 2] = 0 }
        $buf[$o + 3] = $alpha[$i]
    }
    [System.Runtime.InteropServices.Marshal]::Copy($buf, 0, $bd.Scan0, $buf.Length)
    $src.UnlockBits($bd)
    return $src
}
