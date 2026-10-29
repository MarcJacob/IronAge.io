# Entity zoom visibility

Started: 2026-10-29. Web client only (wasm platform layer + TS render).

## Goal

Entities shrink, become cheap squares, or disappear depending on zoom and significance; settlements scale by population.

## Decisions

- `render_entity.significance` (ui32, stride 24, offset +20): client backend leaves 0; the wasm platform layer (`web_game_client_main.cpp`) fills it (settlement pop_total, army troop sum, caravan / ship carried value, worker constant 1, battle 0). TS reads it, no per-entity GDO.
- TS: one min-screen-size function per entity type. Final conversion: screen size 1 = not drawn; below threshold (start 2, named constant) = small square; selected entities never hidden or squared.
- Settlement size: 1 tile for <= 100 inhabitants, then +1 px per power of 2 (<128 base, +1 at 128-255, +2 at 256-511...). No exemption from the cull rule: base min px tuned so settlements never reach it.
- Other types keep today's values (linear), in their own functions.
- Hidden entities are not pickable. Squares use the owner tint (one constant switches to black).

## Tasks

1. [DONE, unbuilt] Significance field, platform fill, `backend.ts` reader (stride 24).
2. [DONE, unbuilt, reworked after play-test] Render algorithm in `render.ts`: one size function per type (settlement, army, caravan, ship, worker, battle), shared `entity_draw_mode` (hidden if round(size) <= 1, square if < `ENTITY_SQUARE_SIZE` 5, selected always sprite), pick skips hidden entities (min hit box 6 px), square = owner tint (`ENTITY_SQUARE_USE_OWNER_COLOR`). Notes: `src/platform/web_game_client/.ai_imprint/ships_client_support.md`.

## Decisions (rework)

- `ENTITY_SCALE_UNIT` = 0.1 tile. Settlement: 1 tile + 1 unit per power of 2 from 128, px floor = square size (never culled), no 1-tile cap. Army / caravan / ship: floor(log2(value)) units, no pixel floor. Worker: 0.5 tile fixed. Battle unchanged.

## Open questions

- Army size uses floor(log2) (stepped); smooth log2 if wanted.
- Dead entries left in `ENTITY_BASE_SCALE_MULTIPLIERS`.

- Armies / caravans / ships have a minimum of 1 scale unit (never hidden at max zoom). 0.1 tile reaches the square size (5 px) only on a canvas >= 1250 px high, so min-size entities show as small squares on smaller windows; user accepted and is tweaking constants.

## Next step

User tuning constants by hand; then flag [DONE].
