// Browser input events, translated into calls the rest of the client understands.

import * as Core from "./core.js"
import * as Backend from "./backend.js"
import * as Render from "./render.js"
import { FRONTEND_CAMERA } from "./camera.js"

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

// World location under the mouse as of the last mousemove event, or null if the mouse hasn't moved over the canvas yet.
export function get_mouse_world_location(): { x: number, y: number } | null
{
    return MOUSE_WORLD_LOC;
}

// QUERY FUNCTIONS

// Returns the GUID of the entity under the given page position, or null if none.
// Works in viewport space (tiles from the viewport's bottom-left), like entity positions: an entity covers a square of
// size_viewport tiles centered on its location, same as draw_entity. If several overlap, the one whose center is nearest wins.
export function pick_entity_at(clientX: number, clientY: number): number | null
{
    if (Backend.LAST_RENDER_STATE === null) return null;

    const viewportCoords = Render.page_to_viewport(clientX, clientY);

    let bestGuid: number | null = null;
    let bestDistSq = Infinity;
    for (let i = 0; i < Backend.LAST_RENDER_STATE.entity_count; i++)
    {
        const entity = Backend.LAST_RENDER_STATE.entity_states[i];
        const dx = viewportCoords.x - entity.viewport_x;
        const dy = viewportCoords.y - entity.viewport_y;
        const halfSize = entity.size_viewport / 2;
        if (Math.abs(dx) > halfSize || Math.abs(dy) > halfSize) continue;

        if (dx * dx + dy * dy < bestDistSq) {
            bestDistSq = dx * dx + dy * dy;
            bestGuid = entity.guid;
        }
    }
    return bestGuid;
}

export function get_selected_entity_view(): Backend.EntityView | null
{
    return SELECTED_ENTITY_GUID != null ? Backend.query_entity(SELECTED_ENTITY_GUID) : null;
}


// INPUT FUNCTIONS

// Sets target location / entity for currently selected entity.
function send_set_target_loc(clientX: number, clientY: number)
{
    const worldLocation = Render.page_to_world(clientX, clientY);

    // Layout of input_event_payload_set_target_loc (game_client_backend.h): i32 x, i32 y.
    const view = Backend.ClientInput.begin_input_event();
    view.setInt32(0, worldLocation.x, true);
    view.setInt32(4, worldLocation.y, true);
    Backend.ClientInput.commit_input_event(Backend.INPUT_EVENT_TYPE.SET_TARGET_LOC);
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
            SELECTED_ENTITY_GUID = pick_entity_at(clientX, clientY); // Empty space clears the selection.
            break;
        case MOUSE_ACTION.SET_MOVE_TARGET:
            send_set_target_loc(clientX, clientY);
            break;
    }
}

function on_pointer_click(clickEvent: MouseEvent)
{
    do_mouse_action(get_mouse_button_action(clickEvent.button), clickEvent.clientX, clickEvent.clientY);
}

function on_context_menu(menuEvent: MouseEvent)
{
    menuEvent.preventDefault();
    do_mouse_action(MOUSE_ACTION.SET_MOVE_TARGET, menuEvent.clientX, menuEvent.clientY);
}

function on_mouse_move(moveEvent: MouseEvent)
{
    MOUSE_WORLD_LOC = Render.page_to_world(moveEvent.clientX, moveEvent.clientY);
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

    // Mouse
    canvas_element.addEventListener('click', on_pointer_click);
    canvas_element.addEventListener('contextmenu', on_context_menu);
    canvas_element.addEventListener('mousemove', on_mouse_move);
    canvas_element.addEventListener('wheel', on_mouse_wheel, { passive: false });

    // Keyboard
    const movementKeyCodes = new Set(['KeyA', 'KeyD', 'KeyS', 'KeyW', 'ArrowLeft', 'ArrowRight', 'ArrowDown', 'ArrowUp']);
    window.addEventListener('keydown', (keyEvent) => {
        if (movementKeyCodes.has(keyEvent.code)) {
            KB_HELD_KEYS.add(keyEvent.code);
            keyEvent.preventDefault();
        }
    });
    window.addEventListener('keyup', (keyEvent) => KB_HELD_KEYS.delete(keyEvent.code));
    window.addEventListener('blur', () => KB_HELD_KEYS.clear());

    // Register tick function on frame callback.
    Core.register_frame_callback(tick_input);
}

function tick_input(delta_time: number)
{
    FRONTEND_CAMERA.pan(update_viewport_move_input(), delta_time);
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

