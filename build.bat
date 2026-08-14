@echo off
echo ========================================================
echo  BUILDING ETS2 AAC TO MP3 RADIO SERVER EXECUTABLES
echo ========================================================
echo.

where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] MinGW g++ compiler not found in PATH.
    echo Please install MinGW-w64 or run from a developer command prompt.
    pause
    exit /b 1
)

echo Compiling update_streams.exe...
g++ -O2 -std=c++17 src/update_streams.cpp -o update_streams.exe
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile update_streams.exe!
    pause
    exit /b 1
)
echo [SUCCESS] update_streams.exe compiled cleanly.

echo.
echo Compiling server.exe...
g++ -O2 -std=c++17 src/server.cpp -o server.exe -lws2_32
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile server.exe!
    pause
    exit /b 1
)
echo [SUCCESS] server.exe compiled cleanly.

echo.
echo ========================================================
echo  BUILD COMPLETE! Executables are ready for plug-and-play.
echo ========================================================
pause
