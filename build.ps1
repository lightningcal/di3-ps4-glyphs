[CmdletBinding()]
param([switch]$Install, [string]$GameDir)
$ErrorActionPreference = 'Stop'
if ($Install -and -not $GameDir) { throw 'Provide -GameDir with the game installation folder.' }
$repo = $PSScriptRoot
$version = (Get-Content -LiteralPath (Join-Path $repo 'mod\mod.json') -Raw | ConvertFrom-Json).version
$cmake = (Get-Command cmake.exe -ErrorAction SilentlyContinue).Source
if (-not $cmake) { $cmake = 'C:\Program Files\CMake\bin\cmake.exe' }
& $cmake -S $repo -B (Join-Path $repo 'build') -G 'Visual Studio 17 2022' -A Win32
if ($LASTEXITCODE) { throw 'Configure failed' }
& $cmake --build (Join-Path $repo 'build') --config Release
if ($LASTEXITCODE) { throw 'Build failed' }
& (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir (Join-Path $repo 'build') -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'Native tests failed' }
$package = Join-Path $repo 'dist\mods\ps4-glyphs'
New-Item -ItemType Directory -Path $package -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'mod\main.lua'),(Join-Path $repo 'mod\mod.json') -Destination $package -Force
Copy-Item -LiteralPath (Join-Path $repo 'mod\glyphs') -Destination $package -Recurse -Force
Copy-Item -LiteralPath (Join-Path $repo 'build\Release\PS4Glyphs.asi') -Destination $package -Force
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE'),(Join-Path $repo 'README.md'),(Join-Path $repo 'VALIDATION.md'),(Join-Path $repo 'INSTALL.md') -Destination $package -Force
Copy-Item -LiteralPath (Join-Path $repo 'docs') -Destination $package -Recurse -Force
$zip = Join-Path $repo "dist\di3-ps4-glyphs-$version.zip"
Compress-Archive -Path (Join-Path $repo 'dist\mods') -DestinationPath $zip -Force
$checksum = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
"$checksum  $(Split-Path $zip -Leaf)" | Set-Content -LiteralPath (Join-Path $repo 'dist\SHA256SUMS.txt') -Encoding ascii
if ($Install) {
    if (Get-Process DisneyInfinity3 -ErrorAction SilentlyContinue) { throw 'Close the game before installing the native plugin.' }
    if (-not (Test-Path -LiteralPath (Join-Path $GameDir 'CrabeLoader.asi'))) { throw 'CrabeLoader.asi was not found in GameDir.' }
    $target = Join-Path $GameDir 'mods\ps4-glyphs'
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    foreach ($item in @('main.lua','mod.json','PS4Glyphs.asi','LICENSE','README.md','VALIDATION.md','INSTALL.md','docs','glyphs')) {
        Copy-Item -LiteralPath (Join-Path $package $item) -Destination $target -Recurse -Force
    }
    Write-Host "Installed: $target (existing settings.json preserved)."
}
Write-Host "Package: $zip"
