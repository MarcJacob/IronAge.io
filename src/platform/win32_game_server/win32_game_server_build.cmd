:: Builds the Win32 Game Server app and outputs it to the path provided as first parameter, or in <Project Root/Builds/Win32_Game_Server/>
:: @TODO Better parameterization to support debug vs release, and maybe grabbing the web content it has to serve.

@echo off

if not defined IRONAGE_DEV_SETUP (
    echo IronAge Dev Environment was not setup.
    exit /b 1
)


setlocal

set "SCRIPT_FOLDER=%~dp0"

:: if -hotreload parameter was just call dll build and pass it second parameter, then stop.
if "%~1"=="-hotreload" (
    call "%SCRIPT_FOLDER%win32_game_server_build_game_server_dll.cmd" %2 || exit /b
    endlocal
    exit /b 0
)

:: Start with rebuilding the platform-independent Game Server code.
:: (bare "exit /b" keeps the DLL script's error code)
call "%SCRIPT_FOLDER%win32_game_server_build_game_server_dll.cmd" %1 || exit /b

set "OUTPUT_FOLDER=%PROJECT_ROOT%build\Win32_Game_Server\"
if not "%~1"=="" set "OUTPUT_FOLDER=%~f1\"
set "OUTPUT=%OUTPUT_FOLDER%win32_game_server.exe"
echo Using build output path = %OUTPUT%

:: Run build.

:: Include folders

set "INCLUDES="
set "INCLUDES=%INCLUDES% -I%PROJECT_ROOT%/include/"

:: Compilation flags

set "COMPILER_FLAGS="
set "COMPILER_FLAGS=%COMPILER_FLAGS% -g"
set "COMPILER_FLAGS=%COMPILER_FLAGS% -O0"

:: Warnings policy: all are enabled with some exceptions,
:: all are considered errors.
set "WARNINGS=-W -Werror"
set "WARNINGS=%WARNINGS% -Wno-varargs"
set "WARNINGS=%WARNINGS% -Wno-unused-parameter"
set "WARNINGS=%WARNINGS% -Wno-missing-field-initializers"

:: Link flags

set "LIBRARIES="
set "LIBRARIES=%LIBRARIES% -lKernel32.lib"
set "LIBRARIES=%LIBRARIES% -lws2_32.lib"

:: Get output folder and create it if it doesn't exist.
for %%i in ("%OUTPUT%") do set "OUTPUT_DIR=%%~dpi"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%" || (
    endlocal
    exit /b 1
)

set "OUTPUT=-o%OUTPUT%"

clang++ "%SCRIPT_FOLDER%win32_game_server_main.cpp"^
%COMPILER_FLAGS%^
%INCLUDES% ^
%WARNINGS% ^
%LIBRARIES% ^
%OUTPUT%

endLocal

if errorLevel 1 (
    echo Win32 Game Server build ended with errors.
    exit /b %errorLevel%
)

echo Win32 Game Server build successful.

exit /b 0
