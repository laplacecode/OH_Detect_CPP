[CmdletBinding()]
param(
    [ValidateSet("Smoke", "Full", "DllSmoke")]
    [string]$Mode = "Smoke",
    [string]$Compiler = $env:MINGW_GPP,
    [string]$OrtRoot = $env:ORT_ROOT,
    [string[]]$QtIncludeDirs = @(),
    [string]$QtLibDir = $env:QT_LIB_DIR,
    [string[]]$QtLibs = @(),
    [string[]]$OpenCvIncludeDirs = @(),
    [string]$OpenCvLibDir = $env:OPENCV_LIB_DIR,
    [string[]]$OpenCvLibs = @(),
    [string[]]$PkgConfigPackages = @(),
    [switch]$UsePkgConfig,
    [switch]$DryRun,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

. (Join-Path $PSScriptRoot "scripts\\common.ps1")

$repoRoot = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $PSScriptRoot "out"
$exeName = if ($Mode -eq "DllSmoke") { "verify_dll_smoke.exe" } else { "cpp_verify_smoke.exe" }
$exePath = Join-Path $outDir $exeName
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
if ($PkgConfigPackages.Count -eq 0) {
    $PkgConfigPackages = Resolve-DefaultPkgConfigPackages
}

$sourceFiles = [System.Collections.Generic.List[string]]::new()
if ($Mode -eq "DllSmoke") {
    $sourceFiles.Add((Join-Path $PSScriptRoot "src\\dll_smoke_main.cpp"))
} else {
    $sourceFiles.Add((Join-Path $PSScriptRoot "src\\smoke_main.cpp"))
}

$args = [System.Collections.Generic.List[string]]::new()
$libraryArgs = [System.Collections.Generic.List[string]]::new()
$args.Add("-std=c++17")
$args.Add("-O2")
$args.Add("-Wall")
$args.Add("-Wextra")
$args.Add("-I$PSScriptRoot\\include")
$args.Add("-I$repoRoot")

if ($Mode -eq "Full" -or $Mode -eq "DllSmoke") {
    $referenceDir = Join-Path $repoRoot "delivery\\reference_qt_adapter"
    $args.Add("-DCPP_VERIFY_FULL_BUILD=1")
    $args.Add("-I$referenceDir")

    if ($Mode -eq "Full") {
        $sourceFiles.Add((Join-Path $referenceDir "OH_Detect_Station_Module.cpp"))
        $sourceFiles.Add((Join-Path $referenceDir "protocol_codec.cpp"))
        $sourceFiles.Add((Join-Path $referenceDir "image_pipeline.cpp"))
        $sourceFiles.Add((Join-Path $referenceDir "seg_runtime.cpp"))
    }

    Add-IncludeArgs -CompileArgs $args -Paths $QtIncludeDirs
    Add-LibraryDirArgs -CompileArgs $libraryArgs -Paths @($QtLibDir)
    Add-LibraryArgs -CompileArgs $libraryArgs -Libraries $QtLibs

    Add-IncludeArgs -CompileArgs $args -Paths $OpenCvIncludeDirs
    Add-LibraryDirArgs -CompileArgs $libraryArgs -Paths @($OpenCvLibDir)
    Add-LibraryArgs -CompileArgs $libraryArgs -Libraries $OpenCvLibs

    if (-not [string]::IsNullOrWhiteSpace($OrtRoot)) {
        Add-IncludeArgs -CompileArgs $args -Paths @((Join-Path $OrtRoot "include"))
        if ($Mode -eq "Full") {
            Add-LibraryDirArgs -CompileArgs $libraryArgs -Paths @((Join-Path $OrtRoot "lib"))
            Add-LibraryArgs -CompileArgs $libraryArgs -Libraries @("onnxruntime")
        }
    }

    if ($UsePkgConfig) {
        foreach ($flag in (Try-GetPkgConfigFlags -Packages $PkgConfigPackages)) {
            if ($flag.StartsWith('-l') -or $flag.StartsWith('-L')) {
                $libraryArgs.Add($flag)
            } else {
                $args.Add($flag)
            }
        }
    }
}

foreach ($sourceFile in $sourceFiles) {
    $args.Add($sourceFile)
}

foreach ($libraryArg in $libraryArgs) {
    $args.Add($libraryArg)
}

$args.Add("-o")
$args.Add($exePath)

if ($Clean) {
    Remove-DirectoryIfExists -Path $outDir
}

Ensure-Directory -Path $outDir

Write-Host "BuildMode=$Mode"
Write-Host "Compiler=$compilerPath"
Write-Host "Output=$exePath"

if ($Mode -eq "Full") {
    Write-Host "QtIncludeDirs=$($QtIncludeDirs -join ';')"
    Write-Host "QtLibDir=$QtLibDir"
    Write-Host "QtLibs=$($QtLibs -join ';')"
    Write-Host "OpenCvIncludeDirs=$($OpenCvIncludeDirs -join ';')"
    Write-Host "OpenCvLibDir=$OpenCvLibDir"
    Write-Host "OpenCvLibs=$($OpenCvLibs -join ';')"
    Write-Host "PkgConfigPackages=$($PkgConfigPackages -join ';')"
    Write-Host "OrtRoot=$OrtRoot"
}

if ($DryRun) {
    Write-Host "DryRun=TRUE"
    Write-Host "Command=$compilerPath $($args -join ' ')"
    exit 0
}

& $compilerPath @args
if ($LASTEXITCODE -ne 0) {
    throw "g++ failed with exit code $LASTEXITCODE"
}

if (($Mode -eq "Full" -or $Mode -eq "DllSmoke") -and -not [string]::IsNullOrWhiteSpace($OrtRoot)) {
    $ortDllPath = Join-Path $OrtRoot "lib\\onnxruntime.dll"
    if (Test-Path -LiteralPath $ortDllPath) {
        Copy-Item -LiteralPath $ortDllPath -Destination (Join-Path $outDir "onnxruntime.dll") -Force
        Write-Host "CopiedOrtDll=$ortDllPath"
    }
}

Write-Host "BuildResult=PASS"
