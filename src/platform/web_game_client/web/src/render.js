// Rendering of the local match onto the canvas. Works from plain state objects, never touches wasm memory.
// Entity positions arrive already relative to the viewport (see backend.read_render_state()); this file only
// scales them to canvas pixels, using the viewport's current size, not the world's.
import * as Core from "./core.js";
import * as Backend from "./backend.js";
let canvas;
let ctx;
// Settlement entity icon, preloaded once at init so draw_entity_settlement() never blocks on it.
let settlementImage = new Image();
let settlementImageLoaded = false;
// World size, viewport rect and scale below are all snapshots as of the last draw() call - everything the render
// state can report (including world size / controlled player) is read fresh every call, nothing is cached at init.
let world_size = new Core.WorldSize();
// Pixels per viewport tile on each axis. Recomputed every draw() call, since viewport size changes with zoom.
let scaleX = 1;
let scaleY = 1;
// World location of the viewport's bottom-left corner, as of the last draw() call.
let viewportBottomLeftX = 0;
let viewportBottomLeftY = 0;
export function init_render(canvas_element) {
    canvas = canvas_element;
    ctx = canvas.getContext("2d");
    settlementImage.onload = () => { settlementImageLoaded = true; };
    settlementImage.src = "art/settlement.svg";
}
// Converts a position on the page (mouse event coordinates) to a world tile position, clamped inside the world.
export function page_to_world(clientX, clientY) {
    const rect = canvas.getBoundingClientRect();
    const canvasX = (clientX - rect.left) * (canvas.width / rect.width);
    const canvasY = (clientY - rect.top) * (canvas.height / rect.height);
    // Canvas Y grows downward; viewport/world Y grows upward - flip before converting.
    const worldX = viewportBottomLeftX + canvasX / scaleX;
    const worldY = viewportBottomLeftY + (canvas.height - canvasY) / scaleY;
    return {
        x: Math.min(Math.max(Math.round(worldX), 0), world_size.width - 1),
        y: Math.min(Math.max(Math.round(worldY), 0), world_size.height - 1),
    };
}
// Converts a position on the page (mouse event coordinates) to a normalized [0, 1] fraction across the canvas,
// (0,0) = bottom-left, for viewport-relative input (e.g. zoom-to-cursor) that doesn't need a world position.
export function page_to_viewport_fraction(clientX, clientY) {
    const rect = canvas.getBoundingClientRect();
    const canvasX = (clientX - rect.left) * (canvas.width / rect.width);
    const canvasY = (clientY - rect.top) * (canvas.height / rect.height);
    return {
        x: Math.min(Math.max(canvasX / canvas.width, 0), 1),
        y: Math.min(Math.max(1 - canvasY / canvas.height, 0), 1),
    };
}
// Draws a settlement icon at the given viewport location, sized `scale` world tiles across. No-op until the icon
// has finished loading. owned_by_local_player tints the icon green.
export function draw_entity_settlement(viewport_x, viewport_y, scale, owned_by_local_player = false) {
    if (!settlementImageLoaded)
        return;
    const w = scale * scaleX;
    const h = scale * scaleY;
    const drawX = viewport_x * scaleX - w / 2;
    const drawY = canvas.height - viewport_y * scaleY - h / 2;
    ctx.drawImage(settlementImage, drawX, drawY, w, h);
    if (owned_by_local_player) {
        // Tint the icon's own drawn pixels only (source-atop only paints where the image already set alpha).
        ctx.save();
        ctx.globalCompositeOperation = 'source-atop';
        ctx.fillStyle = 'rgba(40, 200, 90, 0.45)';
        ctx.fillRect(drawX, drawY, w, h);
        ctx.restore();
    }
}
// Draws the world's edges, wherever they currently fall relative to the viewport (may be partly or fully off-canvas).
function draw_world_border() {
    const left = (0 - viewportBottomLeftX) * scaleX;
    const right = (world_size.width - viewportBottomLeftX) * scaleX;
    const top = canvas.height - (world_size.height - viewportBottomLeftY) * scaleY;
    const bottom = canvas.height - (0 - viewportBottomLeftY) * scaleY;
    ctx.strokeStyle = '#000';
    ctx.lineWidth = 3;
    ctx.strokeRect(left, top, right - left, bottom - top);
}
// Draws the given render state (see backend.read_render_state()).
export function draw(render_state) {
    scaleX = canvas.width / render_state.viewport_width;
    scaleY = canvas.height / render_state.viewport_height;
    viewportBottomLeftX = render_state.viewport_bottom_left_x;
    viewportBottomLeftY = render_state.viewport_bottom_left_y;
    world_size = render_state.world_size;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    draw_world_border();
    for (let i = 0; i < render_state.entity_count; i++) {
        const entity = render_state.entity_states[i];
        if (entity.entity_type === Backend.ENTITY_TYPE.SETTLEMENT) {
            draw_entity_settlement(entity.viewport_x, entity.viewport_y, entity.size_viewport, entity.owner === render_state.controlled_player_id);
        }
    }
}
