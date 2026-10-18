// Ordinary gameplay: HUD, entity selection and orders. Selecting an own settlement enters the settlement selected mode, an own army the army selected mode.

import * as Core from "../core.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, get_selected_entity_view, set_selected_entity_guid } from "../input.js"
import { is_own_army, is_own_settlement } from "../main.js"
import { set_game_ui_visible, update_game_ui } from "../ui/game_ui.js"
import { SettlementSelected } from "./settlement_selected.js"
import { ArmySelected } from "./army_selected.js"

export class MainPlay extends Core.Mode
{
    on_entry(): void
    {
        set_game_ui_visible(true);
    }

    on_exit(): void
    {
        set_game_ui_visible(false);
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

        if (selected !== null)
            {
                if (is_own_settlement(selected)) Core.push_mode(new SettlementSelected(), this);
                else if (is_own_army(selected)) Core.push_mode(new ArmySelected(selected.guid), this);
            }
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
