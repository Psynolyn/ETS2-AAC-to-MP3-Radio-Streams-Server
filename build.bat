@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo  BUILDING ETS2 AAC TO MP3 RADIO SERVER RELEASE PACKAGE
echo ========================================================
echo.

where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] MinGW g++ compiler not found in PATH.
    echo Please install MinGW-w64 or run from a developer command prompt.
    if "%1" neq "--no-pause" pause
    exit /b 1
)

set "BUILD_DIR=build"
set "RELEASE_NAME=ETS2-AAC-to-MP3-Radio-Streams-Server-windows_x64"
set "RELEASE_DIR=%BUILD_DIR%\%RELEASE_NAME%"
set "ZIP_FILE=%BUILD_DIR%\%RELEASE_NAME%.zip"

echo [1/6] Checking portable FFmpeg binaries in ffmpeg/...
if not exist "ffmpeg\ffmpeg.exe" (
    echo [INFO] FFmpeg binaries not found in ffmpeg/. Downloading...
    call download_ffmpeg.bat --no-pause
) else if not exist "ffmpeg\ffprobe.exe" (
    echo [INFO] FFprobe binary missing from ffmpeg/. Downloading...
    call download_ffmpeg.bat --no-pause
) else (
    echo [SUCCESS] Portable FFmpeg binaries found in ffmpeg/.
)

echo.
echo [2/6] Compiling C++ source code...
g++ -O2 -std=c++17 src/update_streams.cpp -o update_streams.exe
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile update_streams.exe!
    if "%1" neq "--no-pause" pause
    exit /b 1
)

g++ -O2 -std=c++17 src/server.cpp -o server.exe -lws2_32
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile server.exe!
    if "%1" neq "--no-pause" pause
    exit /b 1
)
echo [SUCCESS] Compilation completed successfully.

echo.
echo [3/6] Preparing build directory: %RELEASE_DIR%...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%RELEASE_DIR%" mkdir "%RELEASE_DIR%"
if not exist "%RELEASE_DIR%\ffmpeg" mkdir "%RELEASE_DIR%\ffmpeg"
if not exist "%RELEASE_DIR%\input" mkdir "%RELEASE_DIR%\input"
if not exist "%RELEASE_DIR%\output" mkdir "%RELEASE_DIR%\output"
if not exist "%RELEASE_DIR%\processed_cache" mkdir "%RELEASE_DIR%\processed_cache"

echo.
echo [4/6] Copying binaries and release assets to %RELEASE_DIR%...
copy /y update_streams.exe "%RELEASE_DIR%\" >nul
copy /y server.exe "%RELEASE_DIR%\" >nul
copy /y README.md "%RELEASE_DIR%\" >nul
copy /y LICENSE "%RELEASE_DIR%\" >nul

if exist ffmpeg\ffmpeg.exe (
    copy /y ffmpeg\ffmpeg.exe "%RELEASE_DIR%\ffmpeg\" >nul
)
if exist ffmpeg\ffprobe.exe (
    copy /y ffmpeg\ffprobe.exe "%RELEASE_DIR%\ffmpeg\" >nul
)

if exist input\live_streams.sii (
    if not exist "%RELEASE_DIR%\input\live_streams.sii" (
        copy /y input\live_streams.sii "%RELEASE_DIR%\input\" >nul
    )
)

echo.
echo [5/6] Cleaning up root repository binaries...
del /q update_streams.exe 2>nul
del /q server.exe 2>nul
echo [SUCCESS] Root workspace cleaned. Main repository remains source-only.

echo.
echo [6/6] Creating release zip package: %ZIP_FILE%...
powershell -Command "if (Test-Path '%ZIP_FILE%') { Remove-Item '%ZIP_FILE%' -Force }; Compress-Archive -Path '%RELEASE_DIR%\*' -DestinationPath '%ZIP_FILE%' -Force"

echo.
echo ========================================================
echo  BUILD COMPLETE!
echo  Release directory: %RELEASE_DIR%
echo  Release archive:   %ZIP_FILE%
echo ========================================================
if "%1" neq "--no-pause" pause
