# Territory rendering

Started: 2026-11-06. Related: `village_influence_2026-10-03.md` (border flag, old rendering), `blit_canvas_2026-09-26.md`, `entity_player_colors_2026-10-23.md`.

## Scope

Improve how territories and borders look on the web client:
- Borders one tile thick (today two between touching settlements: the sim marks each settlement's own edge tile).
- Borders between settlements of the same player are subtle (slightly darker); per-settlement hover highlight kept.
- Borders between players stay black.
- First application of a rendering-specific per-tile buffer built by C++/WASM from game info (abstract values, colored by the frontend), the seed of a later view-mode system. Sim `border_count` unchanged (mechanics).
- Multi-file architecture change (C++/WASM export + frontend).

## Decisions

- Backend produces abstract per-tile values, frontend colors them.
- Do not design the general view-mode system up front; build transport + one fill function, generalize at the second mode.
- Shared edge between two settlements: one border tile (needs a tie-break rule).
- Colors: between players black; between settlements of the same player only slightly darker than the territory color (supersedes "hidden").

- View plane lives in the web-specific layer (`web_game_client`), not the platform-independent backend (other clients should not be forced to build it). Owned by `WEB_CLIENT` state; unity build can read backend render state.
- Tie-break: lower settlement slot index shows the border on a shared edge.
- Keep the existing owner / flags planes alongside the new plane.
- Colors are viewer-dependent in the frontend (own vs foreign).

## Tasks

0. [DONE] Survey. Facts:
   - Sim stores `border_count` per tile (`world_terrain.h:37`), not a BORDER flag; client builds `RENDER_TILE_FLAGS` (`IS_BORDER`, `IS_ROAD`, `IS_OWNED`) in `game_client_backend.h:35-41`. Border = any orthogonal neighbour unowned or of another settlement, hence 2 tiles on a shared edge.
   - Client planes (row-major, rebuilt every frame in `game_client_render_influence`, `game_client_render.cpp:175`): terrain ui8, owner slot ui16 (0xFFFF unowned), flags ui8; read by JS as views (no copy). Arena `render_tiles_mem` 32 MiB (`game_client_main.cpp:36`).
   - `render.ts` `draw_territory` (:459-531) loops all tiles in JS each frame; slot -> player mapping built in JS from visible settlements; border painted flat black alpha 196 (:517-523), which also covers the hover tint. `BORDER_DARKEN` no longer exists (village_influence doc stale).
   - `territory_targeting.ts` and `campaign_start.ts` read the raw owner / flags planes (keep them).
1. Plane contract + C++ view pass: new row-major ui8 plane (kind: none / interior / border vs unowned / border vs same player / border vs other player), computed from owner + flags with neighbour comparison; one-sided tie-break. [pending]
2. Frontend: pointer / view in `backend.ts`, `draw_territory` colors by palette, hover highlights the whole settlement incl. its border tiles. [pending]
3. Optional later: refresh only on change; move the per-tile loop out of JS.

## Open questions

- None.

## Next step

Builder for tasks 1-2 (awaiting user go).
