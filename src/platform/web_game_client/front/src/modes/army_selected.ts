// An own army is selected: play goes on as usual, and a bottom bar offers what the army can do (settle, no hotkey).
// Leaves when the selection stops being an own army. Escape clears the selection.

import * as Core from "../core.js"
import { get_selected_entity_guid, get_selected_entity_view, clear_selected_entity, send_found_settlement } from "../input.js"
import { show_army_bar, hide_settlement_bar } from "../ui/settlement_bar_ui.js"
import { is_own_army, play_click, play_context_menu, update_army_path_preview } from "./play_common.js"

export class ArmySelected extends Core.Mode
{
    on_entry(): void
    {
        show_army_bar(() => this.settle());
    }

    on_exit(): void
    {
        hide_settlement_bar();
    }

    on_frame_update(delta_time_s: number): void
    {
        if (!this.is_top) return;

        if (!is_own_army(get_selected_entity_view()))
        {
            Core.pop_mode();
            return;
        }

        // Main play only previews the army's movement while it is the top mode.
        update_army_path_preview();
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

    private settle(): void
    {
        const guid = get_selected_entity_guid();
        if (guid !== null) send_found_settlement(guid);
    }
}
