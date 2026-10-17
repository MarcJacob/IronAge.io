# Linux game server platform

Input date: 2026-10-17

## Item

Linux platform layer for the Game Server (sockets / threads / file access / logging / hot reload / process args), plus Linux build and release scripts.

## Why

- Windows VPSs cost much more than Linux ones; a Linux port lowers the hosting cost.
- Public hosting currently targets a Windows box (`public_hosting_2026-10-15.md`).

## Notes

- Supervisor and release workflow are Windows-only today (PowerShell script, `windows-latest` runner, `win32_*.cmd`).
