# Server file logging

Input date: 2026-10-17

## Item

Game server writes its log to a file (rotating / bounded), not only to its console.

## Why

The server runs unattended on the hosting box (`work_units/public_hosting_2026-10-15.md`); a crash or odd behavior leaves no trace once the console is gone. The supervisor only logs its own actions (`.logs/supervisor.log`, `build.log`).

## Notes

- May fit with `backlog/server_configuration_2026-10-15.md` (log path / level as config).
