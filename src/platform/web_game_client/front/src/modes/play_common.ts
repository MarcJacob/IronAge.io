// What the play modes (main play and the modes built on it) share: selection, orders, and the movement path preview.

import * as Render from "../render.js"
import * as Backend from "../backend.js"
import { get_selected_entity_guid, get_selected_entity_view, set_selected_entity_guid, send_context_order } from "../input.js"
import { get_local_player_id } from "../main.js"

// Whether the view is of a settlement of the local player.
export function is_own_settlement(view: Backend.EntityView | null): boolean
{
    return view !== null && view.entity_type === Backend.ENTITY_TYPE.SETTLEMENT && view.owner === get_local_player_id();
}

// Whether the view is of an army of the local player.
export function is_own_army(view: Backend.EntityView | null): boolean
{
    return view !== null && view.entity_type === Backend.ENTITY_TYPE.ARMY && view.owner === get_local_player_id();
}

// Left click: selects the entity under the cursor. Empty space clears the selection.
export function play_click(event: MouseEvent): void
{
    if (event.button !== 0) return;

    set_selected_entity_guid(Render.pick_entity_at(event.clientX, event.clientY, get_selected_entity_guid()));
}

// Right click: order for the selection (attack or move).
export function play_context_menu(event: MouseEvent): void
{
    send_context_order(event.clientX, event.clientY);
}

// With an army selected, previews the path from it to the hovered tile (movement). No preview otherwise.
export function update_army_path_preview(): void
{
    Render.RENDER_CONFIG.path_preview = null;

    const selected = get_selected_entity_view();
    if (selected === null || selected.entity_type !== Backend.ENTITY_TYPE.ARMY) return;

    const hovered = Render.get_hovered_tile();
    if (hovered === null) return;

    const start = { x: selected.location_x, y: selected.location_y };
    Render.RENDER_CONFIG.path_preview = { start, points: Backend.query_path(start, hovered), valid: true };
}
