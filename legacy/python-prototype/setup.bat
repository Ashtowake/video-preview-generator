@echo off
setlocal

echo ====================================
echo   Python Environment Setup Script
echo ====================================

REM Check if Python is available
where python >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Python is not found in PATH.
    pause
    exit /b 1
)

echo [1/4] Creating virtual environment...
python -m venv venv
if errorlevel 1 (
    echo [ERROR] Failed to create virtual environment.
    pause
    exit /b 1
)

echo [2/4] Upgrading pip...
call venv\Scripts\python.exe -m pip install --upgrade pip
if errorlevel 1 (
    echo [ERROR] Failed to upgrade pip.
    pause
    exit /b 1
)

echo [3/4] Installing requirements...
if exist requirements.txt (
    call venv\Scripts\python.exe -m pip install -r requirements.txt
    if errorlevel 1 (
        echo [ERROR] Failed to install requirements.
        pause
        exit /b 1
    )
) else (
    echo [WARNING] requirements.txt not found, skipping package installation.
)

echo [4/4] Setup complete.

REM Optional: Launch script
REM echo Launching application...
REM start "" venv\Scripts\pythonw.exe main.py

pause
