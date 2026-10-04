# Dev iteration loop

Input date: 2026-10-03

## Item

One command (dev console or .bat) that builds and deploys everything (except
the running platform code), has the platform hot-reload and re-init the server
code, and tells the server to reload its http-serveable resources.

- Extend deploy_web_client.bat to trigger the resource reload (dev mode message).
- Possible follow-up: Visual Studio -> VIM once the build is simple enough.

## Depends on

`build_system_revamp_2026-10-03.md`, `server_dev_mode_2026-10-03.md`.

## Next step

Not started.
