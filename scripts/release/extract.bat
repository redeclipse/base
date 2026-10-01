@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0@NAME@-extract.ps1"
if errorlevel 1 (
    echo Extraction failed. Keep this window open to read the error.
    pause
    exit /b 1
)
pause
