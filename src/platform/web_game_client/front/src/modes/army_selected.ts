// An army is selected (anyone's): play goes on as usual, and a bottom bar offers what the army can do (settle, stop raising, detach, detach from siege, disband; no hotkeys).
// The properties panel lists its warbands; the selection made there is kept here. Leaves when the selection stops being this army. Escape clears the selection.
// Another player's army: same panel and bar, every item disabled, no orders, no hint, no path preview.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, clear_selected_entity, send_command, is_command_valid } from "../input.js"
import { query_entity, type EntityData } from "../gdo.js"
import { get_local_player_id, is_own_army } from "../main.js"
import { submit_action_bar, type ActionBarItem, type ActionBarState } from "../ui/ui_element_action_bar.js"
import { submit_properties_panel } from "../ui/ui_element_properties_panel.js"
import { submit_hint } from "../ui/ui_element_hint.js"
import { raise_target_props } from "../ui/troop_gauges.js"
import { army_panel, type WarbandView } from "../ui/entity_panels.js"

// Path preview colors of the army's current order.
const PATH_COLOR_MOVE = 'rgba(60, 220, 60, 0.9)';
const PATH_COLOR_ATTACK = 'rgba(255, 40, 40, 0.9)';

export class ArmySelected extends Core.Mode
{
    // army_guid: the selected army this mode is about.
    constructor(private army_guid: number) { super(); }

    private selected = new Set<number>(); // Warbands selected in the panel's list, by guid (slots shift when warbands leave).
    private anchor = 0; // Warband guid of the last plain or Ctrl click: where a Shift click range starts.
    private barKey = ""; // What the submitted bar was made for (see update_bar).
    private ownHint: boolean | null = null; // Whether the submitted hint is the owner's (null before the first frame).

    private bar: ActionBarState = { items: [] };

    on_exit(): void
    {
        submit_hint(null);
        submit_action_bar(null);
        submit_properties_panel(null);
    }

    on_frame_update(delta_time_s: number): void
    {
        if (!this.is_top) return;

        const army = query_entity(this.army_guid);
        if (army == null || get_selected_entity_guid() !== this.army_guid)
        {
            Core.pop_mode();
            return;
        }

        const own = is_own_army(army);
        if (own !== this.ownHint)
        {
            this.ownHint = own;
            submit_hint(own ? { text: "Army: right click to move or attack (a siege: join it). Right click an own army to accompany it, Shift + right click to merge into it."
                + " Disband only inside home territory." } : null);
        }

        this.update_panel(army, own);
        if (own) this.update_path_preview(army);
    }

    // Army stats, warband list or troop gauges (see army_panel), and the bar. own: the army is the local player's (actions enabled).
    // Raising is per warband: the target sliders (single warband army only) and "Stop raising" are about the focus warband (the only one, or the only one selected).
    private update_panel(army: EntityData, own: boolean): void
    {
        const warbands: WarbandView[] = [];
        for (let slot = 0; slot < army.get_number("warband_count"); slot++)
        {
            const warband = query_entity(army.get_number(`warband_${slot}`));
            if (warband !== null) warbands.push({ warband, home: query_entity(warband.get_number("home_settlement")) });
        }

        // Forget selected warbands that left the army.
        for (const guid of this.selected) if (!warbands.some(view => view.warband.guid === guid)) this.selected.delete(guid);

        const focus = warbands.length === 1 ? warbands[0]
            : this.selected.size === 1 ? warbands.find(view => this.selected.has(view.warband.guid)) ?? null
            : null;
        const focusGuid = focus?.warband.guid ?? 0;

        this.update_bar(army, focus !== null && focus.warband.get_number("raising") === 1 ? focusGuid : 0, this.selected.size > 0 && this.selected.size < warbands.length, own);

        submit_properties_panel(army_panel(army, warbands, this.selected,
            (guid, event) => this.click_row(warbands, guid, event),
            own ? ((targets: number[]) => send_command("SET_ARMY_RAISE_TARGETS", { warband: focusGuid, ...raise_target_props(targets) })) : undefined));
    }

    // List selection, like a file manager: a click selects only that warband, Ctrl + click toggles it,
    // Shift + click selects the warbands from the anchor (last plain or Ctrl click) to it. warbands: the list as shown when clicked.
    private click_row(warbands: WarbandView[], guid: number, event: MouseEvent): void
    {
        const index = warbands.findIndex(view => view.warband.guid === guid);
        const anchorIndex = warbands.findIndex(view => view.warband.guid === this.anchor);
        if (event.shiftKey && index >= 0 && anchorIndex >= 0)
        {
            this.selected.clear();
            for (let i = Math.min(index, anchorIndex); i <= Math.max(index, anchorIndex); i++) this.selected.add(warbands[i].warband.guid);
            return;
        }

        if (event.ctrlKey || event.metaKey)
        {
            if (!this.selected.delete(guid)) this.selected.add(guid);
        }
        else
        {
            this.selected.clear();
            this.selected.add(guid);
        }
        this.anchor = guid;
    }

