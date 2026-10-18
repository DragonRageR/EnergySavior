@echo off
setlocal

set MODE=%1
if "%MODE%"=="" set MODE=release

echo ======================================================================
echo  EnergySavior: Native High-Performance Build Engine
echo  Target: %MODE% (C++20 x86-64-v2)
echo ======================================================================

if /I "%MODE%"=="test" (
    call run_tests.bat %*
    if errorlevel 1 goto :FAILED
    goto :END
)

rem 1. Automatically stop running instance to prevent "Permission Denied" file lock
tasklist /fi "imagename eq EnergySavior.exe" 2>nul | find /i "EnergySavior.exe" >nul
if not errorlevel 1 (
    echo [INFO] Stopping running EnergySavior.exe to release file lock...
    taskkill /f /im EnergySavior.exe >nul 2>&1
    timeout /t 1 /nobreak >nul
)
tasklist /fi "imagename eq EnergyControl.exe" 2>nul | find /i "EnergyControl.exe" >nul
if not errorlevel 1 (
    echo [INFO] Stopping running EnergyControl.exe to release file lock...
    taskkill /f /im EnergyControl.exe >nul 2>&1
    timeout /t 1 /nobreak >nul
)


if /I "%MODE%"=="debug" (
    echo [BUILD] Compiling Debug binary with symbols and runtime diagnostics...
    g++ -O0 -g3 -std=c++20 -finput-charset=UTF-8 -fexec-charset=UTF-8 -DDEBUG -Wall -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unused-variable -Wno-unknown-pragmas -Wno-stringop-overread main.cpp src/core/*.cpp src/hardware/*.cpp src/gui/*.cpp -lsetupapi -lpowrprof -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -loleaut32 -lwbemuuid -lcrypt32 -ldbghelp -lwinhttp -lurlmon -mwindows -o EnergySavior_debug.exe
    if errorlevel 1 goto :FAILED
    echo [SUCCESS] EnergySavior_debug.exe generated successfully!
    goto :END
)

rem Default: Release (Equivalent to cargo build --release in Rust)
echo [BUILD] Compiling Release binary: -O3 -flto -flto-partition=none -march=x86-64-v2 -DNDEBUG -s...
g++ -O3 -std=c++20 -finput-charset=UTF-8 -fexec-charset=UTF-8 -flto -flto-partition=none -march=x86-64-v2 -DNDEBUG -s -Wall -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unused-variable -Wno-unknown-pragmas -Wno-stringop-overread main.cpp src/core/*.cpp src/hardware/*.cpp src/gui/*.cpp -lsetupapi -lpowrprof -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -loleaut32 -lwbemuuid -lcrypt32 -ldbghelp -lwinhttp -lurlmon -mwindows -o EnergySavior.exe


if errorlevel 1 goto :FAILED

echo.
echo [SUCCESS] EnergySavior.exe built successfully in Maximum Performance Mode!
echo  - Optimization:        -O3 (Aggressive vectorization ^& pipeline scheduling)
echo  - Link-Time Opt (LTO): Enabled (-flto Link-Time Optimization)
echo  - Architecture:         x86-64-v2 (SSE4.2, PopCount, CMPXCHG16B)
echo  - Symbol Stripping:     Stripped (-s, ultra-compact binary, sub-300 KB)
echo  - Crash Handler:        Integrated SEH + MiniDumpWriteDump (.dmp ^& .log)
echo  - In-App Updates:       Integrated WinHTTP HTTPS Auto-Updater
goto :END

:FAILED
echo.
echo [ERROR] Build failed! Check compiler errors above.
if not "%2"=="--no-pause" (
    echo.
    pause
)
exit /b 1

:END
if "%MODE%"=="release" (
    echo.
    echo To run automated tests:   build.bat test
    echo To compile debug build:   build.bat debug
)

if not "%2"=="--no-pause" (
    echo.
    pause
)
exit /b 0
