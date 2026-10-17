param([string]$Atlas = (Join-Path $PSScriptRoot '..\assets\ps4-glyph-atlas.png'))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$spec = Get-Content (Join-Path $PSScriptRoot '..\assets\atlas.json') -Raw | ConvertFrom-Json
$out = Join-Path $PSScriptRoot '..\mod\glyphs'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$image = [Drawing.Bitmap]::new((Resolve-Path $Atlas).Path)
try {
    if ($image.Width -ne $spec.sourceSize[0] -or $image.Height -ne $spec.sourceSize[1]) { throw 'Unexpected atlas size.' }
    for ($i = 0; $i -lt $spec.cells.Count; $i++) {
        $r = $spec.cropRectangles[$i]
        $rect = [Drawing.Rectangle]::new($r[0], $r[1], $r[2], $r[3])
        $tile = [Drawing.Bitmap]::new(209,209,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [Drawing.Graphics]::FromImage($tile)
        try {
            $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
            $destination = [Drawing.Rectangle]::new([int][Math]::Floor((209-$r[2])/2),[int][Math]::Floor((209-$r[3])/2),$r[2],$r[3])
            $graphics.DrawImage($image,$destination,$rect,[Drawing.GraphicsUnit]::Pixel)
            $tile.Save((Join-Path $out ($spec.cells[$i] + '.png')), [Drawing.Imaging.ImageFormat]::Png)
        } finally { $graphics.Dispose(); $tile.Dispose() }
    }
} finally { $image.Dispose() }
Write-Host "Saved $($spec.cells.Count) transparent glyph PNGs using verified artwork bounds."
