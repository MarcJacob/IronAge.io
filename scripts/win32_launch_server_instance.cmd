:: Launches a single server instance.
:: Arg %1 = game server executable location.
:: Further args are passed to the server as-is.
:: At the time of writing this:
:: -port=<Num> = Listen port for game server. 8080 by default.
:: @TODO(Marc): Add more CLA + robust config system.

@echo off
if not defined IRONAGE_DEV_SETUP (
    call "%~dp0win32_dev_env_setup.cmd" || exit /b 1
    set "PAUSE_ON_END=1"
)

setlocal

echo Launching game server...

set "GAME_SERVER_EXE_PATH=%PROJECT_ROOT%build\Win32_Game_Server\win32_game_server.exe"
if not "%~1"=="" (
    set "GAME_SERVER_EXE_PATH=%~1"
    echo Using Game Server executable path = %GAME_SERVER_EXE_PATH%
) else echo Using default Win32 Game Server path = %GAME_SERVER_EXE_PATH%

if not exist "%GAME_SERVER_EXE_PATH%" (
    echo Could not find Game Server executable at provided path.
    exit /b 6
)

for %%i in ("%GAME_SERVER_EXE_PATH%") do set "GAME_SERVER_DIR=%%~dpi"

:: Launch with cd = exe file.
pushd %GAME_SERVER_DIR%
call %GAME_SERVER_EXE_PATH% %*
popd

endlocal

if "%PAUSE_ON_END%"=="1" (
    pause
)

exit /b 0
