// Ordinary gameplay: HUD, entity selection and orders. Selecting a settlement enters the settlement selected mode, an army the army selected mode,
// a siege the siege selected mode, a battle the battle selected mode (anyone's).

import * as Core from "../core.js"
import * as Render from "../render.js"
import * as Backend from "../backend.js"
import { get_selected_entity_guid, get_selected_entity_view, set_selected_entity_guid } from "../input.js"
import { set_game_ui_visible, update_game_ui } from "../ui/game_ui.js"
import { SettlementSelected } from "./settlement_selected.js"
import { ArmySelected } from "./army_selected.js"
import { SiegeSelected } from "./siege_selected.js"
import { BattleSelected } from "./battle_selected.js"

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

        // Anyone's entity: each mode disables what the local player cannot do.
        if (selected !== null)
            {
                if (selected.entity_type === Backend.ENTITY_TYPE.SETTLEMENT) Core.push_mode(new SettlementSelected(), this);
                else if (selected.entity_type === Backend.ENTITY_TYPE.ARMY) Core.push_mode(new ArmySelected(selected.guid), this);
                else if (selected.entity_type === Backend.ENTITY_TYPE.SIEGE) Core.push_mode(new SiegeSelected(selected.guid), this);
                else if (selected.entity_type === Backend.ENTITY_TYPE.BATTLE) Core.push_mode(new BattleSelected(selected.guid), this);
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
