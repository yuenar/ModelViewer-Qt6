@echo off
setlocal enabledelayedexpansion

rem ======================================================================
rem  ModelViewer Android debug launcher
rem
rem  NOTE: this file is intentionally ASCII-only.  cmd.exe parses .bat
rem  bytes with the *system* ANSI code page (936 on Chinese Windows), so
rem  UTF-8 Chinese text gets split in the middle of a multi-byte char and
rem  the rest of the line is treated as a command:
rem      'xx is not recognized as an internal or external command'
rem  Keep every line ASCII here; put Chinese in the C++ / QML logs.
rem
rem  Package name: with Qt 6 Gradle namespace the manifest has no package
rem  attribute any more; the id comes from QT_ANDROID_PACKAGE_NAME and
rem  defaults to org.qtproject.example.<target>.
rem ======================================================================
set "PKG=org.qtproject.example.ModelViewer"
set "ACT=org.qtproject.qt.android.bindings.QtActivity"
set "LOG=[run]"

echo %LOG% package : %PKG%
echo %LOG% activity: %PKG%/%ACT%

rem ---- 0. device check ------------------------------------------------
adb get-state >nul 2>&1
if errorlevel 1 goto NO_DEVICE

rem ---- 1. clear every log buffer (includes crash / system history) ----
echo %LOG% 1/5 clear logcat buffers...
adb logcat -b all -c >nul 2>&1

rem ---- 2. force-stop the previous instance (stale pid) ----------------
echo %LOG% 2/5 force-stop previous instance...
adb shell am force-stop %PKG% >nul 2>&1
timeout /t 1 /nobreak >nul

rem ---- 3. launch (-W waits for the first frame) ----------------------
echo %LOG% 3/5 start activity...
adb shell am start -W -n %PKG%/%ACT%
if errorlevel 1 goto START_FAILED

rem ---- 4. wait for pid: pidof lags behind the launch, retry 10s ------
echo %LOG% 4/5 waiting for pid...
set "PID="
set /a TRY=0
:WAIT_PID
set /a TRY+=1
for /f "usebackq tokens=*" %%p in (`adb shell pidof -s %PKG% 2^>nul`) do set "PID=%%p"
if not "%PID%"=="" goto GOT_PID
if %TRY% GEQ 10 goto NO_PID
timeout /t 1 /nobreak >nul
goto WAIT_PID

:GOT_PID
echo %LOG% pid = %PID%
echo %LOG% 5/5 streaming logcat for this pid only (Ctrl+C to stop)
echo %LOG% muted noise tags: Adreno*, libEGL, Surface, Choreographer, HWUI,
echo %LOG%   OpenGLRenderer, ActivityThread, ViewRootImpl, MediaCodec*, AAudio*
echo ======================================================================

rem Filter strategy: "*:I" opens everything at Info+ for this process, then
rem each "tag:S" silences one noisy system / GPU tag.
rem NEVER silence libc / DEBUG / AndroidRuntime: the native
rem "Fatal signal 11" line and the Java exception stack come from there.
adb logcat --pid=%PID% -v time ^
    "*:I" "libc:F" "AndroidRuntime:I" ^
    "AdrenoGLES-0:S" "AdrenoGLES:S" "AdrenoUtils:S" "AdrenoVK-0:S" ^
    "libEGL:S" "OpenGLRenderer:S" "HWUI:S" "Surface:S" "SurfaceFlinger:S" ^
    "Choreographer:S" "BufferQueueConsumer:S" "BufferQueueProducer:S" ^
    "BLASTBufferQueue:S" "ActivityThread:S" "ViewRootImpl:S" ^
    "InsetsController:S" "ImeTracker:S" "InteractionJankMonitor:S" ^
    "MediaCodec:S" "MediaCodecList:S" "CCodec:S" "CCodecConfig:S" ^
    "Codec2Client:S" "AAudio:S" "AAudioStream:S" "AudioTrack:S" ^
    "AudioFlinger:S" "AudioSystem:S" "Perf:S" "RENDER_TURBO:S" ^
    "ComputilityLevel:S" "MI-PreRender:S" "IconCustomizer:S" ^
    "ViewConfiguration:S" "InputMethodManager:S"

echo ======================================================================
echo %LOG% session ended, recent crash buffer entries (empty when no crash):
adb logcat -d -b crash -t 100
pause
exit /b 0

rem ---- failure branches: goto instead of if(...) blocks --------------
:NO_DEVICE
echo [ERROR] no device/emulator in "device" state, run: adb devices
adb devices
pause
exit /b 1

:START_FAILED
echo [ERROR] am start failed, check in this order:
echo   1) installed?  adb shell pm list packages ^| findstr ModelViewer
echo   2) entry point: adb shell cmd package resolve-activity --brief %PKG%
echo   3) android.app.lib_name in the manifest must be "ModelViewer"
pause
exit /b 1

:NO_PID
echo [ERROR] no pid within 10s - the process probably died right after start.
echo -------- crash buffer (Java exception / native Fatal signal) --------
adb logcat -d -b crash -t 200
echo -------- main buffer, Error and above, last 100 lines --------------
adb logcat -d "*:E" -t 100
echo HINT: "dlopen failed / UnsatisfiedLinkError / libxxx.so not found"
echo       means a linked .so is missing from the APK (needs CMake change,
echo       e.g. QT_ANDROID_EXTRA_LIBS).
pause
exit /b 1
