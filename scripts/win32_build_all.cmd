:: Complete build pass for the Win32 platform, including the Web Game Client.

call "./win32_dev_env_setup.cmd"

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

echo Full build successful.

endlocal

pause
exit /b 0
