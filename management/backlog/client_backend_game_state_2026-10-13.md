# BACKLOG: Game state / lobby data in the client backend

Input date: 2026-10-13

## Item

Move high-level game state (lobby state, match start params, in-match / ended status) from the TS
frontend (read straight from message buffers in backend.ts / main.ts) into the client backend (wasm).

## Why

Lobby data is currently decoded in TS as a stopgap (see comments in backend.ts, lobby_ui.ts).
