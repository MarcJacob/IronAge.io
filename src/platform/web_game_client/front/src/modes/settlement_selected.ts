// A settlement is selected (anyone's): play goes on as usual, and the action bar offers what the settlement can do.
// Items are disabled while the backend says their command would be refused (another player's settlement, under siege, limits...).
// While besieged, the panel has an "Under siege" row and the hint line says the actions are blocked.
// Leaves when the selection stops being a settlement. Escape clears the selection.
// The bar state is kept here. While an action's mode is active (a child of this mode) its item shows as selected.

import * as Core from "../core.js"
import { get_selected_entity_guid, get_selected_entity_view, clear_selected_entity, send_command, is_command_valid } from "../input.js"
import { query_entity, type EntityData } from "../gdo.js"
import * as Backend from "../backend.js"
import { submit_action_bar, type ActionBarItem, type ActionBarState } from "../ui/ui_element_action_bar.js"
import { submit_properties_panel } from "../ui/ui_element_properties_panel.js"
import { submit_hint } from "../ui/ui_element_hint.js"
import { settlement_panel } from "../ui/entity_panels.js"
import { RoadBuilding } from "./road_building.js"
import { ArmyRaising } from "./army_raising.js"

// An action of the bar: its item, and whether the backend would currently accept its command for the settlement.
interface SettlementAction
{
    item: ActionBarItem;
    is_valid: (settlement: EntityData) => boolean;
}

export class SettlementSelected extends Core.Mode
{
    // The validity checks probe each command with placeholder values that pass every other check (a road to a neighbour tile,
    // one man-at-arms raised on the settlement's tile), so only what the settlement itself allows decides.
    private actions: SettlementAction[] = [
        { item: this.make_item("Build Road", 'Digit1', (guid, key) => new RoadBuilding(guid, key)),
            is_valid: settlement => {
                const location = settlement.get_location("location");
                return is_command_valid("BUILD_ROAD", { settlement: settlement.guid, origin: location, destination: { x: location.x === 0 ? 1 : location.x - 1, y: location.y } });
            } },
        { item: this.make_item("Raise Army", 'Digit2', (guid, key) => new ArmyRaising(guid, key)),
            is_valid: settlement => is_command_valid("RAISE_ARMY", { settlement: settlement.guid, raise_location: settlement.get_location("location"), target_men_at_arms: 1 }) },
    ];
    private bar: ActionBarState = { items: this.actions.map(action => action.item) };
    private hintText = ""; // The siege hint as submitted ("" for none).

    on_entry(): void
    {
        submit_action_bar(this.bar);
    }

    on_exit(): void
    {
        submit_hint(null);
        submit_action_bar(null);
        submit_properties_panel(null);
    }

    // The mode of the active action was left: nothing is selected anymore (the frame update greys out what is refused). It cleared the hint.
    on_resumed(): void
    {
        for (const item of this.bar.items) if (item.state === 'selected') item.state = 'selectable';
        submit_action_bar(this.bar);
        this.hintText = "";
    }

    on_frame_update(delta_time_s: number): void
    {
        const view = get_selected_entity_view();
        if (view === null || view.entity_type !== Backend.ENTITY_TYPE.SETTLEMENT)
        {
            if (this.is_top) Core.pop_mode();
            return;
        }

        this.update_item_states(view);

        // While an action's mode is active its hint is shown instead.
        const siegeGuid = view.get_number("siege");
        const hint = siegeGuid !== 0 ? "Settlement under siege: building and raising are blocked until the siege ends." : "";
        if (this.is_top && hint !== this.hintText)
        {
            this.hintText = hint;
            submit_hint(hint !== "" ? { text: hint } : null);
        }

        // Kept shown while an action's mode is active: actions use the advanced action panel.
        // Lowering a ceiling below the pool discards the excess troops (the gauge warns). Sliders are interactive only while the backend accepts a ceiling change.
        const canSetCeiling = is_command_valid("SET_SETTLEMENT_TROOP_CEILING", { settlement: view.guid, troop_type: 0, ceiling: 0 });
        submit_properties_panel(settlement_panel(view,
            canSetCeiling ? ((troop_type: number, ceiling: number) => send_command("SET_SETTLEMENT_TROOP_CEILING", { settlement: view.guid, troop_type, ceiling })) : undefined,
            siegeGuid !== 0 ? query_entity(siegeGuid) : null));
    }

    // Greys out the items whose command would be refused, keeps the active one selected. Resubmits the bar only when a state changed.
    private update_item_states(settlement: EntityData): void
    {
        let changed = false;
        for (const action of this.actions)
        {
            if (action.item.state === 'selected') continue;

            const state = action.is_valid(settlement) ? 'selectable' : 'unselectable';
            if (state !== action.item.state) { action.item.state = state; changed = true; }
        }
        if (changed) submit_action_bar(this.bar);
    }

    on_escape(): boolean
    {
        clear_selected_entity();
        Core.pop_mode();
        return true;
    }

    // Clicks fall through to main play (selection).

    // Hotkeys of the bar items run the same action as their button (disabled items do nothing).
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
