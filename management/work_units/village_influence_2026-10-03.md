# Village influence over terrain

Started: 2026-10-03

## Goal

Villages project soft influence over terrain, not hard tile ownership. Rendered
as a faded area on the blit canvas terrain view, not a clean border. Borders
change organically over time (wanted).

## Decisions

- Influence lives in GameCommon sim state. It feeds the settlement econ tick
  (`settlement_econ_tick_2026-10-03.md`).
- Update every tick, deterministic (double-buffered or fixed scan order), simple first:
  no staggering / threading / SIMD (region-by-region work later).
- Tile planes (separate): owner, influence (ui8, 0 = unowned), contestation (ui8).
  Owner = ui16 settlement slot index per tile for now (region selector + 6-bit
  encoding dropped; compact encoding maybe later).
- Model: fluid-like diffusion. Settlement tile is the source; every tile is a uniform
  sink (leak); terrain only changes transmission (conductance), never sinks, so hard
  terrain redirects / slows spread. A tile receives from several neighbours; distance
  (diagonal) also weighs.
- Terrain cost (resistance): plains 4, coast 8, hills 8, sea 12, mountains 32 (barrier);
  lakes are coast + sea tiles (no own type).
- Influence stays 1 byte (memory); small differences truncating to zero flow is intended.
- Capacity = pop * k, k slightly above 1 (start 1.5), so target pop keeps rising from
  existing pop for a while. Equilibrium (pop = output) is the point; it pushes the player
  to found settlements, improve infrastructure, raid neighbours, trade.
- Slow organic change: rise / fall rates R and F per tick, scaled by how settlement
  capacity compares to its last land output (fast shrink after sudden pop loss).
- No supply rule; the field erodes inward: tiles look at whether the owner is
  under capacity vs its last output and adjust by their influence, distance, terrain.
- Capacity = linear in population (no tiers for now). Expansion gets costlier with
  distance / terrain, giving a soft population ceiling.
- Ownership slice 1: unowned tile takes the owner of its strongest neighbour; owned
  tile keeps its owner until influence reaches 0.
- Takeover (with contestation, later): owner changes only when contestation is high
  enough, then higher influence wins; margin depends on contestation (full = 0,
  none = effectively impossible, ~255).
- Contestation (later): influence from a settlement that is not the tile's owner
  contributes contestation instead; static decay per tick, spreads over neighbours like
  influence (peaks at borders, fades outward).
- Land output: one pass over all tiles per tick (all tiles for now), per settlement sum
  of terrain-based value (no fertility) weighted by influence; contested tiles yield
  less. 1 land output = 1 population naturally supported; it is the base of the pop
  growth target. Pop -> capacity -> influence -> output loop.
- Destroy: owned tiles lose their owner, neighbours absorb them over later updates.
- Target map size: 1024^2 first, 4096^2 later (see `blit_canvas`).
- Terrain generation out of scope (placeholder only).
- Rendering: contestation drives hue / alpha (pure red at full), influence a capped
  green tint (~110/255 alpha), per-terrain darkening (plains 1.0, hills 0.85,
  mountains 0.7). Dirty-rect repaint later.

## Open design

- Exact formulas, R / F values, leak amount, k (tuned by play).
- Contested-tile output penalty formula; diplomacy ignored until it exists.
- Rendering: per-owner colors, viewer vs player scheme, border hint.
- Mid-match join: state is derived, confirm it needs no transfer.

## Depends on

`blit_canvas_2026-09-26.md`.

## Progress

- [DONE] Placeholder terrain generated in `match_start` (Chebyshev border: < 10 mountains,
  < 15 hills; central 10x10 lake, outer 2 coast, inner 6x6 sea; else plains).
- [DONE] Storage + render plumbing (commit 640cb5b): per-region planes (owner is a 4-byte
  `entity_guid`, never written), copied to client byte buffers, TS overlay over terrain.
- Placeholder only: econ tick stamps a 20x20 box every tick per settlement.
- [DONE] Coordinate unification in `render.ts` (tile-aligned, entities centered on tile
  center), hover tile highlight.

## Next step

Settle the capacity / field open points, then builder tasks: (1) owner ui16 + diffusion
spread on terrain replacing the box stamp, (2) land output pass feeding the pop target,
(3) rendering formula; contestation / takeover after.
