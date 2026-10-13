// Browser input events, translated into calls the rest of the client understands.

import * as Core from "./core.js"
import * as Backend from "./backend.js"
import * as Render from "./render.js"
import { FRONTEND_CAMERA } from "./camera.js"
import { get_local_player_id } from "./main.js"
import { is_in_placement, send_pick } from "./placement.js"

// Input sources (updated on events, used on tick)
const KB_HELD_KEYS = new Set<string>();

// Input state (updated on tick and events).
let MOUSE_WORLD_LOC: { x: number, y: number } | null = null;

// GUID of the currently selected entity, if any.
let SELECTED_ENTITY_GUID: number | null = null;

export function get_selected_entity_guid(): number | null
{
    return SELECTED_ENTITY_GUID;
}

export function clear_selected_entity(): void
{
    SELECTED_ENTITY_GUID = null;
}

function update_viewport_move_input()
{
    const moveInput = { x: 0, y: 0 };

    if (KB_HELD_KEYS.has('KeyA') || KB_HELD_KEYS.has('ArrowLeft')) moveInput.x -= 1;
    if (KB_HELD_KEYS.has('KeyD') || KB_HELD_KEYS.has('ArrowRight')) moveInput.x += 1;
    if (KB_HELD_KEYS.has('KeyS') || KB_HELD_KEYS.has('ArrowDown')) moveInput.y -= 1;
    if (KB_HELD_KEYS.has('KeyW') || KB_HELD_KEYS.has('ArrowUp')) moveInput.y += 1;

    // Normalize when non-zero input on both axis.
    if (moveInput.x !== 0 && moveInput.y !== 0)
    {
        const invLength = 1 / Math.sqrt(2);
        moveInput.x *= invLength;
        moveInput.y *= invLength;
    }

    return moveInput;
}

// World tile under the mouse (not clamped to the world) as of the last mousemove event, or null if the mouse hasn't moved over the canvas yet.
export function get_mouse_world_location(): { x: number, y: number } | null
{
    return MOUSE_WORLD_LOC;
}

// QUERY FUNCTIONS
export function get_selected_entity_view(): Backend.EntityView | null
{
    return SELECTED_ENTITY_GUID != null ? Backend.query_entity(SELECTED_ENTITY_GUID) : null;
}


// INPUT FUNCTIONS

// Sets target location / entity for currently selected entity.
function send_set_target_loc(clientX: number, clientY: number)
{
    if (SELECTED_ENTITY_GUID === null) return;

    // The backend adds the viewport's bottom-left to these offsets and rounds to a tile (clamped in the world), so the rounded offset
    // to the tile lands back on it. Tile i's location is i (entities are drawn at i + 0.5).
    const tile = Render.page_to_tile(clientX, clientY);
    const viewportLocation = Render.world_to_viewport(tile.x, tile.y);

    // Layout of input_event_payload_set_target_loc (game_client_backend.h): ui32 entity guid, i32 x, i32 y (viewport space, whole tiles).
    const view = Backend.ClientInput.begin_input_event();
    view.setUint32(0, SELECTED_ENTITY_GUID, true);
    view.setInt32(4, Math.round(viewportLocation.x), true);
    view.setInt32(8, Math.round(viewportLocation.y), true);
    Backend.ClientInput.commit_input_event(Backend.INPUT_EVENT_TYPE.SET_TARGET_LOC);
}

// Orders the given army to found a settlement at its location.
export function send_found_settlement(army_guid: number): void
{
    // Layout of input_event_payload_found_settlement (game_client_backend.h): ui32 army guid.
    const view = Backend.ClientInput.begin_input_event();
    view.setUint32(0, army_guid, true);
    Backend.ClientInput.commit_input_event(Backend.INPUT_EVENT_TYPE.FOUND_SETTLEMENT);
}

// Orders the given settlement to spawn an army from its population.
export function send_spawn_army(settlement_guid: number): void
{
    // Layout of input_event_payload_spawn_army (game_client_backend.h): ui32 settlement guid.
    const view = Backend.ClientInput.begin_input_event();
    view.setUint32(0, settlement_guid, true);
    Backend.ClientInput.commit_input_event(Backend.INPUT_EVENT_TYPE.SPAWN_ARMY);
}

