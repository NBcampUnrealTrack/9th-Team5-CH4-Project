@echo off
setlocal
rem Build the launcher only; do not package the dedicated game server.
set "PROJECT_ROOT=C:\Users\shs\Documents\Unreal Projects\DeepRaiders"
set "ENGINE_ROOT=C:\Program Files\Epic Games\UE_5.7"
if exist "%PROJECT_ROOT%\DedicatedServer\EnginePath.txt" set /p ENGINE_ROOT=<"%PROJECT_ROOT%\DedicatedServer\EnginePath.txt"
if not exist "%PROJECT_ROOT%\BuildDedicatedServer\BuildDedicatedServer.ps1" (
    echo Build script not found.
    pause
    exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_ROOT%\BuildDedicatedServer\BuildDedicatedServer.ps1" -EngineRoot "%ENGINE_ROOT%" -LauncherOnly
set "BUILD_RESULT=%ERRORLEVEL%"
echo.
if "%BUILD_RESULT%"=="0" (
    echo Build complete: %PROJECT_ROOT%\DedicatedServer\StartRoomMaster.exe
) else (
    echo Build failed. Check the error above.
)
pause
exit /b %BUILD_RESULT%
