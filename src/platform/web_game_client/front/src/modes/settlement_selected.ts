// An own settlement is selected: play goes on as usual, and a bottom bar offers what the settlement can do.
// Leaves when the selection stops being an own settlement. Escape clears the selection.

import * as Core from "../core.js"
import { get_selected_entity_guid, get_selected_entity_view, clear_selected_entity } from "../input.js"
import { show_settlement_bar, hide_settlement_bar } from "../ui/settlement_bar_ui.js"
import { is_own_settlement, play_click, play_context_menu } from "./play_common.js"
import { RoadBuilding } from "./road_building.js"
import { ArmyRaising } from "./army_raising.js"

const KEY_BUILD_ROAD = 'Digit1';
const KEY_RAISE_ARMY = 'Digit2';

export class SettlementSelected extends Core.Mode
{
    on_entry(): void
    {
        show_settlement_bar(() => this.start_road_building(), () => this.start_army_raising());
    }

    on_exit(): void
    {
        hide_settlement_bar();
    }

    // A child mode takes over the bottom of the screen.
    on_suspended(): void
    {
        hide_settlement_bar();
    }

    on_resumed(): void
    {
        show_settlement_bar(() => this.start_road_building(), () => this.start_army_raising());
    }

    on_frame_update(delta_time_s: number): void
    {
        if (!this.is_top) return;

        if (!is_own_settlement(get_selected_entity_view())) Core.pop_mode();
    }

    on_escape(): void
    {
        clear_selected_entity();
        Core.pop_mode();
    }

    on_click(event: MouseEvent): void
    {
        play_click(event);
    }

    on_context_menu(event: MouseEvent): void
    {
        play_context_menu(event);
    }

    on_key_down(event: KeyboardEvent): void
    {
        if (event.repeat) return;
        if (event.code === KEY_BUILD_ROAD) this.start_road_building();
        else if (event.code === KEY_RAISE_ARMY) this.start_army_raising();
    }

    private start_road_building(): void
    {
        const guid = get_selected_entity_guid();
        if (guid !== null) Core.push_mode(new RoadBuilding(guid, KEY_BUILD_ROAD));
    }

    private start_army_raising(): void
    {
        const guid = get_selected_entity_guid();
        if (guid !== null) Core.push_mode(new ArmyRaising(guid, KEY_RAISE_ARMY));
    }
}
