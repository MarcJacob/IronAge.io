// A battle is selected (anyone's): the properties panel shows both sides (see battle_panel). The action bar is shown, empty for now (battles take no orders yet).
// Leaves when the selection stops being this battle (or the battle ends). Escape clears the selection.

import * as Core from "../core.js"
import { get_selected_entity_guid, clear_selected_entity } from "../input.js"
import { query_entity } from "../gdo.js"
import { submit_action_bar } from "../ui/ui_element_action_bar.js"
import { submit_properties_panel } from "../ui/ui_element_properties_panel.js"
import { battle_panel } from "../ui/entity_panels.js"

export class BattleSelected extends Core.Mode
{
    // battle_guid: the selected battle this mode is about.
    constructor(private battle_guid: number) { super(); }

    on_entry(): void
    {
        submit_action_bar({ items: [] });
    }

    on_exit(): void
    {
        submit_action_bar(null);
        submit_properties_panel(null);
    }

    on_frame_update(delta_time_s: number): void
    {
        if (!this.is_top) return;

        const battle = query_entity(this.battle_guid);
        if (battle === null || get_selected_entity_guid() !== this.battle_guid)
        {
            Core.pop_mode();
            return;
        }

        submit_properties_panel(battle_panel(battle));
    }

    on_escape(): boolean
    {
        clear_selected_entity();
        Core.pop_mode();
        return true;
    }

    // Clicks fall through to main play (selection).
}
