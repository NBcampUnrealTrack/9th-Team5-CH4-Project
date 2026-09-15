@echo off
setlocal
rem Place this file beside BuildDedicatedServer.ps1.
for %%I in ("%~dp0..") do set "PROJECT_ROOT=%%~fI"
set "ENGINE_ROOT=C:\Program Files\Epic Games\UE_5.7"
if exist "%PROJECT_ROOT%\DedicatedServer\EnginePath.txt" set /p ENGINE_ROOT=<"%PROJECT_ROOT%\DedicatedServer\EnginePath.txt"
if not exist "%~dp0BuildDedicatedServer.ps1" (
    echo BuildDedicatedServer.ps1 not found beside this file.
    pause
    exit /b 1
)
rem Build the launcher only; do not package the dedicated game server.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0BuildDedicatedServer.ps1" -EngineRoot "%ENGINE_ROOT%" -LauncherOnly
set "BUILD_RESULT=%ERRORLEVEL%"
echo.
if "%BUILD_RESULT%"=="0" (
    echo Build complete: %PROJECT_ROOT%\DedicatedServer\StartRoomMaster.exe
) else (
    echo Build failed. Check the error above.
)
pause
exit /b %BUILD_RESULT%