// Orders the given settlement to build a road to a world tile.
export function send_build_road(settlement_guid: number, tile: { x: number, y: number }): void
{
    // Layout of input_event_payload_build_road (game_client_backend.h): ui32 settlement guid, ui16 x, ui16 y (world tile).
    const view = Backend.ClientInput.begin_input_event();
    view.setUint32(0, settlement_guid, true);
    view.setUint16(4, tile.x, true);
    view.setUint16(6, tile.y, true);
    Backend.ClientInput.commit_input_event(Backend.INPUT_EVENT_TYPE.BUILD_ROAD);
}

// CONSTRUCTION MODE
// Entered from the HUD: the next left click on the map requests a road from the origin settlement to the clicked tile, then the mode ends.
// Right click or Escape cancels it.
let ROAD_CONSTRUCTION_ORIGIN: number | null = null;

export function enter_road_construction(settlement_guid: number): void
{
    ROAD_CONSTRUCTION_ORIGIN = settlement_guid;
}

export function is_in_construction(): boolean
{
    return ROAD_CONSTRUCTION_ORIGIN !== null;
}

// Orders the given army to chase and attack the given entity.
export function send_attack_target(attacker_guid: number, target_guid: number): void
{
    // Layout of input_event_payload_attack_target (game_client_backend.h): ui32 attacker guid, ui32 target guid.
    const view = Backend.ClientInput.begin_input_event();
    view.setUint32(0, attacker_guid, true);
    view.setUint32(4, target_guid, true);
    Backend.ClientInput.commit_input_event(Backend.INPUT_EVENT_TYPE.ATTACK_TARGET);
}

// Right-click order: attack the entity not owned by the local player under the cursor if the selection is a local army
// and there is one, otherwise move.
function send_context_order(clientX: number, clientY: number)
{
    const localPlayerId = get_local_player_id();
    const selected = get_selected_entity_view();
    if (selected !== null && localPlayerId !== null
        && selected.entity_type === Backend.ENTITY_TYPE.ARMY && selected.owner === localPlayerId)
    {
        const targetGuid = Render.pick_entity_at(clientX, clientY, SELECTED_ENTITY_GUID, localPlayerId);
        if (targetGuid !== null)
        {
            send_attack_target(selected.guid, targetGuid);
            return;
        }
    }
    send_set_target_loc(clientX, clientY);
}

// EVENT HANDLERS

// What a mouse button press does.
enum MOUSE_ACTION
{
    SELECT,
    SET_MOVE_TARGET,
}

// Maps a mouse button to what it does. The only place to change when input becomes contextual.
function get_mouse_button_action(button: number): MOUSE_ACTION | null
{
    if (button === 0) return MOUSE_ACTION.SELECT;
    if (button === 2) return MOUSE_ACTION.SET_MOVE_TARGET;
    return null;
}

function do_mouse_action(action: MOUSE_ACTION | null, clientX: number, clientY: number)
{
    if (action == null) return;

    switch (action) {
        case MOUSE_ACTION.SELECT:
            SELECTED_ENTITY_GUID = Render.pick_entity_at(clientX, clientY, SELECTED_ENTITY_GUID); // Empty space clears the selection.
            break;
        case MOUSE_ACTION.SET_MOVE_TARGET:
            send_set_target_loc(clientX, clientY);
            break;
    }
}

function on_pointer_click(clickEvent: MouseEvent)
{
    // Placement mode: a left click anywhere on the map picks the start location, nothing else is available.
    if (is_in_placement())
    {
        if (clickEvent.button !== 0) return;
        const tile = Render.page_to_tile(clickEvent.clientX, clickEvent.clientY);
        if (Render.is_tile_in_world(tile)) send_pick(tile);
        return;
    }

    // Construction mode: a left click on the map requests the road and ends the mode. Other buttons do nothing.
    if (ROAD_CONSTRUCTION_ORIGIN !== null)
    {
        if (clickEvent.button !== 0) return;
        const tile = Render.page_to_tile(clickEvent.clientX, clickEvent.clientY);
        if (!Render.is_tile_in_world(tile)) return; // Stay in the mode, the click missed the map.
        send_build_road(ROAD_CONSTRUCTION_ORIGIN, tile);
        ROAD_CONSTRUCTION_ORIGIN = null;
        return;
    }

    do_mouse_action(get_mouse_button_action(clickEvent.button), clickEvent.clientX, clickEvent.clientY);
}

