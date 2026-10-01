// Rendering of the local match onto the canvas. Works from plain state objects, never touches wasm memory.
// Entity positions arrive already relative to the viewport (see backend.read_render_state()); this file only
// scales them to canvas pixels, using the viewport's current size, not the world's.

import * as Core from "./core.js"
import * as Backend from "./backend.js"

let canvas: HTMLCanvasElement;

let ctx: CanvasRenderingContext2D;

class RESOURCE_SPRITE
{
    constructor(img_src: string) {
        this.loaded = false;
        this.img.onload = () => { this.loaded = true; };
        this.img.src = img_src;
    }

    img: HTMLImageElement = new Image();
    loaded: boolean = false
}

class RESOURCES_STORE
{
    // ENTITY SPRITES
    ENTITY = {
        SETTLEMENT: new RESOURCE_SPRITE("art/entity_settlement.svg"),
        ARMY: new RESOURCE_SPRITE("art/entity_army.svg"),
    };

    // ...
}

let RESOURCES: RESOURCES_STORE | null = null; // Run constructor to load resources.

// Fill styles according to diplomatic status with local player.
let DIPLOMATIC_TINTS =
{
    OWN: 'rgba(40, 200, 90, 0.45)',
    FRIENDLY: 'rgba(40, 40, 200, 0.45)',
    NEUTRAL: 'rgba(200, 40, 40, 0.45)',
    ENEMY: 'rgba(200, 40, 40, 0.45)',
}

// Current state / parameteres of the render surface. Updated on every tick.
let RENDER_STATE =
{
	WORLD_SIZE : new Core.WorldSize(),

    // Multiplier applied to abstract viewport space to convert to canvas space / pixels.
	VIEWPORT_TO_CANVAS_SCALE: 1,

	// World location of the viewport's bottom-left corner, as of the last draw() call.
	viewportBottomLeftX : 0,
	viewportBottomLeftY : 0,
}

export function init_render(canvas_element : HTMLCanvasElement) {
    canvas = canvas_element;
    ctx = canvas.getContext("2d") as CanvasRenderingContext2D;

    // LOAD RENDER RESOURCES
    console.log("Loading render resources...");
    RESOURCES = new RESOURCES_STORE();
}

// Converts a position on the page (mouse event coordinates) to a world tile position, clamped inside the world.
export function page_to_world(clientX: number, clientY: number)
{
    const rect = canvas.getBoundingClientRect();
    const canvasX = (clientX - rect.left) * (canvas.width / rect.width);
    const canvasY = (clientY - rect.top) * (canvas.height / rect.height);

    const worldX = RENDER_STATE.viewportBottomLeftX + canvasX / RENDER_STATE.VIEWPORT_TO_CANVAS_SCALE;
    const worldY = RENDER_STATE.viewportBottomLeftY + (canvas.height - canvasY) / RENDER_STATE.VIEWPORT_TO_CANVAS_SCALE;

    return {
        x: Math.min(Math.max(Math.round(worldX), 0), RENDER_STATE.WORLD_SIZE.width - 1),
        y: Math.min(Math.max(Math.round(worldY), 0), RENDER_STATE.WORLD_SIZE.height - 1),
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

function draw_entity(entity_img: HTMLImageElement, viewport_x: number, viewport_y: number, scale:number, owned_by_local_player: boolean = false)
{
    const w = scale * RENDER_STATE.VIEWPORT_TO_CANVAS_SCALE;
    const h = scale * RENDER_STATE.VIEWPORT_TO_CANVAS_SCALE;

    const drawX = viewport_x * RENDER_STATE.VIEWPORT_TO_CANVAS_SCALE - w / 2;
    const drawY = canvas.height - viewport_y * RENDER_STATE.VIEWPORT_TO_CANVAS_SCALE - h / 2;

    ctx.drawImage(entity_img, drawX, drawY, w, h);

    // Tint the icon's own drawn pixels only (source-atop only paints where the image already set alpha) according to diplo status with local player.
    ctx.save();
    ctx.globalCompositeOperation = 'source-atop';
    if (owned_by_local_player)
    {
        ctx.fillStyle = DIPLOMATIC_TINTS.OWN;
    }
    else
    {
        ctx.fillStyle = DIPLOMATIC_TINTS.ENEMY;
    }
    ctx.fillRect(drawX, drawY, w, h);
    ctx.restore();

}

// Draws a settlement entity icon at the given viewport location, sized 'scale' world tiles across.
function draw_entity_settlement(viewport_x: number, viewport_y: number, scale: number, owned_by_local_player: boolean = false)
{
    if (RESOURCES == null) return;
    if (!RESOURCES.ENTITY.SETTLEMENT.loaded) return;

    draw_entity(RESOURCES.ENTITY.SETTLEMENT.img, viewport_x, viewport_y, scale, owned_by_local_player);
}

// Draws an army entity icon at the given viewport location, sized 'scale' world tiles across.
function draw_entity_army(viewport_x: number, viewport_y: number, scale: number, owned_by_local_player: boolean = false)
{
    if (RESOURCES == null) return;
    if (!RESOURCES.ENTITY.SETTLEMENT.loaded) return;

    draw_entity(RESOURCES.ENTITY.ARMY.img, viewport_x, viewport_y, scale, owned_by_local_player);
}

// Draws the world's edges, wherever they currently fall relative to the viewport (may be partly or fully off-canvas).
function draw_world_border()
{
    const left = (0 - RENDER_STATE.viewportBottomLeftX) * RENDER_STATE.VIEWPORT_TO_CANVAS_SCALE;
    const right = (RENDER_STATE.WORLD_SIZE.width - RENDER_STATE.viewportBottomLeftX) * RENDER_STATE.VIEWPORT_TO_CANVAS_SCALE;
    const top = canvas.height - (RENDER_STATE.WORLD_SIZE.height - RENDER_STATE.viewportBottomLeftY) * RENDER_STATE.VIEWPORT_TO_CANVAS_SCALE;
    const bottom = canvas.height - (0 - RENDER_STATE.viewportBottomLeftY) * RENDER_STATE.VIEWPORT_TO_CANVAS_SCALE;

    ctx.strokeStyle = '#000';
    ctx.lineWidth = 3;
    ctx.strokeRect(left, top, right - left, bottom - top);
}

// Draws the given render state (see backend.read_render_state()).
export function draw(render_state: Backend.BackendRenderState)
{
    RENDER_STATE.VIEWPORT_TO_CANVAS_SCALE = Math.min(canvas.height, canvas.width) / render_state.viewport_width;
    RENDER_STATE.viewportBottomLeftX = render_state.viewport_bottom_left_x;
    RENDER_STATE.viewportBottomLeftY = render_state.viewport_bottom_left_y;
    RENDER_STATE.WORLD_SIZE = render_state.world_size;

    ctx.clearRect(0, 0, canvas.width, canvas.height);

    draw_world_border();

    for (let i = 0; i < render_state.entity_count; i++)
    {
        const entity = render_state.entity_states[i];

        if (entity.entity_type === Backend.ENTITY_TYPE.SETTLEMENT) {
            draw_entity_settlement(entity.viewport_x, entity.viewport_y, entity.size_viewport,
                entity.owner === render_state.controlled_player_id);
        }
        else if (entity.entity_type === Backend.ENTITY_TYPE.ARMY) {
            draw_entity_army(entity.viewport_x, entity.viewport_y, entity.size_viewport,
                entity.owner == render_state.controlled_player_id);
        }
    }
}
