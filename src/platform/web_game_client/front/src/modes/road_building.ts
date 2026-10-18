// Building roads from a settlement. The player picks an origin (the settlement itself, or a road tile in its territory; the settlement
// by default) and a destination (any tile: the server builds up to the edge of the settlement's territory).
// While the mouse is over the map, the path from origin to hovered tile is previewed, red if the order would be refused.
// Stays active after an order, so several roads can be built in a row. Escape leaves.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, send_command, is_command_valid } from "../input.js"
import { submit_hint } from "../ui/ui_element_hint.js"
import { hotkey_text } from "../ui/ui_element_action_bar.js"
import { is_own_settlement } from "../main.js"

export class RoadBuilding extends Core.Mode
{
    private origin: { x: number, y: number } = { x: 0, y: 0 };

    // exit_key: code of the key that entered the mode, pressing it again leaves the mode.
    constructor(private settlement_guid: number, private exit_key: string) { super(); }

    on_entry(): void
    {
        // Default origin: the settlement itself.
        const view = Backend.query_entity(this.settlement_guid);
        if (view === null) { Core.pop_mode(); return; }

        this.origin = { x: view.location_x, y: view.location_y };
        submit_hint({ text: `Build Road: click a tile to build a road to it. Click the settlement or a road tile to start from there. Esc or [${hotkey_text(this.exit_key)}] to cancel.` });
    }

    on_exit(): void
    {
        submit_hint(null);
        Render.RENDER_CONFIG.path_preview = null;
    }

    on_frame_update(delta_time_s: number): void
    {
        // Leave when the settlement is gone or not the selection anymore.
        const view = Backend.query_entity(this.settlement_guid);
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

        // The preview is limited to the settlement's territory, as the build will be.
        const points = Backend.query_path(this.origin, hovered, this.settlement_guid);
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

    // Whether a road can start from the tile: the settlement's own tile, or a road tile in the settlement's territory.
    private is_origin_candidate(tile: { x: number, y: number }): boolean
    {
        const view = Backend.query_entity(this.settlement_guid);
        if (view === null) return false;
        if (tile.x === view.location_x && tile.y === view.location_y) return true;

        const state = Backend.LAST_RENDER_STATE;
        const flags = Backend.get_tile_flags_view();
        const owners = Backend.get_owner_settlement_tiles_view();
        if (state === null || flags === null || owners === null) return false;

        const tileIndex = tile.y * state.world_size.width + tile.x;
        if ((flags[tileIndex] & Render.TILE_FLAG_ROAD) === 0 || (flags[tileIndex] & Render.TILE_FLAG_OWNED) === 0) return false;

        // The owner is the settlement's slot: the index part of its entity_guid (bits 8-19), as in render.ts.
        return owners[tileIndex] === ((this.settlement_guid >>> 8) & 0xFFF);
    }
}
