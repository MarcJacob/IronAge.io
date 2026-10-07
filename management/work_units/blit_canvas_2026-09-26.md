# Blit canvas

Input date: 2026-09-26

## Item

Second canvas layer written from WASM as a raw pixel buffer and blitted to
screen, for content too expensive to draw through the JS 2D canvas API (e.g.
terrain / fog of war).

## Decisions

- Pure-JS rendering is sufficient for now; needed once a workload exceeds it.
  Phase 3 terrain bitmap is the expected first use.
- Terrain generation is out of scope: single tile type for now (plains);
  image-loaded terrain later.
- Target 1024^2 first; 4096^2 later needs a client memory-size decision
  (fixed 128 MiB block, 64 MiB match arena).
- User implements by hand; manager guides, builders for scoped pieces.
- Render state is split: per-frame `Frame` sub-structure + persistent parts; JS
  reads through small getter exports (no whole-struct pointer).
- Terrain buffer is one byte per tile (terrain type enum value), row-major
  `y * w + x`, row 0 = world y 0. TS converts type -> RGBA via `TERRAIN_COLORS`
  (mirrors `TERRAIN_TILE_TYPE`) into an offscreen canvas of world size (1 px
  per tile), put once at match join; `draw_terrain()` blits the visible source
  rect (clamped, snapped to tiles, Y flipped by transform).
- Tile `i` covers [i, i+1] in world space; entities drawn centered on the tile
  (+0.5), in `draw_entity`.
- Single match per page load for now; no re-allocation / leak handling.
  Allocation failures: asserts.
- `influence_tiles_bitmap` still an RGBA `render_tile` buffer; likely to follow
  the terrain's type-index approach (see `village_influence`).

## Progress

- [DONE] WASM side, `backend.ts` repair, terrain type buffer, offscreen canvas,
  `draw_terrain()` blit, entity centering. Hand-verified: terrain renders correctly.

## Open

- Picking / input still use corner-based rounding (`page_to_world` round,
  `pick_entity_at`, `send_set_target_loc` + C++ round in `game_client_input.cpp`).
  Works acceptably for now; revisit if half-tile offset shows.
- Offscreen canvas is filled once; terrain changes (dirty rects) not yet handled.
- Match re-join (lifecycle step 5) not handled.

## Next step

Unit's terrain-bitmap goal met. Influence overlay and dirty-rect updates belong to
`village_influence_2026-10-03.md`. Decide whether to flag this unit [DONE].