    // Settle, then "Stop raising" while raising_warband (guid, 0 for none) raises, then "Detach" while some but not all warbands are selected,
    // then "Detach from siege" while the army is in a siege, then "Disband" (disabled while refused: only inside home territory, not in a battle).
    // own: false disables the owner's items (another player's army). "Detach from siege" asks the backend (the siege commander may also detach it).
    // Submitted only when the items change.
    private update_bar(army: EntityData, raising_warband: number, can_detach: boolean, own: boolean): void
    {
        const ownState = own ? 'selectable' : 'unselectable';
        const items: ActionBarItem[] = [{ name: "Settle", state: ownState, on_activate: () => this.settle() }];
        if (raising_warband !== 0) items.push({ name: "Stop raising", state: ownState, on_activate: () => send_command("STOP_RAISING_ARMY", { warband: raising_warband }) });
        if (can_detach) items.push({ name: "Detach", state: ownState, on_activate: () => this.detach() });
        if (army.get_number("siege") !== 0) items.push({ name: "Detach from siege", state: is_command_valid("DETACH_FROM_SIEGE", { army: this.army_guid }) ? 'selectable' : 'unselectable',
            on_activate: () => send_command("DETACH_FROM_SIEGE", { army: this.army_guid }) });
        items.push({ name: "Disband", state: is_command_valid("DISBAND_ARMY", { army: this.army_guid }) ? 'selectable' : 'unselectable',
            on_activate: () => send_command("DISBAND_ARMY", { army: this.army_guid }) });

        const key = items.map(item => item.name + ":" + item.state).join("|") + "|" + raising_warband;
        if (key === this.barKey) return;

        this.barKey = key;
        this.bar.items = items;
        submit_action_bar(this.bar);
    }

    // Detaches the selected warbands into a new army. The slot mask is built from a fresh army snapshot (slots shift when warbands leave).
    private detach(): void
    {
        const army = query_entity(this.army_guid);
        if (army === null) return;

        let slotsLow = 0, slotsHigh = 0;
        for (let slot = 0; slot < army.get_number("warband_count"); slot++)
        {
            if (!this.selected.has(army.get_number(`warband_${slot}`))) continue;
            if (slot < 32) slotsLow |= 1 << slot;
            else slotsHigh |= 1 << (slot - 32);
        }
        send_command("DETACH_WARBANDS", { army: this.army_guid, slots_low: slotsLow, slots_high: slotsHigh });
        this.selected.clear();
    }

    on_escape(): boolean
    {
        clear_selected_entity();
        Core.pop_mode();
        return true;
    }

    // Left clicks fall through to main play (selection).

    // Right click: attack the entity not owned by the local player under the cursor if there is one (a siege: its settlement). Otherwise, on another own army:
    // accompany it (an attack order on an own army), or merge into it with Shift held. Otherwise move to the clicked tile.
    // Also works while the army is raising: it keeps the new target and heads there once raised. Another player's army takes no orders (falls through).
    on_context_menu(event: MouseEvent): boolean
    {
        if (!is_own_army(query_entity(this.army_guid))) return false;

        const localPlayerId = get_local_player_id();
        if (localPlayerId === null) return true;

        let targetGuid = Render.pick_entity_at(event.clientX, event.clientY, this.army_guid, localPlayerId);
        if (targetGuid !== null)
        {
            // A siege stands for its settlement: attacking the besieged settlement joins the siege.
            const target = query_entity(targetGuid);
            if (target !== null && target.entity_type === Backend.ENTITY_TYPE.SIEGE) targetGuid = target.get_number("settlement");

            send_command("SET_ENTITY_ATTACK_TARGET", { attacker_entity: this.army_guid, target_entity: targetGuid });
            return true;
        }

        const ownGuid = Render.pick_entity_at(event.clientX, event.clientY, this.army_guid);
        if (ownGuid !== null && ownGuid !== this.army_guid && is_own_army(query_entity(ownGuid)))
        {
            if (event.shiftKey) send_command("MERGE_ARMY", { army: this.army_guid, into_army: ownGuid });
            else send_command("SET_ENTITY_ATTACK_TARGET", { attacker_entity: this.army_guid, target_entity: ownGuid });
            return true;
        }

        const tile = this.get_move_tile(Render.page_to_tile(event.clientX, event.clientY));
        if (tile !== null) send_command("SET_ENTITY_MOVE_TARGET", { entity: this.army_guid, move_target: tile });
        return true;
    }

    // Previews where the army is currently heading: red while attacking (its state), green otherwise (moving, accompanying, merging...).
    // A raising army shows the destination it will head to once raised. Nothing while it stands on its destination.
    private update_path_preview(army: EntityData): void
    {
        Render.RENDER_CONFIG.path_preview = null;

        const start = army.get_location("location");
        const destination = army.get_location("movement_destination");
        if (start.x === destination.x && start.y === destination.y) return;

        const attacking = army.get_number("state") === Backend.ARMY_STATE.ATTACKING;
        Render.RENDER_CONFIG.path_preview = { start, points: Backend.query_path(start, destination), valid: true, color: attacking ? PATH_COLOR_ATTACK : PATH_COLOR_MOVE };
    }

    // The tile a move order to the given clicked tile goes to: clamped inside the world. Return null instead to ignore clicks outside the world.
    private get_move_tile(tile: { x: number, y: number }): { x: number, y: number } | null
    {
        const state = Backend.LAST_RENDER_STATE;
        if (state === null) return null;

        return {
            x: Math.max(0, Math.min(tile.x, state.world_size.width - 1)),
            y: Math.max(0, Math.min(tile.y, state.world_size.height - 1)),
        };
    }

    private settle(): void
    {
        send_command("FOUND_SETTLEMENT", { army: this.army_guid });
    }
}
