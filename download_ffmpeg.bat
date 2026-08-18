@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo  ETS2 RADIO SERVER - PORTABLE FFMPEG DOWNLOADER
echo ========================================================
echo.

if not exist ffmpeg (
    mkdir ffmpeg
)

if exist ffmpeg\ffmpeg.exe (
    if exist ffmpeg\ffprobe.exe (
        echo [INFO] FFmpeg binaries already exist in ffmpeg/ directory.
        echo Nothing to download. You are ready to go!
        if "%1" neq "--no-pause" pause
        exit /b 0
    )
)

echo Downloading portable FFmpeg build for Windows...
echo Please wait...
echo.

set "ZIP_URL=https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip"
set "ZIP_PATH=ffmpeg\ffmpeg_temp.zip"

where curl.exe >nul 2>&1
if %errorlevel% equ 0 (
    curl.exe -L -# -o "%ZIP_PATH%" "%ZIP_URL%"
) else (
    powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $wc = New-Object System.Net.WebClient; $wc.Headers.Add('User-Agent', 'Mozilla/5.0'); $wc.DownloadFile('%ZIP_URL%', '%ZIP_PATH%')"
)

if not exist "%ZIP_PATH%" (
    echo [ERROR] Download failed. Please check your internet connection.
    if "%1" neq "--no-pause" pause
    exit /b 1
)

echo Extracting FFmpeg binaries...
powershell -Command "Expand-Archive -Path '%ZIP_PATH%' -DestinationPath 'ffmpeg\temp_extract' -Force"

echo Moving ffmpeg.exe and ffprobe.exe to ffmpeg/...
for /f "delims=" %%i in ('dir /b /s "ffmpeg\temp_extract\ffmpeg.exe"') do copy /y "%%i" "ffmpeg\ffmpeg.exe" >nul
for /f "delims=" %%i in ('dir /b /s "ffmpeg\temp_extract\ffprobe.exe"') do copy /y "%%i" "ffmpeg\ffprobe.exe" >nul

echo Cleaning up temporary download files...
del /q "%ZIP_PATH%" 2>nul
rmdir /s /q "ffmpeg\temp_extract" 2>nul

if exist ffmpeg\ffmpeg.exe (
    if exist ffmpeg\ffprobe.exe (
        echo.
        echo ========================================================
        echo  SUCCESS: Portable FFmpeg installed into ffmpeg/
        echo  You can now run update_streams.exe and server.exe!
        echo ========================================================
    )
) else (
    echo [ERROR] Extraction failed. Please manually copy ffmpeg.exe and ffprobe.exe into ffmpeg/ folder.
)

if "%1" neq "--no-pause" pause
