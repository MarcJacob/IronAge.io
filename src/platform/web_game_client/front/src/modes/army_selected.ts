// An own army is selected: play goes on as usual, and a bottom bar offers what the army can do (settle, no hotkey).
// Leaves when the selection stops being an own army. Escape clears the selection.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { get_selected_entity_guid, clear_selected_entity, send_command } from "../input.js"
import { get_local_player_id, is_own_army } from "../main.js"
import { show_army_bar, hide_settlement_bar } from "../ui/settlement_bar_ui.js"

export class ArmySelected extends Core.Mode
{
    // army_guid: the selected own army this mode is about.
    constructor(private army_guid: number) { super(); }

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

        if (!is_own_army(Backend.query_entity(this.army_guid)) || get_selected_entity_guid() !== this.army_guid)
        {
            Core.pop_mode();
            return;
        }

        this.update_path_preview();
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

        const army = Backend.query_entity(this.army_guid);
        const hovered = Render.get_hovered_tile();
        if (army === null || hovered === null) return;

        const start = { x: army.location_x, y: army.location_y };
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

    private settle(): void
    {
        send_command("FOUND_SETTLEMENT", { army: this.army_guid });
    }
}
