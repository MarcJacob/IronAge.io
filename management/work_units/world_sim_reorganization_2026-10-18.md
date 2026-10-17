# World simulation reorganization

Started: 2026-10-18.

## Goal

Better organize the world simulation code before Alpha feature work: each system in its own implementation file, plus helper headers per theme.

## Scope

- Game common world simulation (`src/game_common/world_sim/`).
- Start from the imprints there; a good imprint pass is wanted (imprints "need a good general pass").

## Tasks

Not broken down yet.

## Notes

- Multi-file architecture change: surveyed and designed before any edit.

## Next step

Survey (builder, read-only): current systems, files, and shared helpers/headers; then propose a split.
