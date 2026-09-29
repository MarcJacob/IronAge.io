// Browser input events, translated into calls the rest of the client understands.
import * as Core from "./core.js";
import * as Backend from "./backend.js";
import * as Render from "./render.js";
const heldKeys = new Set();
let desiredZoomLevel = 0.5;
let lastMouseWorldLoc = null;
let lastMouseViewportFrac = { x: 0.5, y: 0.5 };
function read_pan_vector() {
    let x = 0, y = 0;
    if (heldKeys.has('a') || heldKeys.has('ArrowLeft'))
        x -= 1;
    if (heldKeys.has('d') || heldKeys.has('ArrowRight'))
        x += 1;
    if (heldKeys.has('s') || heldKeys.has('ArrowDown'))
        y -= 1;
    if (heldKeys.has('w') || heldKeys.has('ArrowUp'))
        y += 1;
    if (x !== 0 && y !== 0) {
        const invLength = 1 / Math.sqrt(2);
        x *= invLength;
        y *= invLength;
    }
    return { x, y };
}
// World location under the mouse as of the last mousemove event, or null if the mouse hasn't moved over the canvas yet.
export function get_last_mouse_world_location() {
    return lastMouseWorldLoc;
}
// Right now a click sets the entity's target location. Later the destination of the input will be the network, not the local backend.
export function init_input(canvas_element) {
    canvas_element.addEventListener('click', (clickEvent) => {
        const worldLocation = Render.page_to_world(clickEvent.clientX, clickEvent.clientY);
        Backend.set_target_loc(worldLocation);
    });
    canvas_element.addEventListener('mousemove', (moveEvent) => {
        lastMouseWorldLoc = Render.page_to_world(moveEvent.clientX, moveEvent.clientY);
        lastMouseViewportFrac = Render.page_to_viewport_fraction(moveEvent.clientX, moveEvent.clientY);
    });
    window.addEventListener('keydown', (keyEvent) => heldKeys.add(keyEvent.key));
    window.addEventListener('keyup', (keyEvent) => heldKeys.delete(keyEvent.key));
    canvas_element.addEventListener('wheel', (wheelEvent) => {
        desiredZoomLevel = Math.min(1, Math.max(0, desiredZoomLevel + wheelEvent.deltaY * 0.001));
        wheelEvent.preventDefault();
    }, { passive: false });
    Core.register_frame_callback((delta_time_s) => {
        const pan = read_pan_vector();
        Backend.apply_viewport_input(pan.x, pan.y, desiredZoomLevel, delta_time_s, lastMouseViewportFrac.x, lastMouseViewportFrac.y);
    });
}
