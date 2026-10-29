// Ordinary gameplay: HUD, entity selection and orders. Selecting an own settlement enters the settlement selected mode, an own army the army selected mode.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_game_parameters, get_local_player_state, type EntityData } from "../gdo.js"
import { get_selected_entity_guid, get_selected_entity_view, set_selected_entity_guid } from "../input.js"
import { is_own_army, is_own_settlement } from "../main.js"
import * as GameUI from "../ui/game_ui.js"
import * as CalendarUI from "../ui/ui_element_calendar.js"
import * as PlayerStateUI from "../ui/ui_element_player_state.js"
import { show_entity } from "../ui/inspection/inspect_entity.js"
import { SettlementSelected } from "./settlement_selected.js"
import { ArmySelected } from "./army_selected.js"

const CARAVAN_PATH_COLOR = 'rgba(240, 200, 60, 0.9)';

const DEBUG_PANEL_HOTKEY = "Backquote"; // Shows / hides the debug entity text (clear of the camera keys).

export class MainPlay extends Core.Mode
{
    private debug_collapsed = true;
    private unsubscribe: (() => void)[] = [];

    on_entry(): void
    {
        this.unsubscribe = [GameUI.on("toggle_debug", () => this.toggle_debug_panel())];
    }

    on_exit(): void
    {
        for (const unsubscribe of this.unsubscribe) unsubscribe();
        this.unsubscribe = [];
        GameUI.submit(null);
        CalendarUI.submit(null);
        PlayerStateUI.submit(null);
        show_entity(null);
        Render.RENDER_CONFIG.outlined_entity = null;
        Render.RENDER_CONFIG.path_preview = null;
    }

    private toggle_debug_panel(): void
    {
        this.debug_collapsed = !this.debug_collapsed;
    }

    // Per-frame feed of the HUD: debug panel (re-queries the selected entity so its values stay live), calendar and player state (world state, not selection).
    private submit_hud(): void
    {
        const selected = get_selected_entity_view();
        GameUI.submit({ debug_text: selected !== null ? GameUI.entity_text(selected) : null, debug_collapsed: this.debug_collapsed });

        // The day is the world age in ticks over ticks_per_day, shown from day 1.
        const ticks_per_day = get_game_parameters().get_number("ticks_per_day");
        if (ticks_per_day === 0) CalendarUI.submit(null);
        else
        {
            const age = Backend.get_world_age();
            CalendarUI.submit({ day: Math.floor(age / ticks_per_day) + 1, progress: (age % ticks_per_day) / ticks_per_day });
        }

        const player = get_local_player_state();
        PlayerStateUI.submit({
            gold: player.get_number("treasury"),
            gold_per_day: player.get_number("income") - player.get_number("expenses"),
            settlements: player.get_number("settlement_count"),
            population: player.get_number("total_population"),
            soldiers: player.get_number("total_troops"),
        });
    }

    on_frame_update(delta_time_s: number): void
    {
        this.submit_hud();
        Render.RENDER_CONFIG.outlined_entity = get_selected_entity_guid();

        // Modes above this one set their own preview after this update (modes update bottom first).
        Render.RENDER_CONFIG.path_preview = null;
        if (!this.is_top) return;

        const selected = get_selected_entity_view();

        if (selected === null) show_entity(null);
        else if (is_own_settlement(selected)) Core.push_mode(new SettlementSelected(), this);
        else if (is_own_army(selected)) Core.push_mode(new ArmySelected(selected.guid), this);
        // Anything else (enemy or other entity types): read-only properties, no callbacks.
        else
        {
            show_entity(selected, {});
            if (selected.entity_type === Backend.ENTITY_TYPE.CARAVAN) this.update_caravan_path_preview(selected);
        }
    }

    // Shows the path of a selected caravan to its movement destination (queried from its tile, as for armies). None when it is not going anywhere.
    private update_caravan_path_preview(caravan: EntityData): void
    {
        const start = caravan.get_location("location");
        const destination = caravan.get_location("movement_destination");
        if (start.x === destination.x && start.y === destination.y) return;

        Render.RENDER_CONFIG.path_preview = { start, points: Backend.query_path(start, destination, 0, (caravan.get_number("flags") & Backend.CARAVAN_FLAG_IS_SHIP) !== 0 ? Backend.PATH_MODE_SEA : Backend.PATH_MODE_LAND), valid: true, color: CARAVAN_PATH_COLOR };
    }

    // Debug panel toggle.
    on_key_down(event: KeyboardEvent): boolean
    {
        if (event.code !== DEBUG_PANEL_HOTKEY) return false;

        if (!event.repeat) this.toggle_debug_panel();
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
