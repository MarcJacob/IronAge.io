// Founding a settlement from a settlement. The player picks the target tile the colonists (a worker taken out of the settlement's peasants) walk to
// to found the new settlement. The path to the hovered tile is previewed, red if the order would be refused (invalid settlement location, not enough peasants...).
// A click on a valid tile sends the order and leaves the mode. Escape or a right click leaves without founding.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, send_command, is_command_valid, get_command_feedback, COMMAND_ERROR, COMMAND_INVALID_PATH } from "../input.js"
import { show_entity, show_found_settlement_panel } from "../ui/inspection/inspect_entity.js"
import { query_entity, get_game_parameters } from "../gdo.js"
import { is_own_settlement } from "../main.js"

export class SettlementFounding extends Core.Mode
{
    // exit_key: code of the key that entered the mode, pressing it again leaves the mode.
    constructor(private settlement_guid: number, private exit_key: string) { super(); }

    on_exit(): void
    {
        show_entity(null);
        Render.RENDER_CONFIG.path_preview = null;
    }

    on_frame_update(delta_time_s: number): void
    {
        // Leave when the settlement is gone or not the selection anymore.
        const view = query_entity(this.settlement_guid);
        if (view === null || !is_own_settlement(view) || get_selected_entity_guid() !== this.settlement_guid)
        {
            Core.pop_mode();
            return;
        }

        const colonists = get_game_parameters().get_number("found_settlement_colonists");
        const peasants = view.get_number("peasants");

        const hovered = Render.get_hovered_tile();
        if (hovered === null)
        {
            show_found_settlement_panel({ colonists, peasants, target: null });
            Render.RENDER_CONFIG.path_preview = null;
            return;
        }

        // The path is computed to any hovered tile (no territory limit: the colonists walk anywhere), so the player sees where they would go.
        // Whether the order is acceptable comes from the validity feedback, not from the path.
        const start = view.get_location("location");
        const points = Backend.query_path(start, hovered);
        const feedback = get_command_feedback("FOUND_SETTLEMENT", { settlement: this.settlement_guid, target: hovered });

        show_found_settlement_panel({ colonists, peasants, target: this.target_text(feedback.valid, feedback.values, points.length > 0) });
        Render.RENDER_CONFIG.path_preview = { start, points, valid: feedback.valid && points.length > 0 };
    }

    // What the validity feedback says about the hovered tile, in words.
    private target_text(valid: boolean, values: { [name: string]: number }, has_path: boolean): string
    {
        const flags = values.error_flags ?? 0;
        if ((flags & COMMAND_ERROR.TARGET_OUT_OF_BOUNDS) !== 0) return "Outside the world";
        if ((flags & COMMAND_ERROR.INVALID_PATH) !== 0 && values.invalid_path_reason === COMMAND_INVALID_PATH.DESTINATION_INVALID) return "Cannot found a settlement here";
        if ((flags & COMMAND_ERROR.NOT_ENOUGH_POPULATION) !== 0) return "Not enough peasants";
        if ((flags & COMMAND_ERROR.ENTITY_LIMIT_REACHED) !== 0) return "Too many entities";
        if (!valid) return "Refused";
        return has_path ? "Valid" : "No path";
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

        // Refused orders (including a location where no settlement can be founded) are not sent and the mode stays.
        const foundSettlement = { settlement: this.settlement_guid, target: tile };
        if (!is_command_valid("FOUND_SETTLEMENT", foundSettlement)) return true;

        send_command("FOUND_SETTLEMENT", foundSettlement);
        Core.pop_mode();
        return true;
    }
}
