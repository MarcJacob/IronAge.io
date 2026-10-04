:: Builds the Win32 Game Server app and outputs it to the path provided as first parameter, or in <Project Root/Builds/Win32_Game_Server/>
:: @TODO Better parameterization to support debug vs release, and maybe grabbing the web content it has to serve.

@echo off

if not defined IRONAGE_DEV_SETUP (
    echo IronAge Dev Environment was not setup.
    exit /b 1
)

setlocal

set "SCRIPT_FOLDER=%~dp0"
set "OUTPUT=%PROJECT_ROOT%build\Win32_Game_Server\win32_game_server.exe"

if not "%~1"=="" (
    set "OUTPUT_FOLDER=%~1"
    echo Using build output path = %OUTPUT%
) else echo Using default build output path = %OUTPUT%

:: Run build.

:: Include folders

set "INCLUDES="
set "INCLUDES=%INCLUDES% -I%PROJECT_ROOT%/include/"

:: Compilation flags

set "COMPILER_FLAGS="
set "COMPILER_FLAGS=%COMPILER_FLAGS% --debug

:: Warnings policy: all are enabled with some exceptions,
:: all are considered errors.
set "WARNINGS=-W -Werror"
set "WARNINGS=%WARNINGS% -Wno-varargs" :: Disable varargs
set "WARNINGS=%WARNINGS% -Wno-unused-parameter" :: Disable unused parameter.
set "WARNINGS=%WARNINGS% -Wno-missing-field-initializers" :: Disable missing field initializers.

:: Link flags

set "LIBRARIES="
set "LIBRARIES=%LIBRARIES% -lws2_32.lib"

:: Get output folder and create it if it doesn't exist.
for %%i in ("%OUTPUT%") do set "OUTPUT_DIR=%%~dpi"
if not exist %OUTPUT_DIR% mkdir %OUTPUT_DIR% || (
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
