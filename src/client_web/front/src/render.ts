// Rendering of the local match onto the canvas. Works from plain state objects, never touches wasm memory.
// Entity positions arrive already relative to the viewport (see backend.read_render_state()); this file only
// scales them to canvas pixels, using the viewport's current size, not the world's.

import * as Core from "./core.js"
import * as Backend from "./backend.js"

let canvas: HTMLCanvasElement;

let ctx: CanvasRenderingContext2D;

let match_info: Core.MatchInfo;

// Pixels per viewport tile on each axis. Recomputed every draw() call, since viewport size changes with zoom.
let scaleX : number = 1;
let scaleY : number = 1;

// World location of the viewport's bottom-left corner, as of the last draw() call.
let viewportBottomLeftX : number = 0;
let viewportBottomLeftY : number = 0;

export function init_render(canvas_element : HTMLCanvasElement, info: Core.MatchInfo) {
    canvas = canvas_element;
    ctx = canvas.getContext("2d") as CanvasRenderingContext2D;

    match_info = info;
}

// Converts a position on the page (mouse event coordinates) to a world tile position, clamped inside the world.
export function page_to_world(clientX: number, clientY: number)
{
    const rect = canvas.getBoundingClientRect();
    const canvasX = (clientX - rect.left) * (canvas.width / rect.width);
    const canvasY = (clientY - rect.top) * (canvas.height / rect.height);

    // Canvas Y grows downward; viewport/world Y grows upward - flip before converting.
    const worldX = viewportBottomLeftX + canvasX / scaleX;
    const worldY = viewportBottomLeftY + (canvas.height - canvasY) / scaleY;

    return {
        x: Math.min(Math.max(Math.round(worldX), 0), match_info.world_size.width - 1),
        y: Math.min(Math.max(Math.round(worldY), 0), match_info.world_size.height - 1),
    };
}

// Converts a position on the page (mouse event coordinates) to a normalized [0, 1] fraction across the canvas,
// (0,0) = bottom-left, for viewport-relative input (e.g. zoom-to-cursor) that doesn't need a world position.
export function page_to_viewport_fraction(clientX: number, clientY: number)
{
    const rect = canvas.getBoundingClientRect();
    const canvasX = (clientX - rect.left) * (canvas.width / rect.width);
    const canvasY = (clientY - rect.top) * (canvas.height / rect.height);

    return {
        x: Math.min(Math.max(canvasX / canvas.width, 0), 1),
        y: Math.min(Math.max(1 - canvasY / canvas.height, 0), 1),
    };
}

function draw_entity(render_state: Backend.BackendRenderState, entity_index: number) {

    const entity = render_state.entity_states[entity_index];
    const w = 5 * scaleX;
    const h = 5 * scaleY;

    const radius = 5 * scaleX;

    ctx.fillStyle = '#222';

    const entityCanvasLoc = {
        x: entity.viewport_x * scaleX,
        y: canvas.height - entity.viewport_y * scaleY,
    };

    ctx.fillRect(entityCanvasLoc.x - w / 2, entityCanvasLoc.y  - h / 2, w, h);

    ctx.strokeStyle = '#888';
    ctx.lineWidth = 2;
    ctx.strokeRect(entityCanvasLoc.x - w / 2, entityCanvasLoc.y - h / 2, w, h);

    const entityCanvasTargetLoc = {
        x: entity.target_viewport_x * scaleX,
        y: canvas.height - entity.target_viewport_y * scaleY,
    };

    ctx.fillStyle = 'red';
    ctx.beginPath();
    ctx.arc(entityCanvasTargetLoc.x, entityCanvasTargetLoc.y, radius, 0, Math.PI * 2);
    ctx.fill();

}

// Draws the world's edges, wherever they currently fall relative to the viewport (may be partly or fully off-canvas).
function draw_world_border()
{
    const left = (0 - viewportBottomLeftX) * scaleX;
    const right = (match_info.world_size.width - viewportBottomLeftX) * scaleX;
    const top = canvas.height - (match_info.world_size.height - viewportBottomLeftY) * scaleY;
    const bottom = canvas.height - (0 - viewportBottomLeftY) * scaleY;

    ctx.strokeStyle = '#000';
    ctx.lineWidth = 3;
    ctx.strokeRect(left, top, right - left, bottom - top);
}

// Draws the given render state (see backend.read_render_state()).
export function draw(render_state: Backend.BackendRenderState)
{
    scaleX = canvas.width / render_state.viewport_width;
    scaleY = canvas.height / render_state.viewport_height;
    viewportBottomLeftX = render_state.viewport_bottom_left_x;
    viewportBottomLeftY = render_state.viewport_bottom_left_y;

    ctx.clearRect(0, 0, canvas.width, canvas.height);

    draw_world_border();

    for (let i = 0; i < render_state.entity_count; i++)
    {
        draw_entity(render_state, i);
    }
}
