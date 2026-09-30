# BACKLOG: Catch-up / late join

Input date: 2026-09-27

## Item

Catch-up / late join: tick bundles, per-client send cursor, `JOIN_REJECTED`,
ring-buffer log eviction for long matches.

## Description

Let a client join (or reconnect to) a match already in progress and catch up
via `SERVER_TICK_BUNDLE` (already declared in `game_messages.h` but unused),
tracking a per-client send cursor into the match's tick history. Needs
`JOIN_REJECTED` for when catch-up isn't possible, and ring-buffer eviction of
old tick history so long-running matches don't grow that log unbounded.

## Why

Currently a client can only join at tick 0 - there's no way to join or
reconnect mid-match. Builds on the per-match input log from
`server_input_validation_recording_2026-09-27.md` and persistent client
identity from `persistent_client_identity_2026-09-24.md`.
