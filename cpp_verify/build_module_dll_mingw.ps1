[CmdletBinding()]
param(
    [string]$Compiler = $env:MINGW_GPP,
    [string]$OrtRoot = $env:ORT_ROOT,
    [string[]]$QtIncludeDirs = @(),
    [string]$QtLibDir = $env:QT_LIB_DIR,
    [string[]]$QtLibs = @(),
    [string[]]$OpenCvIncludeDirs = @(),
    [string]$OpenCvLibDir = $env:OPENCV_LIB_DIR,
    [string[]]$OpenCvLibs = @(),
    [switch]$DryRun,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

. (Join-Path $PSScriptRoot "scripts\common.ps1")

$repoRoot = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $PSScriptRoot "out"
$dllPath = Join-Path $outDir "Hd_AI_Station_Jr_module.dll"
$importLibPath = Join-Path $outDir "Hd_AI_Station_Jr_module.lib"
$compilerPath = Resolve-CompilerPath -Preferred $Compiler

if (-not [string]::IsNullOrWhiteSpace($OrtRoot)) {
    $OrtRoot = $OrtRoot.Trim()
}

if ($QtIncludeDirs.Count -eq 0) {
    $QtIncludeDirs = Split-EnvList -Value $env:QT_INCLUDE_DIRS
}
if ($QtLibs.Count -eq 0) {
    $QtLibs = Split-EnvList -Value $env:QT_LIBS
}
if ($OpenCvIncludeDirs.Count -eq 0) {
    $OpenCvIncludeDirs = Split-EnvList -Value $env:OPENCV_INCLUDE_DIRS
}
if ($OpenCvLibs.Count -eq 0) {
    $OpenCvLibs = Split-EnvList -Value $env:OPENCV_LIBS
}

$referenceDir = Join-Path $repoRoot "delivery\reference_qt_adapter"
$sourceFiles = @(
    (Join-Path $referenceDir "OH_Detect_Station_Module.cpp"),
    (Join-Path $referenceDir "protocol_codec.cpp"),
    (Join-Path $referenceDir "image_pipeline.cpp"),
    (Join-Path $referenceDir "seg_runtime.cpp")
)

$args = [System.Collections.Generic.List[string]]::new()
$libraryArgs = [System.Collections.Generic.List[string]]::new()
$args.Add("-std=c++17")
$args.Add("-O2")
$args.Add("-Wall")
$args.Add("-Wextra")
$args.Add("-shared")
$args.Add("-I$PSScriptRoot\include")
$args.Add("-I$repoRoot")
$args.Add("-I$referenceDir")

Add-IncludeArgs -CompileArgs $args -Paths $QtIncludeDirs
Add-LibraryDirArgs -CompileArgs $libraryArgs -Paths @($QtLibDir)
Add-LibraryArgs -CompileArgs $libraryArgs -Libraries $QtLibs

Add-IncludeArgs -CompileArgs $args -Paths $OpenCvIncludeDirs
Add-LibraryDirArgs -CompileArgs $libraryArgs -Paths @($OpenCvLibDir)
Add-LibraryArgs -CompileArgs $libraryArgs -Libraries $OpenCvLibs

if (-not [string]::IsNullOrWhiteSpace($OrtRoot)) {
    Add-IncludeArgs -CompileArgs $args -Paths @((Join-Path $OrtRoot "include"))
    Add-LibraryDirArgs -CompileArgs $libraryArgs -Paths @((Join-Path $OrtRoot "lib"))
    Add-LibraryArgs -CompileArgs $libraryArgs -Libraries @("onnxruntime")
}

foreach ($sourceFile in $sourceFiles) {
    $args.Add($sourceFile)
}

foreach ($libraryArg in $libraryArgs) {
    $args.Add($libraryArg)
}

$args.Add("-Wl,--out-implib,$importLibPath")
$args.Add("-o")
$args.Add($dllPath)

if ($Clean) {
    Remove-DirectoryIfExists -Path $outDir
}

Ensure-Directory -Path $outDir

Write-Host "BuildTarget=Hd_AI_Station_Jr_module.dll"
Write-Host "Compiler=$compilerPath"
Write-Host "DllOutput=$dllPath"
Write-Host "ImportLibOutput=$importLibPath"
Write-Host "QtIncludeDirs=$($QtIncludeDirs -join ';')"
Write-Host "QtLibDir=$QtLibDir"
Write-Host "QtLibs=$($QtLibs -join ';')"
Write-Host "OpenCvIncludeDirs=$($OpenCvIncludeDirs -join ';')"
Write-Host "OpenCvLibDir=$OpenCvLibDir"
Write-Host "OpenCvLibs=$($OpenCvLibs -join ';')"
Write-Host "OrtRoot=$OrtRoot"

if ($DryRun) {
    Write-Host "DryRun=TRUE"
    Write-Host "Command=$compilerPath $($args -join ' ')"
    exit 0
}

& $compilerPath @args
if ($LASTEXITCODE -ne 0) {
    throw "g++ failed with exit code $LASTEXITCODE"
}

if (-not [string]::IsNullOrWhiteSpace($OrtRoot)) {
    $ortDllPath = Join-Path $OrtRoot "lib\onnxruntime.dll"
    if (Test-Path -LiteralPath $ortDllPath) {
        Copy-Item -LiteralPath $ortDllPath -Destination (Join-Path $outDir "onnxruntime.dll") -Force
        Write-Host "CopiedOrtDll=$ortDllPath"
    }
}

Write-Host "BuildResult=PASS"