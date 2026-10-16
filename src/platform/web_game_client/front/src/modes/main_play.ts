// Ordinary gameplay: HUD, entity selection and orders. Selecting an own settlement enters the settlement selected mode, an own army the army selected mode.

import * as Core from "../core.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, get_selected_entity_view } from "../input.js"
import { set_game_ui_visible, update_game_ui } from "../ui/game_ui.js"
import { is_own_army, is_own_settlement, play_click, play_context_menu, update_army_path_preview } from "./play_common.js"
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
        if (!this.is_top)
        {
            Render.RENDER_CONFIG.path_preview = null;
            return;
        }

        update_army_path_preview();

        const selected = get_selected_entity_view();
        if (is_own_settlement(selected)) Core.push_mode(new SettlementSelected());
        else if (is_own_army(selected)) Core.push_mode(new ArmySelected());
    }

    // The mode right above the game base: leaving it would leave the match without a play mode.
    on_escape(): void {}

    on_click(event: MouseEvent): void
    {
        play_click(event);
    }

    on_context_menu(event: MouseEvent): void
    {
        play_context_menu(event);
    }
}
