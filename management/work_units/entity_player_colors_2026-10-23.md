# Entity player colors

Started: 2026-10-23. Branch `feature/entity_player_colors`. Experimental, client-side only.

## Scope

- Render entities and territories in a per-player color (table of 16, index modulo size, no bright green); own entities stay current green.
- Toolbox expansion: entity/territory rendering accepts any arbitrary color (e.g. future mode-specific territory coloring).
- Bonus: black outline around entities, later colorable by diplomatic status.

## Tasks

1. [DONE, unbuilt] Builder: all in `front/src/render.ts`. `RENDER_CONFIG` policies `entity_color`, `territory_color` (per settlement slot), `entity_outline_color`, `outline_entities`; `draw_entity` takes any color + outline; `PLAYER_COLORS` (16); `DIPLOMATIC_TINTS` removed.

## Decisions

- No server/sim changes. Color per player index modulo table size.
- Other players' territories no longer gray; unowned transparent. Worker markers use entity color, no outline; battle markers plain red.
- Custom color modes = replace the `RENDER_CONFIG` policies (restore on exit). Diplomacy outline = replace `entity_outline_color`.

## Open questions

- Confirm `owner` received by front is the player index.
- `front_src.imprint.md` "Render configuration" section could mention the new policy fields (user to edit).

## Next step

Done. Visuals checked by user.
