@echo off
rem Temporary dev web server over build\web_client\dist (placeholder until the Game Server serves it).
set "OUT=%~dp0build\web_client\dist"

if not exist "%OUT%" (
    echo ERROR: %OUT% not found. Run deploy_web_client.bat first.
    pause
    exit /b 1
)

echo Serving %OUT% on http://localhost:8000 (Ctrl+C to stop)
python -m http.server 8000 --directory "%OUT%"
