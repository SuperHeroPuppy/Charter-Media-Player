@echo off
setlocal
cd /d "%~dp0"

echo Building Charter Media Player...

where gcc >nul 2>nul
if errorlevel 1 if exist "C:\msys64\ucrt64\bin\gcc.exe" set "PATH=C:\msys64\ucrt64\bin;%PATH%"

where gcc >nul 2>nul
if errorlevel 1 (
    echo.
    echo GCC was not found. Install the MSYS2 UCRT64 toolchain or run this file
    echo from an MSYS2 UCRT64 shell.
    exit /b 1
)

if not exist build mkdir build

windres resources\CharterMediaPlayer.rc -I resources -O coff -o build\CharterMediaPlayer_resources.o
if errorlevel 1 (
    echo.
    echo Resource compilation failed.
    exit /b 1
)

gcc src\main.c build\CharterMediaPlayer_resources.o -Isrc -Iresources -O2 -std=c11 -Wall -Wextra -Wpedantic -municode -mwindows ^
    -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm -ldwmapi -luxtheme -lurlmon -lmfplat -lmfreadwrite -lmfplay -lmfuuid -lpropsys -lmsimg32 ^
    -o build\CharterMediaPlayer.exe

set BUILD_RESULT=%ERRORLEVEL%
del /q build\CharterMediaPlayer_resources.o >nul 2>nul

if not "%BUILD_RESULT%"=="0" (
    echo.
    echo Build failed.
    exit /b %BUILD_RESULT%
)

echo.
echo Build complete: build\CharterMediaPlayer.exe
