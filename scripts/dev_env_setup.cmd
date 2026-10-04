:: Sets up a set of useful environment variable in the caller process.
:: Called by most, if not all, Win32 scripts in the project.

@echo off
:: @TODO

echo Loading IronAge.IO project dev env variables.

:: Set major path variables.

set "PROJECT_ROOT=%~dp0..\"
echo PROJECT_ROOT = "%PROJECT_ROOT%"

set "SCRIPTS_DIR=%PROJECT_ROOT%Scripts\"
echo SCRIPTS_DIR = "%SCRIPTS_DIR%"

:: APPS - Base main files to compile to get a specific app on a specific platform.
:: Check specific app requirements to see what they need to be linked with and when.

:: Win32 Game Server main source file.

set "APP_SRC_WIN32_GAME_SERVER=%PROJECT_ROOT%src\platform\win32_game_server\win32_game_server_main.cpp"
echo %APP_SRC_WIN32_GAME_SERVER%
if not exist %APP_SRC_WIN32_GAME_SERVER% (
    echo Error: Win32 Game Server main source file not found !
    exit /b 6
)
echo APP_SRC_WIN32_GAME_SERVER = "%APP_SRC_WIN32_GAME_SERVER%"

:: Web Game Client main WASM source file.

set "APP_SRC_WEB_GAME_CLIENT=%PROJECT_ROOT%src\platform\web_game_client\wasm_client_main.cpp"
if not exist %APP_SRC_WEB_GAME_CLIENT% (
    echo Error: Web Game Client main source file not found !
    exit /b 6
)
echo APP_SRC_WEB_GAME_CLIENT = "%APP_SRC_WEB_GAME_CLIENT%"

:: PLATFORM INDEPENDENT CODE - Pure libraries made to be added to one of the apps.
:: Depending on the app they can be unity-compiled already in the source code, or expected
:: to be statically linked or dynamically linked.

:: Game Server platform independent code main source file.

set "SRC_GAME_SERVER_COMMON=%PROJECT_ROOT%src\game_server\game_server_main.cpp"
if not exist %SRC_GAME_SERVER_COMMON% (
    echo Error: Game Server Common main source file not found !
    exit /b 6
)
echo SRC_GAME_SERVER_COMMON = "%SRC_GAME_SERVER_COMMON%"

:: Game Client backend platform independent code main source file.

set "SRC_GAME_CLIENT_COMMON=%PROJECT_ROOT%src\game_client\game_client_main.cpp"
if not exist %SRC_GAME_CLIENT_COMMON% (
    echo Error: Game Client Common main source file not found !
    exit /b 6
)
echo SRC_GAME_CLIENT_COMMON = "%SRC_GAME_CLIENT_COMMON%"

:: Game Common main source file (required by all game apps).

set "SRC_GAME_COMMON=%PROJECT_ROOT%src\game_common\game_common_main.cpp"
if not exist %SRC_GAME_COMMON% (
    echo Error: Game Common main source file not found !
    exit /b 6
)
echo SRC_GAME_COMMON = "%SRC_GAME_COMMON%"

