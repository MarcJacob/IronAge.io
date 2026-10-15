// Browser input events, forwarded to the current mode (see core.ts), plus a library of input state and input command senders
// (selection, camera input, send_*) the modes build on.

import * as Core from "./core.js"
import * as Backend from "./backend.js"
import * as Render from "./render.js"
import { FRONTEND_CAMERA } from "./camera.js"
import { get_local_player_id } from "./main.js"

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

export function set_selected_entity_guid(guid: number | null): void
{
    SELECTED_ENTITY_GUID = guid;
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

// Orders the given army to chase and attack the given entity.
export function send_attack_target(attacker_guid: number, target_guid: number): void
{
    // Layout of input_event_payload_attack_target (game_client_backend.h): ui32 attacker guid, ui32 target guid.
    const view = Backend.ClientInput.begin_input_event();
    view.setUint32(0, attacker_guid, true);
    view.setUint32(4, target_guid, true);
    Backend.ClientInput.commit_input_event(Backend.INPUT_EVENT_TYPE.ATTACK_TARGET);
}

// Orders the given settlement to build a road from origin to destination (world tiles). The server validates and computes the path.
export function send_build_road(settlement_guid: number, origin: { x: number, y: number }, destination: { x: number, y: number }): void
{
    write_build_road_payload(settlement_guid, origin, destination);
    Backend.ClientInput.commit_input_event(Backend.INPUT_EVENT_TYPE.BUILD_ROAD);
}

// Whether the same build road order would currently be valid (same check as the server's).
export function is_build_road_valid(settlement_guid: number, origin: { x: number, y: number }, destination: { x: number, y: number }): boolean
{
    write_build_road_payload(settlement_guid, origin, destination);
    return Backend.ClientInput.query_command_valid(Backend.MATCH_COMMAND_TYPE.BUILD_ROAD);
}

// Writes the build road payload in the input event buffer. Same layout for the input event and the command it queues.
function write_build_road_payload(settlement_guid: number, origin: { x: number, y: number }, destination: { x: number, y: number }): void
{
    // Layout of input_event_payload_build_road (game_client_backend.h) / command_data_build_road (commands.h):
    // ui32 settlement guid, ui16 origin x, y, ui16 destination x, y.
    const view = Backend.ClientInput.begin_input_event();
    view.setUint32(0, settlement_guid, true);
    view.setUint16(4, origin.x, true);
    view.setUint16(6, origin.y, true);
    view.setUint16(8, destination.x, true);
    view.setUint16(10, destination.y, true);
}

// Right-click order: attack the entity not owned by the local player under the cursor if the selection is a local army
// and there is one, otherwise move.
export function send_context_order(clientX: number, clientY: number)
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

// What mouse clicks and keys mean is up to the current mode: these handlers only forward to it.
function on_pointer_click(clickEvent: MouseEvent)
{
    Core.get_top_mode()?.on_click(clickEvent);
}

function on_context_menu(menuEvent: MouseEvent)
{
    menuEvent.preventDefault();
    Core.get_top_mode()?.on_context_menu(menuEvent);
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

        if (keyEvent.code === 'Escape') Core.get_top_mode()?.on_escape();
        else Core.get_top_mode()?.on_key_down(keyEvent);
    });
    window.addEventListener('keyup', (keyEvent) => KB_HELD_KEYS.delete(keyEvent.code));
    window.addEventListener('blur', () => KB_HELD_KEYS.clear());
}

// Applies held camera keys, updates the camera and sends the resulting viewport to the backend. Call once per frame in modes with a map view.
export function update_camera_input(delta_time: number)
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

