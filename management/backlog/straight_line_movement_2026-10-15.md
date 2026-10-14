# Straight-line pathing and movement

Input date: 2026-10-15

## Item

Adapt pathfinding and entity movement to true straight lines, not only the 8 grid
directions.

## Why

Current paths are runs in one of 8 directions (chamfer 5/7 costs); movement follows tile
to tile.

## Notes

- Related known limitations: diagonals can cut between two blocked tiles; heuristic
  assumes min tile cost 1.
- Origin: `work_units/settlement_trade_construction_2026-10-03.md`.
