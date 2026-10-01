// Browser input events, translated into calls the rest of the client understands.

import * as Core from "./core.js"
import { ClientInput, INPUT_EVENT_TYPE } from "./backend.js"
import * as Render from "./render.js"
import { FRONTEND_CAMERA } from "./camera.js"

// Input sources (updated on events, used on tick)
const KB_HELD_KEYS = new Set<string>();

// Input state (updated on tick and events).
let MOUSE_WORLD_LOC: { x: number, y: number } | null = null;

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
export function get_last_mouse_world_location(): { x: number, y: number } | null
{
    return MOUSE_WORLD_LOC;
}

// TICK
function tick_input(delta_time: number)
{
    FRONTEND_CAMERA.pan(update_viewport_move_input(), delta_time);
    FRONTEND_CAMERA.update(delta_time);

    // Layout of input_event_payload_viewport_control (game_client_backend.h):
    // f32 view_rect_min_x, view_rect_min_y, view_rect_max_x, view_rect_max_y.
    const viewRect = FRONTEND_CAMERA.get_rect();
    const input_event_buff = ClientInput.begin_input_event();
    input_event_buff.setFloat32(0, viewRect.min.x, true);
    input_event_buff.setFloat32(4, viewRect.min.y, true);
    input_event_buff.setFloat32(8, viewRect.max.x, true);
    input_event_buff.setFloat32(12, viewRect.max.y, true);

    ClientInput.commit_input_event(INPUT_EVENT_TYPE.VIEWPORT_CONTROL);
}

// EVENT HANDLERS

function on_pointer_click(clickEvent: PointerEvent)
{
    const worldLocation = Render.page_to_world(clickEvent.clientX, clickEvent.clientY);

    // Layout of input_event_payload_set_target_loc (game_client_backend.h): i32 x, i32 y.
    const view = ClientInput.begin_input_event();
    view.setInt32(0, worldLocation.x, true);
    view.setInt32(4, worldLocation.y, true);
    ClientInput.commit_input_event(INPUT_EVENT_TYPE.SET_TARGET_LOC);
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

export function init_input(canvas_element: HTMLCanvasElement)
{
    // Register front-end canvas input events

    // Mouse
    canvas_element.addEventListener('click', on_pointer_click);
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
