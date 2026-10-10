# Build a portable x64 Windows Vamp plugin without changing PATH or installing tools.
# Example: .\build-windows.ps1 -CacheDirectory C:\build-cache -OutputDirectory C:\plugins\chordino-candidate
[CmdletBinding()]
param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '.build/windows-x64'),
    [string]$CacheDirectory = (Join-Path $PSScriptRoot '.build-cache'),
    [string]$SonicAnnotator = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or
    -not [Environment]::Is64BitOperatingSystem) {
    throw 'This script requires 64-bit Windows.'
}

$sourceRoot = $PSScriptRoot
$cacheRoot = [IO.Path]::GetFullPath($CacheDirectory)
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
$manifest = Get-Content -LiteralPath (Join-Path $sourceRoot 'build-dependencies.json') -Raw | ConvertFrom-Json
$downloadRoot = Join-Path $cacheRoot 'downloads'
New-Item -ItemType Directory -Force -Path $downloadRoot, $outputRoot | Out-Null

function Get-PinnedDependency($Dependency) {
    $archive = Join-Path $downloadRoot $Dependency.archive
    if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
        Write-Host "Downloading $($Dependency.name) $($Dependency.version)..."
        Invoke-WebRequest -Uri $Dependency.url -OutFile $archive
    }
    $actualHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne $Dependency.sha256) {
        throw "SHA256 mismatch for $archive. Expected $($Dependency.sha256), got $actualHash."
    }
    $dependencyRoot = Join-Path $cacheRoot $Dependency.directory
    $marker = Join-Path $dependencyRoot '.verified-archive-sha256'
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf) -or
        (Get-Content -LiteralPath $marker -Raw).Trim() -ne $Dependency.sha256) {
        Write-Host "Extracting $($Dependency.name)..."
        Expand-Archive -LiteralPath $archive -DestinationPath $cacheRoot -Force
        Set-Content -LiteralPath $marker -Value $Dependency.sha256 -Encoding ascii
    }
    return $dependencyRoot
}

function Invoke-Checked([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$([IO.Path]::GetFileName($Executable)) failed with exit code $LASTEXITCODE."
    }
}

$compilerRoot = Get-PinnedDependency $manifest.compiler
$sdkRoot = Get-PinnedDependency $manifest.vamp_sdk
$cxx = Join-Path $compilerRoot 'bin/x86_64-w64-mingw32-clang++.exe'
$cc = Join-Path $compilerRoot 'bin/x86_64-w64-mingw32-clang.exe'
$readObject = Join-Path $compilerRoot 'bin/llvm-readobj.exe'
$workRoot = Join-Path $cacheRoot ('work/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $workRoot | Out-Null

# Deliberately omit -ffast-math: the decoder uses finite-value checks and silence
# probabilities whose behavior must not depend on unsafe floating-point rewrites.
$commonFlags = @('-O2', '-D_USE_MATH_DEFINES',
    "-ffile-prefix-map=$sourceRoot=.", "-ffile-prefix-map=$sdkRoot=./vamp-plugin-sdk",
    '-I', $sdkRoot, '-I', $sourceRoot, '-Wall', '-Wextra')
$sourceFiles = @('chromamethods.cpp', 'NNLSBase.cpp', 'NNLSChroma.cpp',
    'Chordino.cpp', 'Tuning.cpp', 'plugins.cpp', 'viterbi.cpp')
$objects = @()
foreach ($relativePath in $sourceFiles) {
    $sourcePath = Join-Path $sourceRoot $relativePath
    $objectPath = Join-Path $workRoot ($relativePath + '.o')
    Write-Host "Compiling $relativePath..."
    Invoke-Checked $cxx (@('-std=c++11') + $commonFlags + @('-c', $sourcePath, '-o', $objectPath))
    $objects += $objectPath
}
$nnlsObject = Join-Path $workRoot 'nnls.c.o'
Invoke-Checked $cc ($commonFlags + @('-c', (Join-Path $sourceRoot 'nnls.c'), '-o', $nnlsObject))
$objects += $nnlsObject
foreach ($relativePath in @('src/vamp-sdk/PluginAdapter.cpp', 'src/vamp-sdk/RealTime.cpp')) {
    $objectPath = Join-Path $workRoot ('sdk-' + [IO.Path]::GetFileName($relativePath) + '.o')
    Invoke-Checked $cxx (@('-std=c++11') + $commonFlags + @('-c', (Join-Path $sdkRoot $relativePath), '-o', $objectPath))
    $objects += $objectPath
}

# Keep decoder and dictionary regression checks coupled to the built sources.
# C++17 is needed only by the dictionary test's temporary-directory handling.
foreach ($testName in @('test_decoder', 'test_dictionary', 'test_frontend', 'test_diagnostics', 'test_seventh_transitions')) {
    $testExecutable = Join-Path $workRoot ($testName + '.exe')
    $testSource = Join-Path $sourceRoot ('tests/' + $testName + '.cpp')
    $testObjects = $objects | Where-Object { [IO.Path]::GetFileName($_) -ne 'plugins.cpp.o' }
    Invoke-Checked $cxx (@('-std=c++17', '-static') + $commonFlags + @($testSource) + $testObjects + @('-o', $testExecutable))
    Invoke-Checked $testExecutable @()
}

$exports = Join-Path $workRoot 'plugin.def'
Set-Content -LiteralPath $exports -Value "EXPORTS`nvampGetPluginDescriptor" -Encoding ascii
$pluginPath = Join-Path $outputRoot 'nnls-chroma.dll'
# The C++ runtime, unwinder, and threading runtime are statically linked. UCRT and
# kernel APIs are supplied by Windows 10/11, as with the Dechorder desktop app.
Invoke-Checked $cxx (@('-shared', '-static', '-Wl,--no-insert-timestamp', '-o', $pluginPath) + $objects + @($exports))
$inspection = & $readObject '--coff-imports' '--coff-exports' $pluginPath 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) { throw 'Could not inspect the built plugin.' }
if ($inspection -notmatch 'Name: vampGetPluginDescriptor') { throw 'Vamp plugin entry point is missing.' }
if ($inspection -match '(?im)Name: (?:libc\+\+|libunwind|libwinpthread|libgcc|libstdc\+\+).*\.dll') {
    throw 'The plugin unexpectedly depends on a separately installed compiler runtime.'
}
Set-Content -LiteralPath (Join-Path $workRoot 'pe-inspection.txt') -Value $inspection -Encoding utf8

