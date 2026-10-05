:: Complete build pass for the Win32 platform, including the Web Game Client.

@echo off

if not defined IRONAGE_DEV_SETUP (
    call "%~dp0win32_dev_env_setup.cmd" || exit /b 1
    set "PAUSE_ON_END=1"
)

setlocal

if errorLevel 1 (
    echo Error setting up dev environment. Aborting...
    pause
    exit /b %errorLevel%
)

echo Starting Win32 Full Build.

:: Start Win32 Game Server build to default output path.
echo Building Win32 Game Server...
call "%APP_BUILD_WIN32_GAME_SERVER%"

if errorLevel 1 (
    echo Error building Win32 Game Server. Aborting...
    pause
    endlocal
    exit /b %errorLevel%
)

echo Building Web Game Client...
call "%APP_BUILD_WEB_GAME_CLIENT%"

if errorLevel 1 (
    echo Error building Web Game Client. Aborting...
    pause
    endlocal
    exit /b %errorLevel%
)

echo Full build successful.
endlocal

if "%PAUSE_ON_END%"=="1" (
    pause
)

exit /b 0
