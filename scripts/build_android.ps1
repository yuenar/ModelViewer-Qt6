# ModelViewer-Qt6 Android 构建脚本（Windows / PowerShell）
# 用法示例：
#   .\scripts\build_android.ps1                        # 全部使用环境变量
#   .\scripts\build_android.ps1 -QtAndroidRoot D:\Qt\6.11.1\android_arm64_v8a `
#                               -NdkRoot $env:ANDROID_NDK_ROOT -Abi arm64-v8a
#
# 前提（详见 docs/ModelViewer-Qt6-Android改造实施计划.md 第七章）：
#   - Qt 6.9+ for Android（含 arm64-v8a）
#   - Android SDK（platform 34+）、NDK r26+、JDK 17
#   - assimp：优先 find_package；找不到时 CMake 会 FetchContent
#     从 GitHub 拉取源码并静态编译（需要网络）

param(
    [string]$QtAndroidRoot = $env:QT_ANDROID_ROOT,
    [string]$NdkRoot       = $env:ANDROID_NDK_ROOT,
    [string]$SdkRoot       = $env:ANDROID_SDK_ROOT,
    [string]$Abi           = "arm64-v8a",
    [string]$BuildDir      = "build-android"
)

$ErrorActionPreference = "Stop"

function Require-Path([string]$Value, [string]$Name) {
    if ([string]::IsNullOrWhiteSpace($Value)) {
        Write-Error "缺少 $Name（用参数或环境变量提供）"
    }
    if (-not (Test-Path $Value)) {
        Write-Error "$Name 路径不存在: $Value"
    }
}

Require-Path $QtAndroidRoot "Qt for Android 安装路径"
Require-Path $NdkRoot "Android NDK 路径"
Require-Path (Join-Path $NdkRoot "build\cmake\android.toolchain.cmake") "NDK 工具链文件"

$toolchain = Join-Path $NdkRoot "build\cmake\android.toolchain.cmake"
$repoRoot = Split-Path -Parent $PSScriptRoot

Write-Host "=== ModelViewer Android configure ===" -ForegroundColor Cyan
Write-Host "Qt     : $QtAndroidRoot"
Write-Host "NDK    : $NdkRoot"
Write-Host "SDK    : $SdkRoot"
Write-Host "ABI    : $Abi"

$cmakeArgs = @(
    "-B", $BuildDir,
    "-S", $repoRoot,
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
    "-DANDROID_ABI=$Abi",
    "-DANDROID_PLATFORM=android-26",
    "-DCMAKE_PREFIX_PATH=$QtAndroidRoot",
    "-DANDROID_NDK_ROOT=$NdkRoot"
)
if (-not [string]::IsNullOrWhiteSpace($SdkRoot)) {
    $cmakeArgs += "-DANDROID_SDK_ROOT=$SdkRoot"
}

cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { Write-Error "CMake 配置失败" }

Write-Host "=== build (target: apk) ===" -ForegroundColor Cyan
cmake --build $BuildDir --target apk --parallel
if ($LASTEXITCODE -ne 0) { Write-Error "APK 构建失败" }

$apkDir = Join-Path $repoRoot "$BuildDir\android-build\build\outputs\apk"
Write-Host "=== done ===" -ForegroundColor Green
Write-Host "APK 输出目录: $apkDir"
Get-ChildItem -Recurse -Filter *.apk $apkDir -ErrorAction SilentlyContinue |
    ForEach-Object { Write-Host ("  " + $_.FullName) }
