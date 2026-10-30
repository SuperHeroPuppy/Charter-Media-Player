@echo off
setlocal
cd /d "%~dp0"

call "scripts\build-program.bat"
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "scripts\build-installer.ps1" -SkipBuild
if errorlevel 1 exit /b 1

echo.
echo Both release files are ready in build:
echo   CharterMediaPlayer.exe
echo   CharterMediaPlayer-1.1.0-Setup.exe
