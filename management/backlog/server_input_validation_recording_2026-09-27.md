# BACKLOG: Server-side input validation & recording

Input date: 2026-09-27

## Item

Server-side input validation & recording: wire the existing (unused) command
sanity-check functions into `match_tick`; per-match input log; viewed-tick
staleness gating.

## Description

Per-command validity checking and `emit_tick` staleness gating against real
player `CLIENT_TICK` input are both wired in as of 2026-09-30
(`match_command_sequence_output_validated` / `push_validated_sequence` in
`game_server_main.cpp`). What's left: a per-match log of applied input
commands.

## Why

An input log is needed for replay/debugging and for building catch-up/late
join on top of (see `catchup_late_join_2026-09-27.md`).
