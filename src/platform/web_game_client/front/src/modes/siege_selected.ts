// A siege is selected (anyone's): the properties panel shows both sides. Its commander gets Assault and Lift siege, its defender Sally out,
// other players only the panel. Items are disabled while the backend says the command would be refused (e.g. a battle in progress).
// Leaves when the selection stops being this siege. Escape clears the selection.

import * as Core from "../core.js"
import { get_selected_entity_guid, clear_selected_entity, send_command, is_command_valid } from "../input.js"
import { query_entity, type EntityData } from "../gdo.js"
import { get_local_player_id } from "../main.js"
import { submit_action_bar, type ActionBarItem } from "../ui/ui_element_action_bar.js"
import { submit_properties_panel } from "../ui/ui_element_properties_panel.js"
import { submit_hint } from "../ui/ui_element_hint.js"
import { siege_panel } from "../ui/entity_panels.js"

export class SiegeSelected extends Core.Mode
{
    // siege_guid: the selected siege this mode is about.
    constructor(private siege_guid: number) { super(); }

    private barKey = ""; // What the submitted bar and hint were made for (see update_bar).

    on_exit(): void
    {
        submit_hint(null);
        submit_action_bar(null);
        submit_properties_panel(null);
    }

    on_frame_update(delta_time_s: number): void
    {
        if (!this.is_top) return;

        const siege = query_entity(this.siege_guid);
        if (siege === null || get_selected_entity_guid() !== this.siege_guid)
        {
            Core.pop_mode();
            return;
        }

        submit_properties_panel(siege_panel(siege));
        this.update_bar(siege);
    }

    // The local player's actions on the siege, and a hint saying what they do. Nothing for other players. Submitted only when the items change.
    private update_bar(siege: EntityData): void
    {
        const localPlayerId = get_local_player_id();
        const items: ActionBarItem[] = [];
        let hint: string | null = null;

        if (localPlayerId === siege.get_number("commander"))
        {
            items.push(this.make_item("Assault", "ASSAULT_SIEGE"), this.make_item("Lift siege", "LIFT_SIEGE"));
            hint = "Siege: Assault attacks the defenders, whose defense is doubled. Lift siege frees your armies.";
        }
        else if (localPlayerId === siege.get_number("defender"))
        {
            items.push(this.make_item("Sally out", "SALLY_OUT"));
            hint = "Your settlement is besieged: Sally out attacks the besiegers (no defense bonus).";
        }

        const key = items.map(item => item.name + ":" + item.state).join("|");
        if (key === this.barKey) return;

        this.barKey = key;
        submit_action_bar(items.length > 0 ? { items } : null);
        submit_hint(hint !== null ? { text: hint } : null);
    }

    // An item sending the named command about this siege, disabled while the command would be refused.
    private make_item(name: string, command: string): ActionBarItem
    {
        const props = { siege: this.siege_guid };
        return { name, state: is_command_valid(command, props) ? 'selectable' : 'unselectable', on_activate: () => send_command(command, props) };
    }

    on_escape(): boolean
    {
        clear_selected_entity();
        Core.pop_mode();
        return true;
    }

    // Clicks fall through to main play (selection).
}
