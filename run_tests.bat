@echo off
setlocal
echo ======================================================================
echo  Lenovo EnergyControl: Building and Running Automated Unit Tests
echo ======================================================================

g++ -O2 -std=c++20 tests/TestMain.cpp src/core/*.cpp src/hardware/*.cpp src/gui/TrayManager.cpp -lsetupapi -lpowrprof -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -lcrypt32 -ldbghelp -o run_tests.exe

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
