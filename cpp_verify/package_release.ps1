[CmdletBinding()]
param(
    [string]$PackageDir = "",
    [string]$OrtRoot = $env:ORT_ROOT,
    [string]$RuntimeBinDir = "C:\msys64\ucrt64\bin"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

. (Join-Path $PSScriptRoot "scripts\common.ps1")

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($PackageDir)) {
    $PackageDir = Join-Path $repoRoot "delivery\release\OH_Detect_Station_Module_Release"
}
if ([string]::IsNullOrWhiteSpace($OrtRoot)) {
    $OrtRoot = "C:\opt\onnxruntime-win-x64-1.25.0"
}

function Assert-PathExists {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label does not exist: $Path"
    }
}

function Copy-FileRequired {
    param(
        [string]$Source,
        [string]$Destination
    )

    Assert-PathExists -Path $Source -Label "Source file"
    Ensure-Directory -Path (Split-Path -Parent $Destination)
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Copy-DirectoryRequired {
    param(
        [string]$Source,
        [string]$Destination
    )

    Assert-PathExists -Path $Source -Label "Source directory"
    Ensure-Directory -Path (Split-Path -Parent $Destination)
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
}

function Copy-RuntimeByPattern {
    param(
        [string]$Pattern
    )

    if (-not (Test-Path -LiteralPath $RuntimeBinDir)) {
        return
    }

    Get-ChildItem -LiteralPath $RuntimeBinDir -Filter $Pattern -File -ErrorAction SilentlyContinue |
        ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $PackageDir $_.Name) -Force
            Write-Host "CopiedRuntimeDll=$($_.FullName)"
        }
}