function on_context_menu(menuEvent: MouseEvent)
{
    menuEvent.preventDefault();
    if (is_in_placement()) return;
    if (ROAD_CONSTRUCTION_ORIGIN !== null)
    {
        ROAD_CONSTRUCTION_ORIGIN = null; // Right click cancels the construction mode.
        return;
    }
    send_context_order(menuEvent.clientX, menuEvent.clientY);
}

function on_mouse_move(moveEvent: MouseEvent)
{
    MOUSE_WORLD_LOC = Render.page_to_tile(moveEvent.clientX, moveEvent.clientY);
    Render.set_hover_page_pos(moveEvent.clientX, moveEvent.clientY);
}

function on_mouse_leave()
{
    Render.set_hover_page_pos(null, null);
}

function on_mouse_wheel(wheelEvent: WheelEvent)
{
    FRONTEND_CAMERA.zoom_at(Render.page_to_viewport_fraction(wheelEvent.clientX, wheelEvent.clientY), wheelEvent.deltaY);
    wheelEvent.preventDefault();
}

// Main lifecycle functions
export function init_input(canvas_element: HTMLCanvasElement)
{
    // Register front-end canvas input events

    // Mouse. Only events targeting the canvas itself count, so HUD widgets overlaid on it don't pan / zoom / select / order.
    const canvas_only = <E extends Event>(handler: (event: E) => void) =>
        (event: E) => { if (event.target === canvas_element) handler(event); };

    canvas_element.addEventListener('click', canvas_only(on_pointer_click));
    canvas_element.addEventListener('contextmenu', canvas_only(on_context_menu));
    canvas_element.addEventListener('mousemove', canvas_only(on_mouse_move));
    canvas_element.addEventListener('mouseleave', on_mouse_leave);
    canvas_element.addEventListener('wheel', canvas_only(on_mouse_wheel), { passive: false });

    // Keyboard
    const movementKeyCodes = new Set(['KeyA', 'KeyD', 'KeyS', 'KeyW', 'ArrowLeft', 'ArrowRight', 'ArrowDown', 'ArrowUp', 'NumpadAdd', 'NumpadSubtract']);
    window.addEventListener('keydown', (keyEvent) => {
        if (movementKeyCodes.has(keyEvent.code)) {
            KB_HELD_KEYS.add(keyEvent.code);
            keyEvent.preventDefault();
        }
    });
    window.addEventListener('keydown', (keyEvent) => {
        if (keyEvent.code === 'Escape') ROAD_CONSTRUCTION_ORIGIN = null;
    });
    window.addEventListener('keyup',(keyEvent) => KB_HELD_KEYS.delete(keyEvent.code));
    window.addEventListener('blur', () => KB_HELD_KEYS.clear());

    // Register tick function on frame callback.
    Core.register_frame_callback(tick_input);
}

function tick_input(delta_time: number)
{
    FRONTEND_CAMERA.pan(update_viewport_move_input(), delta_time);
    
    // Handle held zoom keys (numpad + and -)
    if (KB_HELD_KEYS.has('NumpadAdd')) {
        FRONTEND_CAMERA.zoom_at({ x: 0.5, y: 0.5 }, 250 * delta_time); // Zoom in at a constant rate
    } else if (KB_HELD_KEYS.has('NumpadSubtract')) {
        FRONTEND_CAMERA.zoom_at({ x: 0.5, y: 0.5 }, -250 * delta_time); // Zoom out at a constant rate
    }
    
    FRONTEND_CAMERA.update(delta_time);

    // Layout of input_event_payload_viewport_control (game_client_backend.h):
    // f32 view_rect_min_x, view_rect_min_y, view_rect_max_x, view_rect_max_y.
    const viewRect = FRONTEND_CAMERA.get_rect();
    const input_event_buff = Backend.ClientInput.begin_input_event();
    input_event_buff.setFloat32(0, viewRect.min.x, true);
    input_event_buff.setFloat32(4, viewRect.min.y, true);
    input_event_buff.setFloat32(8, viewRect.max.x, true);
    input_event_buff.setFloat32(12, viewRect.max.y, true);

    Backend.ClientInput.commit_input_event(Backend.INPUT_EVENT_TYPE.VIEWPORT_CONTROL);
}

