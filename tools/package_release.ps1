Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildScript = Join-Path $root 'build-switcher.cmd'
$buildText = Get-Content -LiteralPath $buildScript -Raw
$versions = @([regex]::Matches($buildText, 'ArrowQuickSwitch-(\d+\.\d+\.\d+)\.dll') |
    ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
if ($versions.Count -ne 1) { throw 'Could not determine one release version from build-switcher.cmd.' }
$version = $versions[0]
$dllName = "ArrowQuickSwitch-$version.dll"

Add-Type -AssemblyName System.IO.Compression.FileSystem

$started = Get-Date
Write-Host "Building Arrow QuickSwitch $version..."
& cmd.exe /d /c "`"$buildScript`""
if ($LASTEXITCODE -ne 0) { throw "Build or tests failed (exit code $LASTEXITCODE). No archive was created." }

$files = [ordered]@{
    "dist\$dllName" = Join-Path $root "dist\$dllName"
    'licenses\MinHook-LICENSE.txt' = Join-Path $root 'dist\MinHook-LICENSE.txt'
    'README_INSTALL.txt' = Join-Path $root 'docs\RELEASE_INSTALL.txt'
    'CUSTOMIZATION.md' = Join-Path $root 'docs\THEMING.md'
    'licenses\miniz-LICENSE.txt' = Join-Path $root 'third_party\miniz\LICENSE'
    'licenses\ImGui-LICENSE.txt' = Join-Path $root 'third_party\imgui\LICENSE.txt'
}
# Optional legacy effect badges are local game-derived assets, not source files.
$legacyIcons = Join-Path $root 'assets\wheel-game-icons.rgba'
if (Test-Path -LiteralPath $legacyIcons -PathType Leaf) {
    $files['assets\wheel-game-icons.rgba'] = $legacyIcons
}
foreach ($source in $files.Values) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Required release file is missing: $source" }
    if ((Get-Item -LiteralPath $source).Length -eq 0) { throw "Required release file is empty: $source" }
}
if ((Get-Item -LiteralPath $files["dist\$dllName"]).LastWriteTime -lt $started.AddSeconds(-2)) {
    throw 'The release DLL was not rebuilt during this run.'
}

$release = Join-Path $root 'release'
New-Item -ItemType Directory -Path $release -Force | Out-Null
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $root 'build'))
$stage = Join-Path $buildRoot ("release-stage-" + [guid]::NewGuid().ToString('N'))
$archive = Join-Path $release "ArrowQuickSwitch-$version.zip"
$temporaryArchive = Join-Path $release (".ArrowQuickSwitch-$version-" + [guid]::NewGuid().ToString('N') + '.zip')

try {
    foreach ($relative in $files.Keys) {
        $destination = Join-Path (Join-Path $stage 'ArrowQuickSwitch') $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        if ($relative -eq 'README_INSTALL.txt') {
            $instructions = (Get-Content -LiteralPath $files[$relative] -Raw) -replace
                'ArrowQuickSwitch-\d+\.\d+\.\d+\.dll', $dllName
            [System.IO.File]::WriteAllText($destination, $instructions, [System.Text.UTF8Encoding]::new($false))
        } else {
            Copy-Item -LiteralPath $files[$relative] -Destination $destination
        }
    }
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $stage, $temporaryArchive, [System.IO.Compression.CompressionLevel]::Optimal, $false)
    $zip = [System.IO.Compression.ZipFile]::OpenRead($temporaryArchive)
    try {
        $actual = @($zip.Entries | ForEach-Object { $_.FullName.Replace('/', '\').TrimEnd('\') })
        $expected = @($files.Keys | ForEach-Object { "ArrowQuickSwitch\$_" })
        $expectedDirectories = @('ArrowQuickSwitch', 'ArrowQuickSwitch\dist', 'ArrowQuickSwitch\assets', 'ArrowQuickSwitch\licenses')
        $missing = @($expected | Where-Object { $_ -notin $actual })
        $extra = @($actual | Where-Object { $_ -notin $expected -and $_ -notin $expectedDirectories })
        if ($missing.Count -or $extra.Count -or @($actual | Select-Object -Unique).Count -ne $actual.Count) {
            throw "ZIP layout mismatch. Missing: $($missing -join ', '); unexpected: $($extra -join ', ')"
        }
        # Read every compressed file and verify it matches the staged contents.
        foreach ($entry in $zip.Entries) {
            if ($entry.FullName.EndsWith('/')) { continue }
            $stream = $entry.Open()
            $hasher = [System.Security.Cryptography.SHA256]::Create()
            try { $actualHash = [System.BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-', '') }
            finally { $stream.Dispose(); $hasher.Dispose() }
            $stagedFile = Join-Path $stage $entry.FullName.Replace('/', '\')
            if ($actualHash -ne (Get-FileHash -LiteralPath $stagedFile -Algorithm SHA256).Hash) {
                throw "ZIP integrity check failed: $($entry.FullName)"
            }
        }
    } finally { $zip.Dispose() }
    Move-Item -LiteralPath $temporaryArchive -Destination $archive -Force
    Write-Host "Ready to upload: $archive"
} finally {
    $resolvedStage = [System.IO.Path]::GetFullPath($stage)
    $buildPrefix = $buildRoot.TrimEnd('\') + '\'
    if ($resolvedStage.StartsWith($buildPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $resolvedStage)) {
        Remove-Item -LiteralPath $resolvedStage -Recurse -Force
    }
    if (Test-Path -LiteralPath $temporaryArchive) { Remove-Item -LiteralPath $temporaryArchive -Force }
}
