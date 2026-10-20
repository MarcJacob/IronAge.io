// Ordinary gameplay: HUD, entity selection and orders. Selecting an own settlement enters the settlement selected mode, an own army the army selected mode.

import * as Core from "../core.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, get_selected_entity_view, set_selected_entity_guid } from "../input.js"
import { is_own_army, is_own_settlement } from "../main.js"
import { set_game_ui_visible, update_game_ui, toggle_debug_panel } from "../ui/game_ui.js"
import { submit_properties_panel } from "../ui/inspection/ui_element_properties_panel.js"
import { inspect_entity } from "../ui/inspection/inspect_entity.js"
import { SettlementSelected } from "./settlement_selected.js"
import { ArmySelected } from "./army_selected.js"

const DEBUG_PANEL_HOTKEY = "Backquote"; // Shows / hides the debug entity text (clear of the camera keys).

export class MainPlay extends Core.Mode
{
    on_entry(): void
    {
        set_game_ui_visible(true);
    }

    on_exit(): void
    {
        set_game_ui_visible(false);
        submit_properties_panel(null);
        Render.RENDER_CONFIG.outlined_entity = null;
        Render.RENDER_CONFIG.path_preview = null;
    }

    on_frame_update(delta_time_s: number): void
    {
        update_game_ui();
        Render.RENDER_CONFIG.outlined_entity = get_selected_entity_guid();

        // Modes above this one set their own preview after this update (modes update bottom first).
        Render.RENDER_CONFIG.path_preview = null;
        if (!this.is_top) return;

        const selected = get_selected_entity_view();

        if (selected === null) submit_properties_panel(null);
        else if (is_own_settlement(selected)) Core.push_mode(new SettlementSelected(), this);
        else if (is_own_army(selected)) Core.push_mode(new ArmySelected(selected.guid), this);
        // Anything else (enemy or other entity types): read-only properties, no callbacks.
        else submit_properties_panel(inspect_entity(selected, {}));
    }

    // Debug panel toggle.
    on_key_down(event: KeyboardEvent): boolean
    {
        if (event.code !== DEBUG_PANEL_HOTKEY) return false;

        if (!event.repeat) toggle_debug_panel();
        return true;
    }

    // The mode right above the game base: leaving it would leave the match without a play mode.
    on_escape(): boolean { return true; }

    // Default left click of the play modes (the modes above it let their clicks fall through to it): selects the entity under the cursor.
    // Empty space clears the selection.
    on_click(event: MouseEvent): boolean
    {
        if (event.button !== 0) return true;

        set_selected_entity_guid(Render.pick_entity_at(event.clientX, event.clientY, get_selected_entity_guid()));
        return true;
    }
}
