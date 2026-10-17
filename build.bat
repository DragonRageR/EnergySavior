@echo off
setlocal

set MODE=%1
if "%MODE%"=="" set MODE=release

echo ======================================================================
echo  Lenovo EnergyControl: Native High-Performance Build Engine
echo  Target: %MODE% (C++20 x86-64-v2)
echo ======================================================================

if /I "%MODE%"=="test" (
    call run_tests.bat
    goto :END
)

if /I "%MODE%"=="debug" (
    echo [BUILD] Compiling Debug binary with symbols and runtime diagnostics...
    g++ -O0 -g3 -std=c++20 -DDEBUG -Wall -Wno-missing-field-initializers -Wno-unused-parameter main.cpp src/core/*.cpp src/hardware/*.cpp src/gui/*.cpp -lsetupapi -lpowrprof -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -lcrypt32 -ldbghelp -lwinhttp -lurlmon -mwindows -o EnergyControl_debug.exe
    if errorlevel 1 goto :FAILED
    echo [SUCCESS] EnergyControl_debug.exe generated successfully!
    goto :END
)

rem Default: Release (Equivalent to cargo build --release in Rust)
echo [BUILD] Compiling Release binary: -O3 -flto -march=x86-64-v2 -DNDEBUG -s...
g++ -O3 -std=c++20 -flto -march=x86-64-v2 -DNDEBUG -s -Wall -Wno-missing-field-initializers -Wno-unused-parameter main.cpp src/core/*.cpp src/hardware/*.cpp src/gui/*.cpp -lsetupapi -lpowrprof -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -lcrypt32 -ldbghelp -lwinhttp -lurlmon -mwindows -o EnergyControl.exe

if errorlevel 1 goto :FAILED

echo.
echo [SUCCESS] EnergyControl.exe built successfully in Maximum Performance Mode!
echo  - Optimization:       -O3 (Aggressive vectorization ^& pipeline scheduling)
echo  - Link-Time Opt (LTO): Enabled (-flto across all translation units)
echo  - Architecture:        x86-64-v2 (SSE4.2, PopCount, CMPXCHG16B)
echo  - Symbol Stripping:    Stripped (-s, ultra-compact binary, sub-200 KB)
echo  - Crash Handler:       Integrated SEH + MiniDumpWriteDump (.dmp ^& .log)
goto :END

:FAILED
echo.
echo [ERROR] Build failed! Check compiler errors above.
exit /b 1

:END
if "%2"=="" (
    if "%MODE%"=="release" (
        echo.
        echo To run automated tests:   build.bat test
        echo To compile debug build:   build.bat debug
    )
)
exit /b 0
