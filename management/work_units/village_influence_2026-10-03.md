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
- Model (replaced the fluid diffusion, too liquid / granular / costly): blocky claim /
  maintain automaton. Per-tile flags plane (`TILE_FLAGS`, first flag BORDER = owned tile
  touching an unowned tile, or touching a tile of a settlement with a higher slot index
  (one-sided at contact lines, stateless); updated incrementally on claim / unclaim; just a mechanics marker, also usable for roads etc.). Border tiles
  render pitch black. Unowned tiles are skipped (influence / contestation forced 0).
- Border tile with enough influence (user-tuned; code has 92 / start 32) claims the
  cheapest affordable unowned neighbour per tick if owner's maintenance total + tile
  cost <= capacity. Over budget (maintenance > capacity): ALL owned tiles except the
  settlement tile decay, fall = max(1, 4 * tile cost * overshoot / (68 * 256)), overshoot
  fixed at tick start; at 0 they are unclaimed and neighbours re-flagged. Costly / far
  tiles go first. Growth and claiming frozen while over budget.
- Interior growth: (strongest same-owner neighbour - own) * pressure, pressure from
  (capacity - output) / capacity; frozen while over budget.
- Tile maintenance cost (fixed point x16) = terrain resistance * (16 + squared Euclidean
  dist to settlement) / 16 (squared, not Chebyshev: square fields). Claim picks the
  cheapest affordable neighbour. Interior growth scaled by ref cost / tile cost
  (`GROW_COST_SCALING`), so far tiles grow slower and stall below the claim threshold:
  a natural field-size cap besides the budget.
  Resistance: plains 4, coast 8, hills 8, sea 12, mountains 32; lakes are coast + sea tiles.
- Effective land output = min(sum, capacity) (drops at once when pop falls).
- Influence stays 1 byte (memory).
- Capacity = linear in pop (existing `pop + 10 + pop / 2`), no tiers. Equilibrium
  (pop = output) is the point; it pushes the player to found settlements, improve
  infrastructure, raid neighbours, trade.
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

- Constants (tuned by play).
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
- [WIP] Claim / maintain automaton written, awaiting hand check: owner ui16, flags plane,
  `world_system_economy_influence_spread` + `world_system_economy_land_output` in
  `world_system_economy.cpp` (terrain output plains 10, hills 6, coast 40, mountains 4,
  sea 10; `land_outputs` feeds the pop target). Untuned: grow div 8, claim min 128 / start
  64, border fall 4, cost scale 1 (pop 100 affords ~25-30 plains tiles).
  `influence_capacities` widened to ui32 (sim + client view `influence_capacity`, TS reader).
  Memory 7 bytes / tile (flags + `influence_next` scratch).
- [DONE] Flags plane copied to client (`tile_flags_bitmap`, `client_render_state_get_tile_flags`);
  border tiles darkened in `draw_influence` (BORDER_DARKEN 0.6, BORDER_MIN_ALPHA 140).
- Known: slot reuse inherits a destroyed settlement's stale tiles; new settlement has
  capacity 0 until its first economy tick (<= 10 ticks).
- [DONE] Coordinate unification in `render.ts` (tile-aligned, entities centered on tile
  center), hover tile highlight.

## Next step

User hand-tests and tunes. Then: rendering (flags plane to client, border = slightly
darker tile, new color formula), then contestation / takeover.
