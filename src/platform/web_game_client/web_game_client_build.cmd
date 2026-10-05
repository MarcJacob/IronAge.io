:: Performs a build of the WASM client and TS sources and deploys them 
:: along with the contents of the /web/ directory into the target directory,
:: forming a fully serveable game client over the web within that directory.

@echo off

if not defined IRONAGE_DEV_SETUP (
    echo IronAge Dev Environment was not setup.
    exit /b 1
)

setlocal

set "SCRIPT_FOLDER=%~dp0"
set "OUTPUT=%PROJECT_ROOT%build\Win32_Game_Server\game_server_resources\web_root\IronAgeIO_WebClient.wasm"

if not "%~1"=="" (
    set "OUTPUT=%~dp1"
    echo Using build output path = %OUTPUT%
) else echo Using default output path = %OUTPUT%

:: Extract WASM file name and output folder separately.
for %%i in ("%OUTPUT%") do set "OUTPUT_FILENAME=%%~ni"
for %%i in ("%OUTPUT%") do set "OUTPUT_DIR=%%~dpi"

:: Run build.

:: Include folders

set "INCLUDES="
set "INCLUDES=%INCLUDES% -I%PROJECT_ROOT%/include/"

:: Compilation flags

set "COMPILER_FLAGS="
set "COMPILER_FLAGS=%COMPILER_FLAGS% --target=wasm32-unknown-unknown"
set "COMPILER_FLAGS=%COMPILER_FLAGS% -ffreestanding"
set "COMPILER_FLAGS=%COMPILER_FLAGS% -fno-exceptions"
set "COMPILER_FLAGS=%COMPILER_FLAGS% -fno-rtti"
set "COMPILER_FLAGS=%COMPILER_FLAGS% -fvisibility=hidden"
set "COMPILER_FLAGS=%COMPILER_FLAGS% -nostdinc -nostdlibinc"

:: Warnings policy: all are enabled with some exceptions,
:: all are considered errors.
:: TODO(Marc): Common flags defined in dev env setup ?
set "WARNINGS=-W -Werror"
set "WARNINGS=%WARNINGS% -Wno-varargs" :: Disable varargs
set "WARNINGS=%WARNINGS% -Wno-unused-parameter" :: Disable unused parameter.
set "WARNINGS=%WARNINGS% -Wno-missing-field-initializers" :: Disable missing field initializers.

:: Link flags

set "LIBRARIES="

set "LINKER_FLAGS="
set "LINKER_FLAGS=%LINKER_FLAGS% -Wl,--no-entry"
set "LINKER_FLAGS=%LINKER_FLAGS% -Wl,--export-dynamic"
set "LINKER_FLAGS=%LINKER_FLAGS% -nostdlib"
set "LINKER_FLAGS=%LINKER_FLAGS% -fuse-ld=lld"

@echo on
:: Build & output WASM to staging Web folder.
clang++ "%SCRIPT_FOLDER%web_game_client_main.cpp" ^
%COMPILER_FLAGS% ^
%INCLUDES% ^
%WARNINGS% ^
-o"%SCRIPT_FOLDER%/web/%OUTPUT_FILENAME%.wasm" ^
%LIBRARIES% ^
%LINKER_FLAGS%
@echo off


if errorLevel 1 (
    echo Web Game Client WASM build ended with errors.
    endlocal
    exit /b %errorLevel%
)

:: Perform TSC build of front sources into /web/src then copy the whole /web/ folder where the .wasm was output.
:: The TSC build configuration can be found in the settings.json file in /front/

pushd "%SCRIPT_FOLDER%front/"
call npm run build
popd

if errorLevel 1 (
    echo Web Game Client TSC build ended with errors.
    endlocal
    exit /b %errorLevel%
)

:: /Web/ -> Output dir copy

:: Perform copy
echo Deploying client files to %OUTPUT_DIR%...
robocopy %SCRIPT_FOLDER%/web/ %OUTPUT_DIR% /R:1 /W:1 /E /MIR /NJH /NJS /NP /NDL

if errorLevel 8 (
    echo Web Game Client copy deployment to %OUTPUT_DIR% failed with error %errorLevel%.
    endlocal
    exit /b %errorLevel%
)

echo "Web Game Client build & copy successful."

endlocal
exit /b 0