function Copy-LddDependencies {
    param(
        [string[]]$BinaryPaths
    )

    $lddPath = Join-Path $RuntimeBinDir "ldd.exe"
    if (-not (Test-Path -LiteralPath $lddPath)) {
        $command = Get-Command ldd -ErrorAction SilentlyContinue
        if ($null -eq $command) {
            return
        }
        $lddPath = $command.Source
    }

    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $queue = [System.Collections.Generic.Queue[string]]::new()
    foreach ($binaryPath in $BinaryPaths) {
        if (Test-Path -LiteralPath $binaryPath) {
            $queue.Enqueue($binaryPath)
        }
    }

    while ($queue.Count -gt 0) {
        $current = $queue.Dequeue()
        $output = & $lddPath $current 2>$null
        foreach ($line in @($output)) {
            foreach ($match in [regex]::Matches($line, '[A-Za-z]:\\[^\s]+?\.dll')) {
                $dllPath = $match.Value
                if (-not (Test-Path -LiteralPath $dllPath)) {
                    continue
                }
                if ($dllPath.StartsWith("$env:WINDIR\", [System.StringComparison]::OrdinalIgnoreCase)) {
                    continue
                }
                if ($seen.Add($dllPath)) {
                    Copy-Item -LiteralPath $dllPath -Destination (Join-Path $PackageDir (Split-Path -Leaf $dllPath)) -Force
                    Write-Host "CopiedLddDll=$dllPath"
                    $queue.Enqueue($dllPath)
                }
            }
        }
    }
}

$resolvedRepoRoot = [System.IO.Path]::GetFullPath($repoRoot)
$resolvedPackageDir = [System.IO.Path]::GetFullPath($PackageDir)
if (-not $resolvedPackageDir.StartsWith($resolvedRepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "PackageDir must be inside repository root. PackageDir=$resolvedPackageDir RepoRoot=$resolvedRepoRoot"
}

$outDir = Join-Path $PSScriptRoot "out"
$moduleDll = Join-Path $outDir "Hd_AI_Station_Jr_module.dll"
$moduleLib = Join-Path $outDir "Hd_AI_Station_Jr_module.lib"
$smokeExe = Join-Path $outDir "verify_dll_smoke.exe"
$ortDll = Join-Path $outDir "onnxruntime.dll"
if (-not (Test-Path -LiteralPath $ortDll)) {
    $ortDll = Join-Path $OrtRoot "lib\onnxruntime.dll"
}

Assert-PathExists -Path $moduleDll -Label "Module DLL"
Assert-PathExists -Path $moduleLib -Label "Module import library"
Assert-PathExists -Path $smokeExe -Label "DLL smoke verifier"
Assert-PathExists -Path $ortDll -Label "ONNX Runtime DLL"


Ensure-Directory -Path $PackageDir
Ensure-Directory -Path (Join-Path $PackageDir "include")
Ensure-Directory -Path (Join-Path $PackageDir "input")
Ensure-Directory -Path (Join-Path $PackageDir "output")
Ensure-Directory -Path (Join-Path $PackageDir "doc")

Copy-FileRequired -Source $moduleDll -Destination (Join-Path $PackageDir "Hd_AI_Station_Jr_module.dll")
Copy-FileRequired -Source $moduleLib -Destination (Join-Path $PackageDir "Hd_AI_Station_Jr_module.lib")
Copy-FileRequired -Source $smokeExe -Destination (Join-Path $PackageDir "verify_dll_smoke.exe")
Copy-FileRequired -Source $ortDll -Destination (Join-Path $PackageDir "onnxruntime.dll")
Copy-FileRequired -Source (Join-Path $PSScriptRoot "package_templates\run_verify.bat") -Destination (Join-Path $PackageDir "run_verify.bat")
Copy-FileRequired -Source (Join-Path $PSScriptRoot "package_templates\README_verify.md") -Destination (Join-Path $PackageDir "doc\README_verify.md")
Copy-FileRequired -Source (Join-Path $PSScriptRoot "package_templates\customer_usage_old_style.md") -Destination (Join-Path $PackageDir "doc\customer_usage_old_style.md")
Copy-FileRequired -Source (Join-Path $repoRoot "delivery\package\base.h") -Destination (Join-Path $PackageDir "include\base.h")
Copy-FileRequired -Source (Join-Path $repoRoot "delivery\package\OH_Detect_Station_Module.h") -Destination (Join-Path $PackageDir "include\OH_Detect_Station_Module.h")
Copy-DirectoryRequired -Source (Join-Path $repoRoot "delivery\package\model") -Destination (Join-Path $PackageDir "model")
Copy-DirectoryRequired -Source (Join-Path $repoRoot "delivery\package\doc") -Destination (Join-Path $PackageDir "doc\interface")
Copy-FileRequired -Source (Join-Path $repoRoot "python_verify\adapter_input\01_Pit_80.jpg") -Destination (Join-Path $PackageDir "input\01_Pit_80.jpg")

Copy-LddDependencies -BinaryPaths @(
    (Join-Path $PackageDir "verify_dll_smoke.exe"),
    (Join-Path $PackageDir "Hd_AI_Station_Jr_module.dll")
)

$fallbackPatterns = @(
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "libopencv_*.dll",
    "opencv_world*.dll",
    "libjpeg-8.dll",
    "libOpenEXR-3_4.dll",
    "libOpenEXRCore-3_4.dll",
    "libOpenEXRUtil-3_4.dll",
    "libopenjp2-7.dll",
    "libpng16-16.dll",
    "libtiff-6.dll",
    "libwebp-7.dll",
    "libwebpdemux-2.dll",
    "libwebpmux-3.dll",
    "zlib1.dll",
    "libImath-3_2.dll",
    "libIex-3_4.dll",
    "libIlmThread-3_4.dll",
    "libdeflate.dll",
    "libjbig-0.dll",
    "libLerc.dll",
    "liblzma-5.dll",
    "libzstd.dll",
    "libsharpyuv-0.dll",
    "libtbb12.dll",
    "libdouble-conversion.dll",
    "libb2-1.dll",
    "libopenblas.dll",
    "liblcms2-2.dll",
    "libaec-0.dll",
    "libbrotlicommon.dll",
    "libbrotlidec.dll",
    "libbrotlienc.dll",
    "libgcc_s_*.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
    "VCRUNTIME140_1.dll",
    "VCRUNTIME140.dll",
    "MSVCP140_1.dll",
    "MSVCP140.dll",
    "libpcre2-16-0.dll",
    "libopenjph-*.dll",
    "libicuuc*.dll",
    "libicuin*.dll",
    "libicudt*.dll",
    "libquadmath-0.dll",
    "libgomp-1.dll",
    "libgfortran-5.dll",
    "libutf8_validity.dll",
    "libprotobuf.dll",
    "libabsl*.dll"
)
foreach ($pattern in $fallbackPatterns) {
    Copy-RuntimeByPattern -Pattern $pattern
}

Write-Host "PackageDir=$PackageDir"
Write-Host "PackageResult=PASS"