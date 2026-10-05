:: Starts local dev environment for the project, launching Neovim with a local project configuration and the dev env variables loaded in.

:: First setup dev env variables
call "%~dp0"scripts/win32_dev_env_setup.cmd

if errorLevel 1 (
    echo Error setting up dev env.
    exit /b 1
)

:: Then open nvim
nvim ./ -u .nvim/project_config.lua
