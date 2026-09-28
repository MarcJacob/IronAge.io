// Rendering of the local match onto the canvas. Works from plain state objects, never touches wasm memory.

// TODO(Marc): Viewpoint / Camera system, so only a part of the world can be viewed & zooming is supported.
// TODO(Marc): Interpolation system between two states rather than only one at a time, at least for some elements.

import * as Core from "./core.js"
import * as Backend from "./backend.js"

let canvas: HTMLCanvasElement;

let ctx: CanvasRenderingContext2D;

let match_info: Core.MatchInfo;

// Pixels per world tile on each axis.
let scaleX : number = 1;
let scaleY : number = 1;

export function init_render(canvas_element : HTMLCanvasElement, info: Core.MatchInfo) {
    canvas = canvas_element;
    ctx = canvas.getContext("2d") as CanvasRenderingContext2D;

    match_info = info;

    scaleX = canvas.width / match_info.world_size.width;
    scaleY = canvas.height / match_info.world_size.height;
}

// Converts a position on the page (mouse event coordinates) to a world tile position, clamped inside the world.
export function page_to_world(clientX: number, clientY: number)
{
    const rect = canvas.getBoundingClientRect();
    const canvasX = (clientX - rect.left) * (canvas.width / rect.width);
    const canvasY = (clientY - rect.top) * (canvas.height / rect.height);

    return {
        x: Math.min(Math.max(Math.round(canvasX / scaleX), 0), match_info.world_size.width - 1),
        y: Math.min(Math.max(Math.round(canvasY / scaleY), 0), match_info.world_size.height - 1),
    };
}

function draw_entity(render_state: Backend.BackendRenderState, entity_index: number) {

    const entity = render_state.entity_states[entity_index];
    const w = 5 * scaleX;
    const h = 5 * scaleY;

    const radius = 5 * scaleX;

    ctx.fillStyle = '#222';

    const entityCanvasLoc = {
        x: entity.location.x * scaleX,
        y: entity.location.y * scaleY,
    };

    ctx.fillRect(entityCanvasLoc.x - w / 2, entityCanvasLoc.y  - h / 2, w, h);

    ctx.strokeStyle = '#888';
    ctx.lineWidth = 2;
    ctx.strokeRect(entityCanvasLoc.x - w / 2, entityCanvasLoc.y - h / 2, w, h);

    const entityCanvasTargetLoc = {
        x: entity.target_location.x * scaleX,
        y: entity.target_location.y * scaleY,
    };

    ctx.fillStyle = 'red';
    ctx.beginPath();
    ctx.arc(entityCanvasTargetLoc.x, entityCanvasTargetLoc.y, radius, 0, Math.PI * 2);
    ctx.fill();

}

// Draws the given render state (see backend.read_render_state()).
export function draw(render_state: Backend.BackendRenderState)
{
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    for (let i = 0; i < render_state.entity_count; i++)
    {
        draw_entity(render_state, i);
    }
}
