:: Builds the Win32 Game Server app and outputs it to the path provided as first parameter, or in <Project Root/Builds/Win32_Game_Server/>
:: @TODO Better parameterization to support debug vs release, and maybe grabbing the web content it has to serve.

@echo off

goto :skip
if not defined IRONAGE_DEV_SETUP (
    echo IronAge Dev Environment was not setup.
    exit /b 1
)

:skip
:: Run build.

:: Compilation flags

set "INCLUDES="
set "INCLUDES=%INCLUDES% -I../../../include/"

:: Warnings policy: all are enabled with some exceptions,
:: all are considered errors.
set "WARNINGS=-W -Werror"
set "WARNINGS=%WARNINGS% -Wno-varargs" :: Disable varargs
set "WARNINGS=%WARNINGS% -Wno-unused-parameter" :: Disable unused parameter.
set "WARNINGS=%WARNINGS% -Wno-missing-field-initializers" :: Disable missing field initializers.

:: Link flags

set "LIBRARIES="
set "LIBRARIES=%LIBRARIES% -lws2_32.lib"

clang++ win32_game_server_main.cpp %INCLUDES% %WARNINGS% %LIBRARIES%
