# Village influence over terrain

Started: 2026-10-03

## Goal

Villages project soft influence over terrain, not hard tile ownership. Rendered
as a faded area on the blit canvas terrain view, not a clean border. Borders
change organically over time (wanted).

## Decisions

- Influence lives in GameCommon sim state (all game mechanics do). It feeds
  the settlement econ tick (`settlement_econ_tick_2026-10-03.md`).
- Approach: cellular-automaton spread (chosen over distance-binding design).
  Terrain affects influence (water blocks, mountains slow, etc.).
- Settlement emits from adjacent tiles at full influence; each update spreads
  to neighbours (flood-fill style) while the settlement's weighted exploitation
  is below a threshold depending on population (and tier: big urban settlements
  claim less per inhabitant). Spread weighted by distance.
- Each tile remembers one main owner settlement.
- Contestation: sourced from strong tiles touching non-friendly (enemy / neutral)
  owned tiles, itself flood-filled to surrounding tiles.
- Tiles feed their owner additively into "land exploitation", weighted by tile
  fertility; contested tiles yield less in total (encourages conflict).
- Settlement influence generation rarely jumps suddenly; slow propagation
  latency is acceptable / desired.
- Tile state, 3 separate planes (SIMD-friendly): owner (1 byte), influence
  (1 byte, 0 = unowned), contestation (1 byte).
- Owner byte = 2-bit region selector + 6-bit index into that region's settlement
  table (cap 64 per region; spawn denied when full, plus a gameplay min distance).
  Selector picks among 4 candidate regions (own, nearest horizontal / vertical
  neighbour by which half of the region the tile is in, diagonal). Requires
  influence path distance from owner < 64 tiles. Tile scale vs gameplay still
  undecided; a non-region-based settlement encoding may replace this later.
- Supply rule: tiles adjacent to a settlement stop being fed when its exploitation
  (previous tick's sum) exceeds its capacity; unfed influence decays outward
  until equilibrium. A tile owned by another settlement is only taken when
  incoming influence beats current by a margin (tuning constant).
- Update every tick for the first slice.
- Updates must be deterministic (lockstep): double-buffered or fixed scan order.
- Destroy: owned tiles lose their owner, neighbours absorb them over later updates.
- Target map size: 1024^2 first, 4096^2 later (see `blit_canvas`).
- Simple first: no staggering, threading or SIMD (ideas for when perf requires;
  multithreading to be discussed then). Practice reasonable non-pessimization.
- Terrain generation out of scope (placeholder only, see Progress).

## Open design

- Spread/decay formula, capacity formula, takeover margin.
- Contestation byte semantics (precision).
- Terrain spread costs per terrain type.
- Contested-tile output penalty formula; diplomatic status ignored until diplomacy exists.
- Rendering: blend of owner colors by strength, alpha by total strength; color
  scheme per viewer vs per player; whether any border hint remains.
- Mid-match join: state is derived, confirm it needs no transfer.

## Depends on

`blit_canvas_2026-09-26.md`.

## Progress

- [DONE] Prerequisite slice: placeholder terrain generated in `match_start`
  (Chebyshev border: < 10 mountains, < 15 hills; central 10x10 lake, outer 2 coast,
  inner 6x6 sea; else plains), copied into client `terrain_tiles` at match begin.
  Region/tile index math duplicated (generation + client copy); extract a helper
  when a third user appears.
- Survey: `area_influence` ui8 exists on settlements, unused.

## Next step

Settle remaining open points (spread formula, capacity), then first slice: per-region
settlement tables + tile planes + CA spread in GameCommon on flat terrain, then terrain costs and land output sum,
then the TS overlay (byte buffer colored in TS, dirty-rect repaint).
