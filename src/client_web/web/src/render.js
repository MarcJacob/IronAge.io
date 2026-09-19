// Rendering of the local match onto the canvas. Works from plain state objects, never touches wasm memory.

// TODO(Marc): Viewpoint / Camera system, so only a part of the world can be viewed & zooming is supported.
// TODO(Marc): Interpolation system between two states rather than only one at a time, at least for some elements.

/** @type {HTMLCanvasElement} */
let canvas = null;

/** @type {CanvasRenderingContext2D} */
let ctx = null;

let worldWidth = 0;
let worldHeight = 0;

// Pixels per world tile on each axis.
let scaleX = 1;
let scaleY = 1;

export function init_render(canvas_element, match_info) {
    canvas = canvas_element;
    ctx = canvas.getContext("2d");

    worldWidth = match_info.world_width;
    worldHeight = match_info.world_height;

    scaleX = canvas.width / worldWidth;
    scaleY = canvas.height / worldHeight;
}

// Converts a position on the page (mouse event coordinates) to a world tile position, clamped inside the world.
export function page_to_world(clientX, clientY) {
    const rect = canvas.getBoundingClientRect();
    const canvasX = (clientX - rect.left) * (canvas.width / rect.width);
    const canvasY = (clientY - rect.top) * (canvas.height / rect.height);

    return {
        x: Math.min(Math.max(Math.round(canvasX / scaleX), 0), worldWidth - 1),
        y: Math.min(Math.max(Math.round(canvasY / scaleY), 0), worldHeight - 1),
    };
}

// Draws the given render state (see backend.read_render_state()).
export function draw(render_state) {
    const x = render_state.entity_x * scaleX;
    const y = render_state.entity_y * scaleY;
    const w = 5 * scaleX;
    const h = 5 * scaleY;

    const cx = render_state.target_x * scaleX;
    const cy = render_state.target_y * scaleY;
    const radius = 5 * scaleX;

    ctx.clearRect(0, 0, canvas.width, canvas.height);

    ctx.fillStyle = '#222';
    ctx.fillRect(x - w / 2, y - h / 2, w, h);

    ctx.strokeStyle = '#888';
    ctx.lineWidth = 2;
    ctx.strokeRect(x - w / 2, y - h / 2, w, h);

    ctx.fillStyle = 'red';
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0, Math.PI * 2);
    ctx.fill();
}
