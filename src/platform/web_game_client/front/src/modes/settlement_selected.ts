// An own settlement is selected: play goes on as usual, and the action bar offers what the settlement can do.
// Leaves when the selection stops being an own settlement. Escape clears the selection.
// The bar state is kept here. While an action's mode is active (a child of this mode) its item shows as selected.

import * as Core from "../core.js"
import { get_selected_entity_guid, get_selected_entity_view, clear_selected_entity, send_command } from "../input.js"
import { submit_action_bar, type ActionBarItem, type ActionBarState } from "../ui/ui_element_action_bar.js"
import { submit_properties_panel } from "../ui/inspection/ui_element_properties_panel.js"
import { inspect_entity } from "../ui/inspection/inspect_entity.js"
import { is_own_settlement } from "../main.js"
import { RoadBuilding } from "./road_building.js"
import { ArmyRaising } from "./army_raising.js"

export class SettlementSelected extends Core.Mode
{
    private raise_item = this.make_item("Raise Army", 'Digit2', (guid, key) => new ArmyRaising(guid, key));

    private bar: ActionBarState = { items: [
        this.make_item("Build Road", 'Digit1', (guid, key) => new RoadBuilding(guid, key)),
        this.raise_item,
    ] };

    on_entry(): void
    {
        submit_action_bar(this.bar);
    }

    on_exit(): void
    {
        submit_action_bar(null);
        submit_properties_panel(null);
    }

    // The mode of the active action was left: nothing is selected anymore.
    on_resumed(): void
    {
        for (const item of this.bar.items) item.state = 'selectable';
        submit_action_bar(this.bar);
    }

    on_frame_update(delta_time_s: number): void
    {
        const view = get_selected_entity_view();
        if (!is_own_settlement(view) || view === null)
        {
            if (this.is_top) Core.pop_mode();
            return;
        }

        // The panel belongs to the raise army mode while it is active.
        if (this.raise_item.state === 'selected') return;

        // Lowering a ceiling below the pool discards the excess troops (the gauge warns).
        submit_properties_panel(inspect_entity(view, {
            on_ceiling: (troop_type, ceiling) => send_command("SET_SETTLEMENT_TROOP_CEILING", { settlement: view.guid, troop_type, ceiling }),
            on_tax_rate: (tax_rate) => send_command("SET_SETTLEMENT_TAX_RATE", { settlement: view.guid, tax_rate }),
        }));
    }

    on_escape(): boolean
    {
        clear_selected_entity();
        Core.pop_mode();
        return true;
    }

    // Clicks fall through to main play (selection).

    // Hotkeys of the bar items run the same action as their button.
    on_key_down(event: KeyboardEvent): boolean
    {
        const item = this.bar.items.find(candidate => candidate.hotkey === event.code);
        if (item === undefined) return false;

        if (!event.repeat) item.on_activate();
        return true;
    }

    // An item that starts a child mode. hotkey: shown on the button, also the key the child mode is left with.
    private make_item(name: string, hotkey: string, make_mode: (settlement_guid: number, exit_key: string) => Core.Mode): ActionBarItem
    {
        const item: ActionBarItem = { name, hotkey, state: 'selectable', on_activate: () => this.activate(item, hotkey, make_mode) };
        return item;
    }

    private activate(item: ActionBarItem, hotkey: string, make_mode: (settlement_guid: number, exit_key: string) => Core.Mode): void
    {
        if (item.state === 'unselectable') return;

        // Pressing the button of the active action leaves its mode (the child mode is on top, and resets the bar when popped).
        if (item.state === 'selected')
        {
            Core.pop_mode();
            return;
        }

        const guid = get_selected_entity_guid();
        if (guid === null) return;

        // Pops the mode of another action first if there is one (which resets the bar), then pushes the new one.
        Core.push_mode(make_mode(guid, hotkey), this);

        // The new mode may have left right away (then this mode resumed and reset the bar).
        if (this.is_top) return;

        for (const other of this.bar.items) other.state = other === item ? 'selected' : 'selectable';
        submit_action_bar(this.bar);
    }
}
