@echo off
setlocal
echo ======================================================================
echo  EnergyControl: Building Native High-Performance Multi-threaded GUI
echo ======================================================================

g++ -O2 -std=c++20 main.cpp src/core/*.cpp src/hardware/*.cpp src/gui/*.cpp -lsetupapi -lpowrprof -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -lcrypt32 -mwindows -o EnergyControl.exe

if errorlevel 1 goto :FAILED

echo.
echo [SUCCESS] EnergyControl.exe built successfully!
echo Architecture: Multi-threaded (Background Telemetry Worker + 60 FPS Smooth UI)
echo System Tray: Left-Click 1-Click Toggle + Right-Click Context Menu
echo Universal Hardware Detection: SMBIOS + Registry + NVML + Win32 IOCTL
goto :END

:FAILED
echo.
echo [ERROR] Build failed! Check compiler errors above.
goto :END

:END
echo.
pause
