@echo off
setlocal

:: Check for Administrator privileges
net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [Requesting Administrator Elevation...]
    powershell -NoProfile -Command "Start-Process cmd.exe -ArgumentList '/c \"\"%~f0\"\"' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"
title EnergySavior - AutoStart Removal

echo ======================================================================
echo  Removing EnergySavior AutoStart entries...
echo ======================================================================
echo.

echo 1. Removing from Windows Task Manager Startup (HKCU Run key)...
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "EnergySavior" /f >nul 2>nul
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "EnergyControl" /f >nul 2>nul
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "LenovoEnergyControl" /f >nul 2>nul

echo 2. Removing from Task Scheduler...
schtasks /delete /tn "EnergySavior" /f >nul 2>nul
schtasks /delete /tn "LenovoEnergyControl" /f >nul 2>nul

echo.
echo ======================================================================
echo  [SUCCESS] EnergySavior auto-start entries have been removed.
echo ======================================================================
echo.
pause
