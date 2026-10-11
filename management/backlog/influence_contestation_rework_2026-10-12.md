# BACKLOG: Influence / contestation rework

Input date: 2026-10-12

## Item

Rework the influence and contestation mechanics (see `work_units/village_influence_2026-10-03.md`
for the current claim / maintain / contestation / takeover implementation) with these goals:
- Simplify the rules and code.
- Optimize (full-map passes over owned tiles every tick, per-frame overlay rebuild on the client,
  scratch plane memory).
- More organic feel (fields, fronts).
- Greater impact of contestation.

## Why

The first implementation works and is good enough for now, but the user is not fully happy with
how it plays and feels. Known points: front oscillation after takeover, strong-influence tiles
clearing contestation fast, slot-reuse stale tiles, 8 bytes per tile.
