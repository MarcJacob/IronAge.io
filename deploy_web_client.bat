@echo off
rem Assembles build\web_client\dist from src\client_web\web and the built wasm.
rem Optional arg 1: path to the .wasm (CMake passes it). Without args, uses build\wasm and pauses.
setlocal
set "ROOT=%~dp0"
set "WEB_SRC=%ROOT%src\client_web\web"
set "OUT=%ROOT%build\web_client\dist"

if "%~1"=="" (
    set "WASM=%ROOT%build\web_client\IronAgeIO_WebClient.wasm"
) else (
    set "WASM=%~1"
)

if not exist "%WEB_SRC%" (
    echo ERROR: web sources not found at %WEB_SRC%
    goto :fail
)

if not exist "%OUT%" mkdir "%OUT%"

xcopy "%WEB_SRC%" "%OUT%" /E /I /Y /Q >nul
if errorlevel 1 (
    echo ERROR: copying web sources failed.
    goto :fail
)

if exist "%WASM%" (
    copy /Y "%WASM%" "%OUT%\" >nul
    if errorlevel 1 (
        echo ERROR: copying wasm failed.
        goto :fail
    )
) else (
    echo WARNING: wasm not found at %WASM%, keeping any previously deployed copy.
)

echo Deployed web client to %OUT%
if "%~1"=="" pause
exit /b 0

:fail
if "%~1"=="" pause
exit /b 1
