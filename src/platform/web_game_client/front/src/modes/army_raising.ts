// Raising an army from a settlement. The player picks the raise location (in its territory) the new army will move to.
// The raise location must be in the settlement's territory, and the preview is limited to it. The path to the hovered tile is previewed, red if the order would be refused.
// A click sends the order and leaves the mode. Escape leaves without raising.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, send_command, is_command_valid } from "../input.js"
import { show_mode_hint, hide_mode_hint } from "../ui/mode_hint_ui.js"
import { is_own_settlement } from "../main.js"

const HINT = "Raise Army: click a tile to raise an army that moves there. Esc or [2] to cancel.";

export class ArmyRaising extends Core.Mode
{
    // exit_key: code of the key that entered the mode, pressing it again leaves the mode.
    constructor(private settlement_guid: number, private exit_key: string) { super(); }

    on_entry(): void
    {
        show_mode_hint(HINT);
    }

    on_exit(): void
    {
        hide_mode_hint();
        Render.RENDER_CONFIG.path_preview = null;
    }

    on_frame_update(delta_time_s: number): void
    {
        // Leave when the settlement is gone or not the selection anymore.
        const view = Backend.query_entity(this.settlement_guid);
        if (view === null || !is_own_settlement(view) || get_selected_entity_guid() !== this.settlement_guid)
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

        const start = { x: view.location_x, y: view.location_y };
        const points = Backend.query_path(start, hovered, this.settlement_guid);
        const valid = points.length > 0 && is_command_valid("RAISE_ARMY", { settlement: this.settlement_guid, raise_location: hovered });
        Render.RENDER_CONFIG.path_preview = { start, points, valid };
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

        // Refused orders are not sent and the mode stays.
        const raiseArmy = { settlement: this.settlement_guid, raise_location: tile };
        if (!is_command_valid("RAISE_ARMY", raiseArmy)) return true;

        send_command("RAISE_ARMY", raiseArmy);
        Core.pop_mode();
        return true;
    }
}
