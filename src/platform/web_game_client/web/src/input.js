// Browser input events, translated into calls the rest of the client understands.
import * as Core from "./core.js";
import { ClientInput, INPUT_EVENT_TYPE } from "./backend.js";
import * as Render from "./render.js";
// Input sources (updated on events, used on tick)
const KB_HELD_KEYS = new Set();
// Input state (updated on tick and events).
let MOUSE_WORLD_LOC = null;
let MOUSE_VIEWPORT_LOC = null;
let MOUSE_VIEWPORT_LOC_NORM = null;
let VIEWPORT_TARGET_ZOOM_LEVEL = 0.5;
let VIEWPORT_MOVE_INPUT = {
    x: 0,
    y: 0
};
function update_viewport_move_input() {
    VIEWPORT_MOVE_INPUT.x = 0;
    VIEWPORT_MOVE_INPUT.y = 0;
    if (KB_HELD_KEYS.has('a') || KB_HELD_KEYS.has('ArrowLeft'))
        VIEWPORT_MOVE_INPUT.x -= 1;
    if (KB_HELD_KEYS.has('d') || KB_HELD_KEYS.has('ArrowRight'))
        VIEWPORT_MOVE_INPUT.x += 1;
    if (KB_HELD_KEYS.has('s') || KB_HELD_KEYS.has('ArrowDown'))
        VIEWPORT_MOVE_INPUT.y -= 1;
    if (KB_HELD_KEYS.has('w') || KB_HELD_KEYS.has('ArrowUp'))
        VIEWPORT_MOVE_INPUT.y += 1;
    // Normalize when non-zero input on both axis.
    if (VIEWPORT_MOVE_INPUT.x !== 0 && VIEWPORT_MOVE_INPUT.y !== 0) {
        const invLength = 1 / Math.sqrt(2);
        VIEWPORT_MOVE_INPUT.x *= invLength;
        VIEWPORT_MOVE_INPUT.y *= invLength;
    }
}
// World location under the mouse as of the last mousemove event, or null if the mouse hasn't moved over the canvas yet.
export function get_last_mouse_world_location() {
    return MOUSE_WORLD_LOC;
}
// TICK
function tick_input(delta_time) {
    // Update input state.
    update_viewport_move_input();
    // Build & Commit Viewport Control input.
    // Layout of input_event_payload_viewport_control:
    // f32 pan_x, pan_y, zoom_delta,
    // cursor_viewport_frac_x, cursor_viewport_frac_y.
    const input_event_buff = ClientInput.begin_input_event();
    // Viewport move vector
    input_event_buff.setFloat32(0, VIEWPORT_MOVE_INPUT.x, true);
    input_event_buff.setFloat32(4, VIEWPORT_MOVE_INPUT.y, true);
    // Viewport target zoom level
    input_event_buff.setFloat32(8, VIEWPORT_TARGET_ZOOM_LEVEL, true);
    // Viewport zoom location
    if (MOUSE_VIEWPORT_LOC_NORM != null) {
        input_event_buff.setFloat32(12, MOUSE_VIEWPORT_LOC_NORM.x, true);
        input_event_buff.setFloat32(16, MOUSE_VIEWPORT_LOC_NORM.y, true);
    }
    else {
        input_event_buff.setFloat32(12, 0.5, true);
        input_event_buff.setFloat32(16, 0.5, true);
    }
    // Commit viewport control event. 
    ClientInput.commit_input_event(INPUT_EVENT_TYPE.VIEWPORT_CONTROL);
}
// EVENT HANDLERS
function on_pointer_click(clickEvent) {
    const worldLocation = Render.page_to_world(clickEvent.clientX, clickEvent.clientY);
    // Layout of input_event_payload_set_target_loc (game_client_backend.h): i32 x, i32 y.
    const view = ClientInput.begin_input_event();
    view.setInt32(0, worldLocation.x, true);
    view.setInt32(4, worldLocation.y, true);
    ClientInput.commit_input_event(INPUT_EVENT_TYPE.SET_TARGET_LOC);
}
function on_mouse_move(moveEvent) {
    MOUSE_WORLD_LOC = Render.page_to_world(moveEvent.clientX, moveEvent.clientY);
    MOUSE_VIEWPORT_LOC_NORM = Render.page_to_viewport_fraction(moveEvent.clientX, moveEvent.clientY);
}
function on_mouse_wheel(wheelEvent) {
    VIEWPORT_TARGET_ZOOM_LEVEL = Math.min(1, Math.max(0, VIEWPORT_TARGET_ZOOM_LEVEL + wheelEvent.deltaY * 0.001));
    wheelEvent.preventDefault();
}
export function init_input(canvas_element) {
    // Register front-end canvas input events
    // Mouse
    canvas_element.addEventListener('click', on_pointer_click);
    canvas_element.addEventListener('mousemove', on_mouse_move);
    canvas_element.addEventListener('wheel', on_mouse_wheel, { passive: false });
    // Keyboard
    canvas_element.addEventListener('keydown', (keyEvent) => KB_HELD_KEYS.add(keyEvent.key));
    canvas_element.addEventListener('keyup', (keyEvent) => KB_HELD_KEYS.delete(keyEvent.key));
    // Register tick function on frame callback.
    Core.register_frame_callback(tick_input);
}
