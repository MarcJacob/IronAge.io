// An own settlement is selected: play goes on as usual, and the action bar offers what the settlement can do.
// Leaves when the selection stops being an own settlement. Escape clears the selection.
// The bar state is kept here. While an action's mode is active (a child of this mode) its item shows as selected.

import * as Core from "../core.js"
import { get_selected_entity_guid, get_selected_entity_view, clear_selected_entity, send_command, is_command_valid } from "../input.js"
import * as ActionBarUI from "../ui/ui_element_action_bar.js"
import * as SettlementPanelUI from "../ui/inspection/ui_element_settlement_panel.js"
import { show_entity } from "../ui/inspection/inspect_entity.js"
import { is_own_settlement } from "../main.js"
import { RoadBuilding } from "./road_building.js"
import { ArmyRaising } from "./army_raising.js"
import { SettlementFounding } from "./settlement_founding.js"

// A bar item that starts a child mode. hotkey: shown on the button, also the key the child mode is left with.
interface SettlementAction extends ActionBarUI.ActionBarItem
{
    hotkey: string;
    make_mode: (settlement_guid: number, exit_key: string) => Core.Mode;
}

export class SettlementSelected extends Core.Mode
{
    private raise_item = this.make_item("raise_army", "Raise Army", 'Digit2', (guid, key) => new ArmyRaising(guid, key));

    private found_item = this.make_item("found_settlement", "Found Settlement", 'Digit3', (guid, key) => new SettlementFounding(guid, key));

    private bar: { items: SettlementAction[] } = { items: [
        this.make_item("build_road", "Build Road", 'Digit1', (guid, key) => new RoadBuilding(guid, key)),
        this.raise_item,
        this.found_item,
    ] };

    private unsubscribe: (() => void)[] = [];

    on_entry(): void
    {
        // The panel's settings and build buttons apply to the selected settlement (the panel only shows it while it is selected).
        const send_to_selected = (name: string, props: { [name: string]: number }) => {
            const settlement = get_selected_entity_guid();
            if (settlement !== null) send_command(name, { settlement, ...props });
        };

        this.unsubscribe = [
            ActionBarUI.on("action", id => {
                const item = this.bar.items.find(candidate => candidate.id === id);
                if (item !== undefined) this.activate(item);
            }),
            SettlementPanelUI.on("tax_rate", tax_rate => send_to_selected("SET_SETTLEMENT_TAX_RATE", { tax_rate })),
            SettlementPanelUI.on("export_ratio", export_ratio => send_to_selected("SET_EXPORT_RATIO", { export_ratio })),
            // Lowering a ceiling below the pool discards the excess troops (the gauge warns).
            SettlementPanelUI.on("ceiling", (troop_type, ceiling) => send_to_selected("SET_SETTLEMENT_TROOP_CEILING", { troop_type, ceiling })),
            SettlementPanelUI.on("build", building_type => send_to_selected("BUILD_SETTLEMENT_BUILDING", { building_type })),
        ];
        ActionBarUI.submit(this.bar);
    }

    on_exit(): void
    {
        for (const unsubscribe of this.unsubscribe) unsubscribe();
        this.unsubscribe = [];
        ActionBarUI.submit(null);
        show_entity(null);
    }

    // The mode of the active action was left: nothing is selected anymore.
    on_resumed(): void
    {
        for (const item of this.bar.items) item.state = 'selectable';
        ActionBarUI.submit(this.bar);
    }

    on_frame_update(delta_time_s: number): void
    {
        const view = get_selected_entity_view();
        if (!is_own_settlement(view) || view === null)
        {
            if (this.is_top) Core.pop_mode();
            return;
        }

        // The panel belongs to the raise army and found settlement modes while they are active.
        if (this.raise_item.state === 'selected' || this.found_item.state === 'selected') return;

        // Own settlement: editable panel (events subscribed in on_entry), build buttons enabled when the project would be accepted.
        show_entity(view, {
            edit_settlement: true,
            buildable: view.get_array("building_counts").map((_, building_type) => is_command_valid("BUILD_SETTLEMENT_BUILDING", { settlement: view.guid, building_type })),
        });
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

        if (!event.repeat) this.activate(item);
        return true;
    }

    private make_item(id: string, name: string, hotkey: string, make_mode: SettlementAction["make_mode"]): SettlementAction
    {
        return { id, name, hotkey, state: 'selectable', make_mode };
    }

    private activate(item: SettlementAction): void
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
        Core.push_mode(item.make_mode(guid, item.hotkey), this);

        // The new mode may have left right away (then this mode resumed and reset the bar).
        if (this.is_top) return;

        for (const other of this.bar.items) other.state = other === item ? 'selected' : 'selectable';
        ActionBarUI.submit(this.bar);
    }
}
