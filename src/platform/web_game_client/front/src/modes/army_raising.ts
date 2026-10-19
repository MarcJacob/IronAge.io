// Raising an army from a settlement. The player sets the troops to raise in the advanced action panel, then picks the target location (in the settlement's territory).
// The army appears inside the settlement, empty, and draws its men from it until it reaches its targets (see world_system_armies_raising), then walks to the target.
// Its men keep counting in the settlement's population: the panel shows how much of it is raised. A settlement has a limited number of raised armies.
// The path from the settlement to the hovered tile is previewed, red if the order would be refused.
// A click sends the order and leaves the mode. Escape leaves without raising.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, send_command, is_command_valid, get_command_feedback, COMMAND_ERROR, type CommandFeedback } from "../input.js"
import { submit_hint } from "../ui/ui_element_hint.js"
import { hotkey_text } from "../ui/ui_element_action_bar.js"
import { submit_advanced_action_panel } from "../ui/ui_element_advanced_action_panel.js"
import { army_gauges, raise_target_props } from "../ui/troop_gauges.js"
import { settlement_raised_rows } from "../ui/entity_panels.js"
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
    // Troops to raise per type (levies, men-at-arms, horsemen, knights), set with the panel's sliders. At least one must be nonzero to raise.
    private targets = [0, 0, 0, 0];

    // exit_key: code of the key that entered the mode, pressing it again leaves the mode.
    constructor(private settlement_guid: number, private exit_key: string) { super(); }

    on_entry(): void
    {
        submit_hint({ text: `Raise Army: set the troops with the sliders, then click a tile of the settlement's territory as the army's target. The army gathers its men inside the settlement over time, then marches to the target (select it and right click to change the target, even while raising). Esc or [${hotkey_text(this.exit_key)}] to cancel.` });
    }

    on_exit(): void
    {
        submit_hint(null);
        submit_advanced_action_panel(null);
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

        submit_advanced_action_panel({
            title: "Raise army",
            stats: settlement_raised_rows(view),
            gauges_title: "Troops to raise",
            slider_name: "target",
            gauges: army_gauges(null, view, this.targets, (troop, value) => { this.targets[troop] = value; }),
        });

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
        const feedback = get_command_feedback("RAISE_ARMY", { settlement: this.settlement_guid, raise_location: hovered, ...raise_target_props(this.targets) });
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

        // Refused orders (including no troops to raise) are not sent and the mode stays.
        const raiseArmy = { settlement: this.settlement_guid, raise_location: tile, ...raise_target_props(this.targets) };
        if (!this.targets.some(target => target > 0) || !is_command_valid("RAISE_ARMY", raiseArmy)) return true;

        send_command("RAISE_ARMY", raiseArmy);
        Core.pop_mode();
        return true;
    }
}