Copy-Item -LiteralPath (Join-Path $sourceRoot 'COPYING') -Destination (Join-Path $outputRoot 'COPYING') -Force
Copy-Item -LiteralPath (Join-Path $sourceRoot 'nnls-chroma.cat') -Destination (Join-Path $outputRoot 'nnls-chroma.cat') -Force
Copy-Item -LiteralPath (Join-Path $sourceRoot 'nnls-chroma.n3') -Destination (Join-Path $outputRoot 'nnls-chroma.n3') -Force
Copy-Item -LiteralPath (Join-Path $sdkRoot 'COPYING') -Destination (Join-Path $outputRoot 'VAMP-SDK-COPYING') -Force
Copy-Item -LiteralPath (Join-Path $compilerRoot 'LICENSE.TXT') -Destination (Join-Path $outputRoot 'LLVM-LICENSE.TXT') -Force
foreach ($license in $manifest.runtime_licenses) {
    $licensePath = Join-Path $sourceRoot ('build-licenses/' + $license.file)
    if ((Get-FileHash -LiteralPath $licensePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $license.sha256) {
        throw "Runtime license checksum mismatch: $($license.file)"
    }
    Copy-Item -LiteralPath $licensePath -Destination (Join-Path $outputRoot $license.file) -Force
}
Get-ChildItem -LiteralPath (Join-Path $compilerRoot 'x86_64-w64-mingw32/share/mingw32') -Filter 'COPYING*' -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $outputRoot ('MINGW-' + $_.Name)) -Force
}

$sourceHashes = [ordered]@{}
Get-ChildItem -LiteralPath $sourceRoot -File | Where-Object {
    $_.Extension -in @('.cpp', '.c', '.h') -or $_.Name -in @('build-windows.ps1', 'build-dependencies.json')
} | Sort-Object Name | ForEach-Object {
    $sourceHashes[$_.Name] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}
$buildInfo = [ordered]@{
    schema_version = 1
    compiler = $manifest.compiler
    vamp_sdk = $manifest.vamp_sdk
    runtime_licenses = $manifest.runtime_licenses
    target = 'x86_64-w64-mingw32-ucrt'
    optimization = '-O2 (without fast-math)'
    runtime = 'static C++/unwinder/thread runtime; system Windows UCRT'
    source_sha256 = $sourceHashes
    plugin_sha256 = (Get-FileHash -LiteralPath $pluginPath -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $outputRoot 'BUILD-INFO.json'),
    ($buildInfo | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))

if ($SonicAnnotator) {
    $hostPath = (Resolve-Path -LiteralPath $SonicAnnotator).Path
    $previousVampPath = $env:VAMP_PATH
    try {
        $env:VAMP_PATH = $outputRoot
        $plugins = & $hostPath '-l' 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or $plugins -notmatch 'vamp:nnls-chroma:chordino:simplechord') {
            throw "Sonic Annotator could not load the new Chordino plugin: $plugins"
        }
        Invoke-Checked $hostPath @('-s', 'vamp:nnls-chroma:chordino:simplechord')
    } finally {
        $env:VAMP_PATH = $previousVampPath
    }
}
Write-Host "Built $pluginPath"
Write-Host "SHA256 $($buildInfo.plugin_sha256)"
