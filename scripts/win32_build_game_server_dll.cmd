:: Builds the Platform-independent Game Server code as a .dll that can be loaded by the win32 platform app.

@echo off

if not defined IRONAGE_DEV_SETUP (
    call "%~dp0win32_dev_env_setup.cmd" || exit /b 1
    set "PAUSE_ON_END=1"
)

setlocal

if errorLevel 1 (
    echo Error setting up dev environment. Aborting...
    goto :end
)

:: Start Win32 Game Server build to default output path.
echo Building Win32 Game Server...
call "%APP_BUILD_WIN32_GAME_SERVER%" -hotreload

if errorLevel 1 (
    echo Error building Game Server DLL.
    goto :end
)

set "%errorLevel%=0"

:end
if "%PAUSE_ON_END%"=="1" pause
endlocal
exit /b %errorLevel%
