// An own army is selected: play goes on as usual, and a bottom bar offers what the army can do (stop raising, no hotkey; no bar otherwise).
// Leaves when the selection stops being an own army. Escape clears the selection.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, clear_selected_entity, send_command } from "../input.js"
import { query_entity, type EntityData } from "../gdo.js"
import { get_local_player_id, is_own_army } from "../main.js"
import { submit_action_bar, type ActionBarState } from "../ui/ui_element_action_bar.js"
import { submit_properties_panel } from "../ui/inspection/ui_element_properties_panel.js"
import { inspect_entity } from "../ui/inspection/inspect_entity.js"
import { raise_target_props } from "./army_raising.js"

export class ArmySelected extends Core.Mode
{
    // army_guid: the selected own army this mode is about.
    constructor(private army_guid: number) { super(); }

    private raising = false; // Whether the bar currently offers to stop raising.

    private bar: ActionBarState = { items: [
        { name: "Stop raising", state: 'selectable', on_activate: () => send_command("STOP_RAISING_ARMY", { army: this.army_guid }) },
    ] };

    on_exit(): void
    {
        submit_action_bar(null);
        submit_properties_panel(null);
    }

    on_frame_update(delta_time_s: number): void
    {
        if (!this.is_top) return;

        const army = query_entity(this.army_guid);
        if (!is_own_army(army) || get_selected_entity_guid() !== this.army_guid)
        {
            Core.pop_mode();
            return;
        }

        this.update_panel(army);
        this.update_path_preview();
    }

    // The bar offers to stop raising while the army is raising. The panel's target sliders (while raising) send the new targets.
    private update_panel(army: EntityData): void
    {
        const raising = army.get_number("raising") === 1;
        if (raising !== this.raising)
        {
            this.raising = raising;
            submit_action_bar(raising ? this.bar : null);
        }

        submit_properties_panel(inspect_entity(army, {
            on_raise_targets: targets => send_command("SET_ARMY_RAISE_TARGETS", { army: this.army_guid, ...raise_target_props(targets) }),
        }));
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

    // Previews the path from the army to the hovered tile (movement). No preview while the mouse is off the map.
    private update_path_preview(): void
    {
        Render.RENDER_CONFIG.path_preview = null;

        const army = query_entity(this.army_guid);
        const hovered = Render.get_hovered_tile();
        if (army === null || hovered === null) return;

        const start = army.get_location("location");
        Render.RENDER_CONFIG.path_preview = { start, points: Backend.query_path(start, hovered), valid: true };
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
