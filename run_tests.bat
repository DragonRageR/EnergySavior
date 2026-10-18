@echo off
setlocal
echo ======================================================================
echo  EnergySavior: Building and Running Automated Unit Tests
echo ======================================================================

g++ -O2 -std=c++20 -finput-charset=UTF-8 -fexec-charset=UTF-8 tests/TestMain.cpp src/core/*.cpp src/hardware/*.cpp src/gui/*.cpp -lsetupapi -lpowrprof -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -loleaut32 -lwbemuuid -lcrypt32 -ldbghelp -lwinhttp -lurlmon -lpdh -ldxgi -o run_tests.exe

if errorlevel 1 (
    echo.
    echo [ERROR] Test suite compilation failed!
    exit /b 1
)

echo.
echo [OK] Compilation successful. Running test cases...
echo.

run_tests.exe
set TEST_RESULT=%errorlevel%

del run_tests.exe 2>nul

if %TEST_RESULT% NEQ 0 (
    echo.
    echo [ERROR] Some unit tests failed!
    exit /b %TEST_RESULT%
)

echo.
echo [SUCCESS] All unit tests executed with 100%% success!
exit /b 0
