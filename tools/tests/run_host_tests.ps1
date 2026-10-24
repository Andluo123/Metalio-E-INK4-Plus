<#
.SYNOPSIS
    在 PC 上编译并运行 tests/*.cc（不需要 ESP-IDF 或开发板）。

.DESCRIPTION
    Windows 下用 MSVC（与 test_lifecycle_fixes.py 相同的工具链），
    Linux/macOS 下用 g++/clang++。

    用法：
        pwsh tools/tests/run_host_tests.ps1
        pwsh tools/tests/run_host_tests.ps1 -Compiler clang++
#>
[CmdletBinding()]
param(
    [string]$Compiler = "",
    [string]$OutDir = "build/host-tests"
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$Out = Join-Path $Root $OutDir
New-Item -ItemType Directory -Force -Path $Out | Out-Null

$Tests = Get-ChildItem -Path (Join-Path $Root "tests") -Filter *.cc | Sort-Object Name
if ($Tests.Count -eq 0) {
    throw "tests/*.cc not found"
}

function Find-MsvcCl {
    $vswhere = "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
    if (Test-Path $vswhere) {
        $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath 2>$null | Select-Object -First 1
        if ($install) {
            $vcvars = Join-Path $install "VC/Auxiliary/Build/vcvars64.bat"
            if (Test-Path $vcvars) { return $vcvars }
        }
    }
    $fallback = Get-ChildItem -Path "C:/Program Files (x86)/Microsoft Visual Studio/2022" `
        -Recurse -Filter vcvars64.bat -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($fallback) { return $fallback.FullName }
    return $null
}

function Import-MsvcEnvironment([string]$Vcvars) {
    # cl.exe 需要 INCLUDE/LIB；把 vcvars64.bat 的环境导入当前 PowerShell。
    $lines = & cmd.exe /c "`"$Vcvars`" >nul 2>&1 && set"
    foreach ($line in $lines) {
        $idx = $line.IndexOf("=")
        if ($idx -gt 0) {
            $key = $line.Substring(0, $idx)
            $value = $line.Substring($idx + 1)
            Set-Item -Path "Env:$key" -Value $value
        }
    }
}

$isWindowsHost = $IsWindows -or ($env:OS -eq "Windows_NT")
$usingMsvc = $false
if (-not $Compiler) {
    if ($isWindowsHost) {
        $vcvars = Find-MsvcCl
        if (-not $vcvars) { throw "MSVC vcvars64.bat not found; pass -Compiler <path>" }
        Import-MsvcEnvironment $vcvars
        $Compiler = (Get-Command cl.exe -ErrorAction SilentlyContinue)?.Source
        if (-not $Compiler) { throw "cl.exe not found after importing MSVC environment" }
        $usingMsvc = $true
    } else {
        $Compiler = (Get-Command g++ -ErrorAction SilentlyContinue)?.Source
        if (-not $Compiler) { $Compiler = (Get-Command clang++).Source }
    }
}

$failed = 0
foreach ($test in $Tests) {
    $name = $test.BaseName
    $exe = Join-Path $Out ($name + $(if ($isWindowsHost) { ".exe" } else { "" }))
    Write-Host "== $name ==" -ForegroundColor Cyan
    if ($usingMsvc) {
        $obj = Join-Path $Out ($name + ".obj")
        # 不加 /WX：wifi_tx_parser 的第三方头里有既有的窄化告警，
        # 这里的目标是跑断言，而不是替上游组件做 lint。
        & $Compiler /nologo /utf-8 /EHsc /std:c++17 /W4 /I $Root $test.FullName "/Fe:$exe" "/Fo:$obj"
    } else {
        & $Compiler -std=c++17 -Wall -Wextra -Werror -I $Root $test.FullName -o $exe
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  build FAILED" -ForegroundColor Red
        $failed++
        continue
    }
    & $exe
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  run FAILED (exit $LASTEXITCODE)" -ForegroundColor Red
        $failed++
    } else {
        Write-Host "  PASS" -ForegroundColor Green
    }
}

if ($failed -ne 0) {
    Write-Host "$failed host test(s) failed" -ForegroundColor Red
    exit 1
}
Write-Host "all host tests passed" -ForegroundColor Green
