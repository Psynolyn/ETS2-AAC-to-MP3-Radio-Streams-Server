@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo  ETS2 RADIO SERVER - PORTABLE FFMPEG DOWNLOADER
echo ========================================================
echo.

if not exist bin (
    mkdir bin
)

if exist bin\ffmpeg.exe (
    if exist bin\ffprobe.exe (
        echo [INFO] FFmpeg binaries already exist in bin/ directory.
        echo Nothing to download. You are ready to go!
        pause
        exit /b 0
    )
)

echo Downloading portable FFmpeg build for Windows...
echo Please wait...
echo.

set "ZIP_URL=https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n7.1-latest-win64-gpl-7.1.zip"
set "ZIP_PATH=bin\ffmpeg_temp.zip"

powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; (New-Object System.Net.WebClient).DownloadFile('%ZIP_URL%', '%ZIP_PATH%')"

if not exist "%ZIP_PATH%" (
    echo [ERROR] Download failed. Please check your internet connection.
    pause
    exit /b 1
)

echo Extracting FFmpeg binaries...
powershell -Command "Expand-Archive -Path '%ZIP_PATH%' -DestinationPath 'bin\temp_extract' -Force"

echo Moving ffmpeg.exe and ffprobe.exe to bin/...
for /f "delims=" %%i in ('dir /b /s "bin\temp_extract\ffmpeg.exe"') do copy /y "%%i" "bin\ffmpeg.exe" >nul
for /f "delims=" %%i in ('dir /b /s "bin\temp_extract\ffprobe.exe"') do copy /y "%%i" "bin\ffprobe.exe" >nul

echo Cleaning up temporary download files...
del /q "%ZIP_PATH%" 2>nul
rmdir /s /q "bin\temp_extract" 2>nul

if exist bin\ffmpeg.exe (
    if exist bin\ffprobe.exe (
        echo.
        echo ========================================================
        echo  SUCCESS: Portable FFmpeg installed into bin/
        echo  You can now run update_streams.exe and server.exe!
        echo ========================================================
    )
) else (
    echo [ERROR] Extraction failed. Please manually copy ffmpeg.exe and ffprobe.exe into bin/ folder.
)

pause
