# Village influence over terrain

Started: 2026-10-03

## Goal

Villages project influence over terrain tiles. Tiles are colored by
ownership: own territory green, enemy red, etc., with a dark shade on the
border. Rendered on the blit canvas terrain view.

## Decisions

- Influence lives in GameCommon sim state (all game mechanics do). It feeds
  the settlement econ tick (`settlement_econ_tick_2026-10-03.md`).

## Open design

- Influence model: radius, falloff, contested tiles, per-tile data layout.
- Color scheme per viewer vs per player.

## Depends on

`blit_canvas_2026-09-26.md`.

## Progress

Not started.
