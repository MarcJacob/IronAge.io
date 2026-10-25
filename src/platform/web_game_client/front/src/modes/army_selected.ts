// An own army is selected: play goes on as usual, and a bottom bar offers what the army can do (stop raising while raising, disband; no hotkeys).
// Leaves when the selection stops being an own army. Escape clears the selection.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, clear_selected_entity, send_command, is_command_valid } from "../input.js"
import { query_entity, type EntityData } from "../gdo.js"
import { get_local_player_id, is_own_army } from "../main.js"
import * as ActionBarUI from "../ui/ui_element_action_bar.js"
import * as ArmyPanelUI from "../ui/inspection/ui_element_army_panel.js"
import { show_entity } from "../ui/inspection/inspect_entity.js"
import { raise_target_props } from "./army_raising.js"

const ARMY_PATH_COLOR_MOVE = 'rgba(40, 220, 60, 0.9)';
const ARMY_PATH_COLOR_ATTACK = 'rgba(255, 40, 40, 0.9)';

export class ArmySelected extends Core.Mode
{
    // army_guid: the selected own army this mode is about.
    constructor(private army_guid: number) { super(); }

    private raising: boolean | null = null; // Whether the bar currently offers to stop raising (null: no bar submitted yet).
    private can_disband: boolean | null = null; // Whether the bar currently shows Disband as selectable.

    private stop_item: ActionBarUI.ActionBarItem = { id: "stop_raising", name: "Stop raising", state: 'selectable' };

    // Disband is refused (greyed out) unless the army is in its home settlement's territory.
    private disband_item: ActionBarUI.ActionBarItem = { id: "disband", name: "Disband", state: 'unselectable' };

    private unsubscribe: (() => void)[] = [];

    on_entry(): void
    {
        this.unsubscribe = [
            ActionBarUI.on("action", id => {
                if (id === this.stop_item.id) send_command("STOP_RAISING_ARMY", { army: this.army_guid });
                else if (id === this.disband_item.id) send_command("DISBAND_ARMY", { army: this.army_guid });
            }),
            ArmyPanelUI.on("raise_targets", targets => send_command("SET_ARMY_RAISE_TARGETS", { army: this.army_guid, ...raise_target_props(targets) })),
        ];
    }

    on_exit(): void
    {
        for (const unsubscribe of this.unsubscribe) unsubscribe();
        this.unsubscribe = [];
        ActionBarUI.submit(null);
        show_entity(null);
    }

    on_frame_update(delta_time_s: number): void
    {
        if (!this.is_top) return;

        const army = query_entity(this.army_guid);
        if (army === null || !is_own_army(army) || get_selected_entity_guid() !== this.army_guid)
        {
            Core.pop_mode();
            return;
        }

        this.update_panel(army);
        this.update_path_preview();
    }

    // The bar offers to stop raising while the army is raising, and to disband it (when allowed). The panel's target sliders (while raising) send the new targets.
    private update_panel(army: EntityData): void
    {
        const raising = army.get_number("raising") === 1;
        const canDisband = is_command_valid("DISBAND_ARMY", { army: this.army_guid });
        if (raising !== this.raising || canDisband !== this.can_disband)
        {
            this.raising = raising;
            this.can_disband = canDisband;
            this.disband_item.state = canDisband ? 'selectable' : 'unselectable';
            ActionBarUI.submit({ items: raising ? [this.stop_item, this.disband_item] : [this.disband_item] });
        }

        show_entity(army, { edit_raise_targets: true });
    }

    on_escape(): boolean
    {
        clear_selected_entity();
        Core.pop_mode();
        return true;
    }

    // Left clicks fall through to main play (selection).

    // Right click: attack the entity not owned by the local player under the cursor if there is one, otherwise move to the clicked tile.
    on_context_menu(event: MouseEvent): boolean
    {
        const localPlayerId = get_local_player_id();
        if (localPlayerId === null) return true;

        const targetGuid = Render.pick_entity_at(event.clientX, event.clientY, this.army_guid, localPlayerId);
        if (targetGuid !== null)
        {
            send_command("SET_ENTITY_ATTACK_TARGET", { attacker_entity: this.army_guid, target_entity: targetGuid });
            return true;
        }

        const tile = this.get_move_tile(Render.page_to_tile(event.clientX, event.clientY));
        if (tile !== null) send_command("SET_ENTITY_MOVE_TARGET", { entity: this.army_guid, move_target: tile });
        return true;
    }

    // Shows the path the army is following to its destination: green while moving, red while chasing an attack target. None when it is not going anywhere.
    // The path is queried from the army's tile to its destination, as the simulation does at every step.
    private update_path_preview(): void
    {
        Render.RENDER_CONFIG.path_preview = null;

        const army = query_entity(this.army_guid);
        if (army === null) return;

        const start = army.get_location("location");
        const destination = army.get_location("movement_destination");
        if (start.x === destination.x && start.y === destination.y) return;

        const attacking = army.get_number("action_target") !== 0;
        Render.RENDER_CONFIG.path_preview = { start, points: Backend.query_path(start, destination), valid: true, color: attacking ? ARMY_PATH_COLOR_ATTACK : ARMY_PATH_COLOR_MOVE };
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
}
