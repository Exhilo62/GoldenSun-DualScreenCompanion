# One-off: creates assets/banner.png (256x128, HOME Menu banner) and assets/silence.wav from the mockup art. Output is committed.
Add-Type -AssemblyName PresentationCore, WindowsBase, System.Drawing
$root = Resolve-Path "$PSScriptRoot\.."
$fs = [System.IO.File]::OpenRead("$root\assets\reference\map_mockup.webp")
$dec = [System.Windows.Media.Imaging.BitmapDecoder]::Create($fs, 'None', 'OnLoad')
$enc = New-Object System.Windows.Media.Imaging.PngBitmapEncoder
$enc.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($dec.Frames[0]))
$ms = New-Object System.IO.MemoryStream; $enc.Save($ms); $fs.Close(); $ms.Position = 0
$src = [System.Drawing.Bitmap]::new($ms)
$bmp = New-Object System.Drawing.Bitmap 256, 128
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = 'HighQualityBicubic'; $g.SmoothingMode = 'HighQuality'; $g.TextRenderingHint = 'AntiAlias'
$g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, 256, 128), (New-Object System.Drawing.Rectangle 60, 430, 400, 200), 'Pixel')
$shade = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(110, 4, 10, 30)); $g.FillRectangle($shade, 0, 0, 256, 128)
$gold = [System.Drawing.Color]::FromArgb(236, 188, 68)
$pen = New-Object System.Drawing.Pen $gold, 2; $g.DrawRectangle($pen, 3, 3, 249, 121)
$f1 = New-Object System.Drawing.Font 'Georgia', 30, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$f2 = New-Object System.Drawing.Font 'Segoe UI', 15, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$sf = New-Object System.Drawing.StringFormat; $sf.Alignment = 'Center'; $sf.FormatFlags = 'NoWrap'
$g.DrawString('Golden Sun', $f1, [System.Drawing.Brushes]::Black, (New-Object System.Drawing.RectangleF 2, 22, 256, 50), $sf)
$g.DrawString('Golden Sun', $f1, (New-Object System.Drawing.SolidBrush $gold), (New-Object System.Drawing.RectangleF 0, 20, 256, 50), $sf)
$g.DrawString('3DS Companion', $f2, [System.Drawing.Brushes]::White, (New-Object System.Drawing.RectangleF 0, 78, 256, 24), $sf)
$g.Dispose(); $bmp.Save("$root\assets\banner.png")
# half a second of silence (16 kHz, 16-bit mono) for the banner jingle
$n = 8000; $data = New-Object byte[] ($n * 2)
$wav = New-Object System.IO.MemoryStream; $w = New-Object System.IO.BinaryWriter $wav
$w.Write([System.Text.Encoding]::ASCII.GetBytes('RIFF')); $w.Write([int](36 + $data.Length)); $w.Write([System.Text.Encoding]::ASCII.GetBytes('WAVEfmt '))
$w.Write([int]16); $w.Write([int16]1); $w.Write([int16]1); $w.Write([int]16000); $w.Write([int]32000); $w.Write([int16]2); $w.Write([int16]16)
$w.Write([System.Text.Encoding]::ASCII.GetBytes('data')); $w.Write([int]$data.Length); $w.Write($data); $w.Flush()
[System.IO.File]::WriteAllBytes("$root\assets\silence.wav", $wav.ToArray())
"banner.png and silence.wav written"
