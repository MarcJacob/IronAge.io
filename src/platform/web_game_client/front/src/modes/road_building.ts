// Building roads from a settlement. The player picks an origin (the settlement itself, or a road tile in its territory; the settlement
// by default) and a destination (any tile: building outside the player's territory takes longer).
// While the mouse is over the map, the path from origin to hovered tile is previewed, red if the order would be refused.
// Stays active after an order, so several roads can be built in a row. Escape leaves.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, send_command, is_command_valid, get_command_feedback, COMMAND_INVALID_PATH } from "../input.js"
import { query_entity } from "../gdo.js"
import { is_own_settlement } from "../main.js"

export class RoadBuilding extends Core.Mode
{
    private origin: { x: number, y: number } = { x: 0, y: 0 };

    // exit_key: code of the key that entered the mode, pressing it again leaves the mode.
    constructor(private settlement_guid: number, private exit_key: string) { super(); }

    on_entry(): void
    {
        // Default origin: the settlement itself.
        const view = query_entity(this.settlement_guid);
        if (view === null) { Core.pop_mode(); return; }

        this.origin = view.get_location("location");
    }

    on_exit(): void
    {
        Render.RENDER_CONFIG.path_preview = null;
    }

    on_frame_update(delta_time_s: number): void
    {
        // Leave when the settlement is gone or not the selection anymore.
        const view = query_entity(this.settlement_guid);
        if (!is_own_settlement(view) || get_selected_entity_guid() !== this.settlement_guid)
        {
            Core.pop_mode();
            return;
        }

        const hovered = Render.get_hovered_tile();
        if (hovered === null)
        {
            Render.RENDER_CONFIG.path_preview = null;
            return;
        }

        const points = Backend.query_path(this.origin, hovered, 0, true); // Tile mode: roads cover tiles, not straight lines.
        const valid = points.length > 0 && is_command_valid("BUILD_ROAD", { settlement: this.settlement_guid, origin: this.origin, destination: hovered });
        Render.RENDER_CONFIG.path_preview = { start: this.origin, points, valid };
    }

    // Only the exit key is handled here: other keys (e.g. the hotkey of a sibling mode) fall through to the modes below.
    // Clicks and right clicks are handled actions of this mode.
    on_key_down(event: KeyboardEvent): boolean
    {
        if (event.code !== this.exit_key) return false;

        if (!event.repeat) Core.pop_mode();
        return true;
    }

    // Right click leaves, like Escape (the browser menu is already suppressed by input.ts).
    on_context_menu(event: MouseEvent): boolean
    {
        Core.pop_mode();
        return true;
    }

    on_click(event: MouseEvent): boolean
    {
        if (event.button !== 0) return true;

        const tile = Render.page_to_tile(event.clientX, event.clientY);
        if (!Render.is_tile_in_world(tile)) return true;

        // Settlement or road tile of its territory: new origin.
        if (this.is_origin_candidate(tile))
        {
            this.origin = tile;
            return true;
        }

        // Anything else: destination. Refused orders are not sent.
        const buildRoad = { settlement: this.settlement_guid, origin: this.origin, destination: tile };
        if (is_command_valid("BUILD_ROAD", buildRoad)) send_command("BUILD_ROAD", buildRoad);
        return true;
    }

    // Whether a road can start from the tile, as the server's validity check says (the settlement's own tile, or a road tile of its territory).
    // The destination is not known yet: a neighbouring tile stands in for it, so that the check does not stop at "origin is destination"
    // (which would hide the origin error). Other errors (e.g. out of bounds destination) are ignored, only the origin verdict is read.
    private is_origin_candidate(tile: { x: number, y: number }): boolean
    {
        const feedback = get_command_feedback("BUILD_ROAD", { settlement: this.settlement_guid, origin: tile, destination: { x: tile.x + 1, y: tile.y } });
        // Empty values: the command could not be built.
        return feedback.values.invalid_path_reason !== undefined && feedback.values.invalid_path_reason !== COMMAND_INVALID_PATH.ORIGIN_INVALID;
    }
}
