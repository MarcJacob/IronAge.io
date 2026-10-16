:: Launches a single server instance, at a pre-set path.
:: Further args are passed to the server as-is.
:: At the time of writing this:
:: -port=<Num> = Listen port for game server. 8080 by default.
:: @TODO(Marc): Add more CLA + robust config system.

@echo off
setlocal

set "EXIT_CODE=0"

echo Args = %*

if not defined IRONAGE_DEV_SETUP (
    set "PAUSE_ON_END=1"
    call "%~dp0win32_dev_env_setup.cmd" || (set "EXIT_CODE=1" & goto :end)
)

echo Launching game server...

set "GAME_SERVER_EXE_PATH=%PROJECT_ROOT%build\Win32_Game_Server\win32_game_server.exe"
echo Using default Win32 Game Server path = %GAME_SERVER_EXE_PATH%

if not exist "%GAME_SERVER_EXE_PATH%" (
    echo Could not find Game Server executable at path "%GAME_SERVER_EXE_PATH%".
    set "EXIT_CODE=6"
    goto :end
)

for %%i in ("%GAME_SERVER_EXE_PATH%") do set "GAME_SERVER_DIR=%%~dpi"

:: Launch with cd = exe file.
pushd "%GAME_SERVER_DIR%"
echo "%GAME_SERVER_EXE_PATH%" %*
"%GAME_SERVER_EXE_PATH%" %*
set "EXIT_CODE=%errorLevel%"
popd

:end
if "%PAUSE_ON_END%"=="1" pause

endlocal & exit /b %EXIT_CODE%
