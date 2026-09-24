@echo off
setlocal
cd /d "%~dp0"

set "TARGET=build\CharterMediaPlayer.exe"
if not exist "%TARGET%" (
    echo Charter Media Player has not been built yet.
    echo Run build.bat before signing the release.
    exit /b 1
)

if "%CHARTER_SIGNING_THUMBPRINT%"=="" (
    echo CHARTER_SIGNING_THUMBPRINT is not set.
    echo Set it to the SHA-1 thumbprint of the trusted code-signing certificate
    echo installed in the current user's Windows certificate store.
    exit /b 1
)

set "SIGNTOOL_PATH="
for /f "delims=" %%S in ('where signtool.exe 2^>nul') do if not defined SIGNTOOL_PATH set "SIGNTOOL_PATH=%%S"

if not defined SIGNTOOL_PATH if exist "%ProgramFiles(x86)%\Windows Kits\10\bin" (
    for /f "delims=" %%S in ('dir /b /s "%ProgramFiles(x86)%\Windows Kits\10\bin\*\x64\signtool.exe" 2^>nul ^| sort /r') do if not defined SIGNTOOL_PATH set "SIGNTOOL_PATH=%%S"
)

if not defined SIGNTOOL_PATH (
    echo SignTool was not found. Install the Windows SDK and try again.
    exit /b 1
)

if "%CHARTER_TIMESTAMP_URL%"=="" set "CHARTER_TIMESTAMP_URL=http://timestamp.digicert.com"

echo Signing %TARGET% as Super's Network...
"%SIGNTOOL_PATH%" sign /sha1 "%CHARTER_SIGNING_THUMBPRINT%" /fd SHA256 /tr "%CHARTER_TIMESTAMP_URL%" /td SHA256 /d "Charter Media Player" "%TARGET%"
if errorlevel 1 (
    echo.
    echo Signing failed. The executable was not released.
    exit /b 1
)

echo.
echo Verifying Authenticode signature...
"%SIGNTOOL_PATH%" verify /pa /v "%TARGET%"
if errorlevel 1 (
    echo.
    echo Signature verification failed.
    exit /b 1
)

echo.
echo Signed release ready: %TARGET%
