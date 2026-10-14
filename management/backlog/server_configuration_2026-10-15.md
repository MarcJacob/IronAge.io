# Server configurability

Input date: 2026-10-15

## Item

General server configuration through a config file and / or command line parameters,
ideally plugged into the admin command system (`work_units/server_dev_mode_2026-10-03.md`)
so settings can also be read / changed at runtime where it makes sense.

## Why

Currently hardcoded in the win32 game server net component:
- Listen address (loopback only until 2026-10-15, now all interfaces) and port 8000.
- Candidates found by the listen socket audit: socket reuse options, timeouts (e.g. 2 s
  HTTP idle), client / connection limits (web server 64, platform 1024).
- Wanted: loopback by default for local dev, public bind on request.

## Notes

- Server has no authentication; exposing it publicly is open to anyone for now.
