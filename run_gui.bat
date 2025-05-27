@echo off
setlocal

REM Set path to the virtual environment's Python interpreter
set PYTHON_EXEC=venv\Scripts\pythonw.exe

REM Run the script using the venv
start "" "%PYTHON_EXEC%" "main.py"
