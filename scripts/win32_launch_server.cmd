:: Launches the game server.
:: Arg %1 = game server executable location.
:: Arg %2 = game server resources path (absolute or relative to CD)

@echo off
if not defined IRONAGE_DEV_SETUP (
    call "%~dp0win32_dev_env_setup.cmd" || exit /b 1
    set "PAUSE_ON_END=1"
)

setlocal

echo Launching game server...

set "GAME_SERVER_EXE_PATH=%PROJECT_ROOT%build\Win32_Game_Server\win32_game_server.exe"
if not "%~1"=="" (
    set "GAME_SERVER_EXE_PATH=%~1%"
    echo Using Game Server executable path = %GAME_SERVER_EXE_PATH%
) else echo Using default Win32 Game Server path = %GAME_SERVER_EXE_PATH%

if not exist "%GAME_SERVER_EXE_PATH%" (
    echo Could not find Game Server executable at provided path.
    exit /b 6
)

for %%i in ("%GAME_SERVER_EXE_PATH%") do set "GAME_SERVER_DIR=%%~dpi"

set "GAME_SERVER_RESOURCES_DIR=%GAME_SERVER_DIR%game_server_resources\"
if not "%~2"=="" (
    set "GAME_SERVER_RESOURCES_DIR=%~2"
    echo Using Resources path = %GAME_SERVER_RESOURCES%
) else echo Using default Resources path = %GAME_SERVER_RESOURCES_DIR%"

call %GAME_SERVER_EXE_PATH% %GAME_SERVER_RESOURCES_DIR%

endlocal

if "%PAUSE_ON_END%"=="1" (
    pause
)

exit /b 0
