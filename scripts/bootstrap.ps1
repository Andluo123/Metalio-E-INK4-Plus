<#
.SYNOPSIS
    一次性准备 metalio-e-ink4-plus 的构建环境（ESP-IDF 6.1 + host 依赖）。

.DESCRIPTION
    做三件事，全部可重复执行（幂等）：
      1. 检查 ESP-IDF 路径、版本与子模块
      2. 检查 IDF_TOOLS_PATH 指向的工具链是否已安装
      3. 安装 scripts/requirements-host.txt 里的额外 Python 包

    不修改系统环境变量；只在本进程内设置 IDF_PATH/IDF_TOOLS_PATH。

.EXAMPLE
    pwsh scripts/bootstrap.ps1
    pwsh scripts/bootstrap.ps1 -IdfPath E:\esp-idf-v6.1 -ToolsPath E:\esp-idf
#>
[CmdletBinding()]
param(
    [string]$IdfPath = "E:\esp-idf-v6.1",
    [string]$ToolsPath = "E:\esp-idf",
    [switch]$SkipPythonPackages
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Fail([string]$Message) {
    Write-Host "错误: $Message" -ForegroundColor Red
    exit 1
}

Write-Host "== 1/4 检查 ESP-IDF ==" -ForegroundColor Cyan
if (-not (Test-Path $IdfPath)) {
    Fail "找不到 ESP-IDF: $IdfPath（用 -IdfPath 指定）"
}
$versionFile = Join-Path $IdfPath "version.txt"
if (Test-Path $versionFile) {
    $ver = (Get-Content $versionFile -Raw).Trim()
    Write-Host "  ESP-IDF version.txt = $ver"
    if ($ver -notmatch "^v?6\.1") {
        Write-Host "  警告: 本项目在 6.1 上验证；其他版本可能需要调整 sdkconfig/组件版本" -ForegroundColor Yellow
    }
} else {
    Write-Host "  警告: 缺少 version.txt，无法确认 IDF 版本" -ForegroundColor Yellow
}

Write-Host "== 2/4 检查子模块 ==" -ForegroundColor Cyan
$missing = @()
foreach ($sub in @("components/lwip", "components/mbedtls", "components/freertos", "components/esp_wifi")) {
    if (-not (Test-Path (Join-Path $IdfPath $sub))) {
        $missing += $sub
    }
}
if ($missing.Count -gt 0) {
    Write-Host "  缺少子模块: $($missing -join ', ')" -ForegroundColor Yellow
    Write-Host "  请执行: git -C $IdfPath submodule update --init --recursive"
    Fail "ESP-IDF 不完整"
}
Write-Host "  子模块齐全"

Write-Host "== 3/4 准备工具链 ==" -ForegroundColor Cyan
$env:IDF_PATH = $IdfPath
$env:IDF_TOOLS_PATH = $ToolsPath
$env:PYTHONUTF8 = "1"
$installScript = Join-Path $IdfPath "install.ps1"
if (-not (Test-Path $installScript)) {
    Fail "找不到 $installScript"
}
# 幂等：已安装的工具会被跳过
& $installScript
if ($LASTEXITCODE -ne 0) { Fail "install.ps1 失败" }

Write-Host "== 4/4 安装额外 Python 包 ==" -ForegroundColor Cyan
if ($SkipPythonPackages) {
    Write-Host "  已跳过"
} else {
    . (Join-Path $IdfPath "export.ps1") | Out-Null
    $python = (Get-Command python).Source
    & $python -m pip install --disable-pip-version-check -r (Join-Path $PSScriptRoot "requirements-host.txt")
    if ($LASTEXITCODE -ne 0) { Fail "pip install 失败" }
}

Write-Host ""
Write-Host "环境就绪。后续构建：" -ForegroundColor Green
Write-Host "  `$env:IDF_TOOLS_PATH='$ToolsPath'"
Write-Host "  . `"$(Join-Path $IdfPath 'export.ps1')`""
Write-Host "  idf.py build"
