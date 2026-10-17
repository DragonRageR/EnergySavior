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
title EnergySavior - AutoStart Registration (Administrator)

echo ======================================================================
echo  EnergySavior: AutoStart Registration (Administrator)
echo ======================================================================
echo.

set "TARGET=%~dp0EnergySavior.exe"

if not exist "%TARGET%" (
    echo [ERROR] EnergySavior.exe not found at:
    echo "%TARGET%"
    echo.
    pause
    exit /b 1
)

echo 1. Registering in Windows Task Manager Startup (HKCU Run key)...
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "EnergySavior" /t REG_SZ /d "\"%TARGET%\" --minimized" /f >nul
rem Remove legacy keys
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "EnergyControl" /f >nul 2>nul
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "LenovoEnergyControl" /f >nul 2>nul

echo 2. Registering in Windows Task Scheduler (Highest Privileges / On Logon)...
schtasks /create /tn "EnergySavior" /tr "\"%TARGET%\" --minimized" /rl HIGHEST /sc ONLOGON /f >nul
rem Remove legacy scheduled task
schtasks /delete /tn "LenovoEnergyControl" /f >nul 2>nul

if %ERRORLEVEL% EQU 0 (
    powershell -NoProfile -Command "Set-ScheduledTask -TaskName 'EnergySavior' -Settings (New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit 0)" >nul 2>nul

    echo.
    echo ======================================================================
    echo  [SUCCESS] EnergySavior successfully registered for Auto-Start
    echo ======================================================================
    echo.
    echo  - Appears in Task Manager Startup tab
    echo  - Runs automatically with Windows
    echo  - Operates with Highest Administrator Privileges
    echo  - No UAC prompts on logon
    echo  - Enabled for battery power operation
    echo  - Starts minimized directly into the System Tray
) else (
    echo.
    echo ======================================================================
    echo  [ERROR] Failed to register Task Scheduler entry.
    echo ======================================================================
)

echo.
pause
