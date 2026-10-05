:: Builds the platform-independent Game Server code as a dynamic-link-library for use by the win32 app.

@echo off

if not defined IRONAGE_DEV_SETUP (
    echo IronAge Dev Environment was not setup.
    exit /b 1
)

setlocal

echo Building Win32 Game Server DLL....

set "SCRIPT_FOLDER=%~dp0"
set "OUTPUT_FOLDER=%PROJECT_ROOT%build\Win32_Game_Server\"
if not "%~1"=="" set "OUTPUT_FOLDER=%~f1\"
set "OUTPUT=%OUTPUT_FOLDER%game_server.dll"
echo Using build output path = %OUTPUT%

:: Run build.

:: Include folders

set "INCLUDES="
set "INCLUDES=%INCLUDES% -I%PROJECT_ROOT%/include/"

:: Compilation flags

set "COMPILER_FLAGS="
set "COMPILER_FLAGS=%COMPILER_FLAGS% -fno-exceptions"
set "COMPILER_FLAGS=%COMPILER_FLAGS% -fno-rtti"
set "COMPILER_FLAGS=%COMPILER_FLAGS% -nostdinc"
set "COMPILER_FLAGS=%COMPILER_FLAGS% --debug"
set "COMPILER_FLAGS=%COMPILER_FLAGS% -shared"

:: Warnings policy: all are enabled with some exceptions,
:: all are considered errors.

set "WARNINGS=-W -Werror"
set "WARNINGS=%WARNINGS% -Wno-varargs"
set "WARNINGS=%WARNINGS% -Wno-unused-parameter"
set "WARNINGS=%WARNINGS% -Wno-missing-field-initializers"

:: Link flags

set "LINKER_FLAGS="
set "LINKER_FLAGS=%LINKER_FLAGS% -fuse-ld=lld"

:: Get output folder and create it if it doesn't exist.
for %%i in ("%OUTPUT%") do set "OUTPUT_DIR=%%~dpi"
if not exist %OUTPUT_DIR% mkdir %OUTPUT_DIR% || (
    endlocal
    exit /b 1
)

set "OUTPUT=-o%OUTPUT%"

echo on
clang++ "%SRC_GAME_SERVER_COMMON%"^
%COMPILER_FLAGS%^
%INCLUDES% ^
%WARNINGS% ^
%OUTPUT% ^
%LINKER_FLAGS%
echo off

endLocal

if errorLevel 1 (
    echo Game Server DLL build ended with errors.
    exit /b %errorLevel%
)

echo Game Server DLL build successful.

exit /b 0
