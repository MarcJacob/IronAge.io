@echo off
rem Assembles web client files into /game_server_resources/ from src\client_web\web and the built wasm.
rem Optional arg 1: path to the .wasm (CMake passes it). Without args, uses build\web_client_debug\wasm and pauses.
setlocal
set "ROOT=%~dp0"
set "WEB_SRC=%ROOT%src\platform\web_game_client\web"
set "TS_SRC=%ROOT%src\platform\web_game_client\front"
set "OUT=%ROOT%game_server_resources\web_root\"

if "%~1"=="" (
    set "WASM=%ROOT%build\web_client_debug\IronAgeIO_WebClient.wasm"
) else (
    set "WASM=%~1"
)

if not exist "%WEB_SRC%" (
    echo ERROR: web sources not found at %WEB_SRC%
    goto :fail
)

rem Compile the web client's TypeScript (front\*.ts -> web\src\*.js) before copying sources.
where npm >nul 2>nul
if errorlevel 1 (
    echo ERROR: npm not found on PATH. Install Node.js, then run "npm install" in %TS_SRC%.
    goto :fail
)
if not exist "%TS_SRC%\node_modules" (
    echo ERROR: %TS_SRC%\node_modules not found. Run "npm install" in %TS_SRC% once, then retry.
    goto :fail
)

pushd "%TS_SRC%"
call npm run build
set "TSC_ERR=%errorlevel%"
popd
if not "%TSC_ERR%"=="0" (
    echo ERROR: TypeScript build failed.
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
