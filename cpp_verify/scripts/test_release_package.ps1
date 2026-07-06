[CmdletBinding()]
param(
    [string]$PackageDir = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if ([string]::IsNullOrWhiteSpace($PackageDir)) {
    $PackageDir = Join-Path $repoRoot "delivery\release\OH_Detect_Station_Module_Release"
}

$requiredFiles = @(
    "run_verify.bat",
    "verify_dll_smoke.exe",
    "Hd_AI_Station_Jr_module.dll",
    "Hd_AI_Station_Jr_module.lib",
    "model\best.onnx",
    "input\01_Pit_80.jpg",
    "include\base.h",
    "include\OH_Detect_Station_Module.h",
    "doc\README_verify.md"
)

Write-Host "PackageDir=$PackageDir"

foreach ($relativePath in $requiredFiles) {
    $fullPath = Join-Path $PackageDir $relativePath
    if (-not (Test-Path -LiteralPath $fullPath)) {
        throw "Missing required package file: $relativePath"
    }
}

$previousLocation = Get-Location
try {
    Set-Location -LiteralPath $PackageDir
    & cmd /c "run_verify.bat"
    if ($LASTEXITCODE -ne 0) {
        throw "run_verify.bat failed with exit code $LASTEXITCODE"
    }
} finally {
    Set-Location -LiteralPath $previousLocation
}

$expectedOutputs = @(
    "output\m_drawingImage_8bit.png",
    "output\m_rawImage_8bit.png",
    "output\m_enhancedImage_8bit.png",
    "output\m_segImage.png",
    "output\m_segImageTo255.png",
    "output\sendData.bin"
)

foreach ($relativePath in $expectedOutputs) {
    $fullPath = Join-Path $PackageDir $relativePath
    if (-not (Test-Path -LiteralPath $fullPath)) {
        throw "Missing expected output file: $relativePath"
    }
}

Write-Host "ReleasePackageSmoke=PASS"
