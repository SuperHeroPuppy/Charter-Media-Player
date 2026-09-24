@echo off
setlocal
cd /d "%~dp0"

echo Building Charter Music Browser...

if not exist build mkdir build

windres resources\CharterMusicBrowser.rc -I resources -O coff -o build\CharterMusicBrowser_resources.o
if errorlevel 1 (
    echo.
    echo Resource compilation failed.
    exit /b 1
)

gcc src\main.c build\CharterMusicBrowser_resources.o -Isrc -Iresources -O2 -std=c11 -Wall -Wextra -Wpedantic -municode -mwindows ^
    -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm -ldwmapi -luxtheme -lurlmon -lmfplat -lmfreadwrite -lmfplay -lmfuuid -lpropsys -lmsimg32 ^
    -o build\CharterMusicBrowser.exe

set BUILD_RESULT=%ERRORLEVEL%
del /q build\CharterMusicBrowser_resources.o >nul 2>nul

if not "%BUILD_RESULT%"=="0" (
    echo.
    echo Build failed.
    exit /b %BUILD_RESULT%
)

echo.
echo Build complete: build\CharterMusicBrowser.exe
