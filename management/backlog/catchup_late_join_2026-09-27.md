# BACKLOG: Catch-up / late join

Input date: 2026-09-27

## Item

Join or reconnect mid-match: `SERVER_TICK_BUNDLE` (declared in `game_messages.h`,
unused), per-client send cursor into tick history, `JOIN_REJECTED`, ring-buffer
eviction of old history.

## Why

Clients can only join at tick 0 today. Builds on
`server_input_validation_recording_2026-09-27.md` (input log) and
`persistent_client_identity_2026-09-24.md`.
