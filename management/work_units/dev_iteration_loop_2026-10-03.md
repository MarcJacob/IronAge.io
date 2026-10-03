# BACKLOG: Dev iteration loop

Input date: 2026-10-03

## Item

A dev console or simple .bat that builds and deploys everything (except the
game server platform code while it is running), has the platform hot-reload
the server code and re-initialize it, and signals the server to reload its
http-serveable resources.

## Description

- Extend deploy_web_client.bat so it also tells the running server to reload
  its resources (uses a dev mode message).
- Build + deploy + hot-reload server code in one command.
- Possible follow-up: switch from Visual Studio to VIM once the build is
  simple enough.

## Why

Speeds up iteration and simplifies the build process.
Depends on `build_system_revamp_2026-10-03.md` (server DLL, static Game
Common) and `server_dev_mode_2026-10-03.md` (resource reload message).
Suggested order: build revamp, dev mode, then this.
