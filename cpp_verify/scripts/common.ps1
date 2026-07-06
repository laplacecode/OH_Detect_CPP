Set-StrictMode -Version Latest

function Split-EnvList {
    param(
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return @()
    }

    return $Value.Split(';', [System.StringSplitOptions]::RemoveEmptyEntries) |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -ne '' }
}

function Resolve-CompilerPath {
    param(
        [string]$Preferred
    )

    if (-not [string]::IsNullOrWhiteSpace($Preferred)) {
        return $Preferred
    }

    $command = Get-Command g++ -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    throw "g++ was not found. Set MINGW_GPP or pass -Compiler."
}

function Ensure-Directory {
    param(
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Remove-DirectoryIfExists {
    param(
        [string]$Path
    )

    if (Test-Path -LiteralPath $Path) {
        for ($attempt = 0; $attempt -lt 5; $attempt++) {
            try {
                Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
                return
            } catch {
                if ($attempt -eq 4) {
                    throw
                }
                Start-Sleep -Milliseconds 300
            }
        }
    }
}

function Add-IncludeArgs {
    param(
        [System.Collections.Generic.List[string]]$CompileArgs,
        [string[]]$Paths
    )

    foreach ($path in $Paths) {
        if (-not [string]::IsNullOrWhiteSpace($path)) {
            $CompileArgs.Add("-I$path")
        }
    }
}

function Add-LibraryDirArgs {
    param(
        [System.Collections.Generic.List[string]]$CompileArgs,
        [string[]]$Paths
    )

    foreach ($path in $Paths) {
        if (-not [string]::IsNullOrWhiteSpace($path)) {
            $CompileArgs.Add("-L$path")
        }
    }
}

function Add-LibraryArgs {
    param(
        [System.Collections.Generic.List[string]]$CompileArgs,
        [string[]]$Libraries
    )

    foreach ($library in $Libraries) {
        if (-not [string]::IsNullOrWhiteSpace($library)) {
            if ($library.StartsWith('-l') -or $library.EndsWith('.a') -or $library.EndsWith('.lib')) {
                $CompileArgs.Add($library)
            } else {
                $CompileArgs.Add("-l$library")
            }
        }
    }
}

function Try-GetPkgConfigFlags {
    param(
        [string[]]$Packages
    )

    $pkgConfig = Get-Command pkg-config -ErrorAction SilentlyContinue
    if ($null -eq $pkgConfig -or $Packages.Count -eq 0) {
        return @()
    }

    $previousPreference = $false
    $hasPreferenceVariable = Test-Path Variable:PSNativeCommandUseErrorActionPreference
    if ($hasPreferenceVariable) {
        $previousPreference = $PSNativeCommandUseErrorActionPreference
        $PSNativeCommandUseErrorActionPreference = $false
    }

    try {
        $output = & $pkgConfig.Source --cflags --libs @Packages 2>$null
        if ($LASTEXITCODE -ne 0) {
            return @()
        }

        $lines = @($output)
        $tokens = [System.Collections.Generic.List[string]]::new()
        foreach ($line in $lines) {
            foreach ($token in ($line -split '\s+')) {
                if (-not [string]::IsNullOrWhiteSpace($token)) {
                    $tokens.Add($token)
                }
            }
        }

        return ,($tokens.ToArray())
    } finally {
        if ($hasPreferenceVariable) {
            $PSNativeCommandUseErrorActionPreference = $previousPreference
        }
    }
}

function Test-PkgConfigPackages {
    param(
        [string[]]$Packages
    )

    $pkgConfig = Get-Command pkg-config -ErrorAction SilentlyContinue
    if ($null -eq $pkgConfig -or $Packages.Count -eq 0) {
        return $false
    }

    $previousPreference = $false
    $hasPreferenceVariable = Test-Path Variable:PSNativeCommandUseErrorActionPreference
    if ($hasPreferenceVariable) {
        $previousPreference = $PSNativeCommandUseErrorActionPreference
        $PSNativeCommandUseErrorActionPreference = $false
    }

    try {
        & $pkgConfig.Source --exists @Packages 2>$null
        return ($LASTEXITCODE -eq 0)
    } finally {
        if ($hasPreferenceVariable) {
            $PSNativeCommandUseErrorActionPreference = $previousPreference
        }
    }
}

function Resolve-DefaultPkgConfigPackages {
    $qt6Packages = @("Qt6Core", "Qt6Gui", "opencv4")
    if (Test-PkgConfigPackages -Packages $qt6Packages) {
        return $qt6Packages
    }

    $qt5Packages = @("Qt5Core", "Qt5Gui", "opencv4")
    if (Test-PkgConfigPackages -Packages $qt5Packages) {
        return $qt5Packages
    }

    return @("opencv4")
}
