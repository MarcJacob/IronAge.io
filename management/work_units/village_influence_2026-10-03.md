# Village influence over terrain

Started: 2026-10-03

## Goal

Villages project soft influence over terrain, not hard tile ownership. Several
settlements (usually 1, sometimes 2, rarely 3) can influence the same tile.
Rendered as a faded area on the blit canvas terrain view, not a clean border.

## Decisions

- Influence lives in GameCommon sim state (all game mechanics do). It feeds
  the settlement econ tick (`settlement_econ_tick_2026-10-03.md`).
- Per tile: 2 (maybe 3) unranked influencing-settlement slots, strength
  derived from distance and population. Tiles feed their settlements additively into "land exploitation",
  weighted by tile fertility; a contested tile yields less in total (encourages
  conflict).
- Storage is region-local (tiles belong to regions): static entities are stored
  per region so tile claimants can be updated cheaply when a settlement is
  founded / destroyed in nearby regions.
- Target map size: 1024^2 first, 4096^2 later (see `blit_canvas`).
- Strength depends on population and is reduced by tier (big urban settlements
  claim less per inhabitant); falls off with squared distance; never reaches
  further than one region.
- Slot choice is unranked: settlements aligned behind a closer one are
  discarded (natural areas when dense, cheap).
- Slot entry ~1 byte: per-region settlement table index + which of the 9
  regions (self + 8 neighbours). Needs a strict settlements-per-region cap.
- Land production per settlement is updated coarsely, deterministically
  staggered (never every tick).
- Optional coarse influence grid if memory requires.
- Terrain generation out of scope: one tile type for now; color-coded image
  loading later.

## Open design

- Alignment discard rule and slot count (2 vs 3).
- Whether slot membership is static between found / destroy (strength computed
  on read from distance + current population) or re-evaluated over time.
- Update schedule: period and stagger rule.
- Per-region settlement cap and behavior when a region is full.
- Contested-tile output penalty formula.
- Rendering: blend of owner colors by strength, alpha by total strength; color
  scheme per viewer vs per player; whether any border hint remains.

## Depends on

`blit_canvas_2026-09-26.md`.

## Progress

Not started. Survey done (terrain struct exists but is never generated;
`area_influence` ui8 exists on settlements, unused). `blit_canvas` base is in
place (terrain type buffer + offscreen canvas + blit).

- [DONE] Prerequisite slice: placeholder terrain generated in `match_start`
  (Chebyshev border: < 10 mountains, < 15 hills; central 10x10 lake, outer 2 coast,
  inner 6x6 sea; else plains), copied into client `terrain_tiles` at match begin.
  Region/tile index math duplicated (generation + client copy); extract a helper
  when a third user appears.

## Next step

Design discussion on the open points above, then first slice: per-region settlement tables and
tile slot bytes in GameCommon with a fixed influence radius; then the staggered
land production update; then the TS overlay (influence as a type / byte buffer,
colored in TS, dirty-rect repaint of the offscreen canvas).
