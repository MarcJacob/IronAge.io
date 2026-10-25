# Server test mode

Started: 2026-10-25. Supersedes the lobby-control part of `lobby_system_2026-09-27.md` and the auth part of `server_dev_mode_2026-10-03.md` (both stay open for their remaining scope).

## Scope

- Server launch flag turns on test mode.
- New server -> client "authenticated" message on connection, carrying server-wide info (first field: test mode on/off; extensible).
- Test mode lobby: no automatic countdown; admin controls (set player count, start game, sliders...) shown to every client. No security / permission limits at this stage.

## Decisions

- Everyone gets admin controls when the server is in test mode.
- Test mode is a server launch parameter (command line) AND a per-lobby parameter; for now the lobby just follows the server's flag. Per-lobby test lobbies later.
- Admin controls UI is built on the new UI module system (`web_ui_static_modules_2026-10-23.md`).

## Tasks

1. Server: launch flag, "authenticated" message, test-mode lobby behavior, admin commands (design the command set first).
2. Client: read the flag, lobby admin controls UI (after UI module shape is decided).

## Open questions

- Admin command set (user to specify).
- Client: keydown handler (`input.ts:245`) swallows keys while an `<input>` has focus; fix needed for sliders / text.

## Next step

Pin the admin command set with the user, then a server-side builder (task 1).
