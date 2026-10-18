// Raising an army from a settlement. The player picks the raise location (in its territory) the new army will move to.
// The raise location must be in the settlement's territory, and the preview is limited to it. The path to the hovered tile is previewed, red if the order would be refused.
// A click sends the order and leaves the mode. Escape leaves without raising.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, send_command, is_command_valid, get_command_feedback, COMMAND_ERROR, type CommandFeedback } from "../input.js"
import { submit_hint } from "../ui/ui_element_hint.js"
import { hotkey_text } from "../ui/ui_element_action_bar.js"
import { query_entity } from "../gdo.js"
import { is_own_settlement } from "../main.js"

// Whether the path preview is drawn as acceptable (green) or refused (red), from the validity feedback of the order.
// Example of using the feedback: each flag can be given its own treatment here (for now every error makes it red).
function is_preview_valid(feedback: CommandFeedback, points: { x: number, y: number }[]): boolean
{
    const flags = feedback.values.error_flags ?? 0;

    // A raise location outside the settlement's territory: the path is shown, but red.
    if ((flags & COMMAND_ERROR.TARGET_OUT_OF_TERRITORY) !== 0) return false;

    // Any other error (not enough population, army limit...) also refuses the order.
    if (!feedback.valid) return false;

    return points.length > 0;
}

export class ArmyRaising extends Core.Mode
{
    // exit_key: code of the key that entered the mode, pressing it again leaves the mode.
    constructor(private settlement_guid: number, private exit_key: string) { super(); }

    on_entry(): void
    {
        submit_hint({ text: `Raise Army: click a tile to raise an army that moves there. Esc or [${hotkey_text(this.exit_key)}] to cancel.` });
    }

    on_exit(): void
    {
        submit_hint(null);
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

        const hovered = Render.get_hovered_tile();
        if (hovered === null)
        {
            Render.RENDER_CONFIG.path_preview = null;
            return;
        }

        // The path is computed to any hovered tile (no territory limit), so the player sees where an order outside the territory would go.
        // Whether the order is acceptable comes from the validity feedback, not from the path.
        const start = view.get_location("location");
        const points = Backend.query_path(start, hovered);
        const feedback = get_command_feedback("RAISE_ARMY", { settlement: this.settlement_guid, raise_location: hovered });
        Render.RENDER_CONFIG.path_preview = { start, points, valid: is_preview_valid(feedback, points) };
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
