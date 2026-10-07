// Rendering of the local match onto the canvas. Works from plain state objects, never touches wasm memory.
// Entity positions arrive already relative to the viewport (see backend.read_render_state()); this file only
// scales them to canvas pixels, using the viewport's current size, not the world's.

import * as Core from "./core.js"
import * as Backend from "./backend.js"
import { FRONTEND_CAMERA } from "./camera.js"
import { get_selected_entity_guid } from "./input.js"

let canvas: HTMLCanvasElement;

let canvas_offscreen_tiles: OffscreenCanvas;

let ctx: CanvasRenderingContext2D;

let tintCanvas: HTMLCanvasElement | null = null;
let tintCtx: CanvasRenderingContext2D | null = null;

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
        CARAVAN: new RESOURCE_SPRITE("art/entity_caravan.svg"),
        ARMY: new RESOURCE_SPRITE("art/entity_army.svg"),
    };

    // ...
}

let RESOURCES: RESOURCES_STORE | null = null; // Run constructor to load resources.

let BACKBUFFER_TILES_TERRAIN: ImageData | null = null; // Off-screen buffer of pixels, one pixel per tile to represent terrain.
let BACKBUFFER_TILES_INFLUENCE: ImageData | null = null; // Off-screen buffer of pixels, one pixel per tile to represent influence.

// Fill styles according to diplomatic status with local player.
let DIPLOMATIC_TINTS =
{
    OWN: 'rgba(0, 250, 0, 0.45)',
    FRIENDLY: 'rgba(40, 40, 200, 0.45)',
    NEUTRAL: 'rgba(200, 40, 40, 0.45)',
    ENEMY: 'rgba(200, 40, 40, 0.45)',
}

// RGBA color per terrain type, indexed by TERRAIN_TILE_TYPE value (0..4). MUST mirror TERRAIN_TILE_TYPE in include/game_common/match/world.h.
let TERRAIN_COLORS: Array<[number, number, number, number]> =
[
    [80, 120, 80, 255],     // LAND_PLAINS
    [75, 100, 90, 255],  // LAND_HILLS
    [170, 170, 170, 255], // LAND_MOUNTAINS
    [230, 210, 140, 255], // WATER_COAST
    [30, 80, 190, 255],   // WATER_SEA
];
const TERRAIN_COLOR_UNKNOWN: [number, number, number, number] = [255, 0, 255, 255];

// Current state / parameteres of the render surface. Updated on every tick.
let RENDER_PARAMS =
{
	WORLD_SIZE : new Core.WorldSize(),

    // Multiplier applied to abstract viewport space to convert to canvas space / pixels.
	VIEWPORT_TO_CANVAS_SCALE: 1,

	// World location of the viewport's bottom-left corner, as of the last draw() call.
	viewportBottomLeftX : 0,
	viewportBottomLeftY : 0,
}

function draw_entity(entity_img: HTMLImageElement, viewport_x: number, viewport_y: number, scale:number, owned_by_local_player: boolean = false, is_selected: boolean = false)
{
    const w = scale * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;
    const h = scale * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;

    // Tile i covers [i, i+1]: center the entity on its tile's center. Viewport space is world space minus a translation, so +0.5 applies as is.
    const drawX = (viewport_x + 0.5) * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE - w / 2;
    const drawY = canvas.height - (viewport_y + 0.5) * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE - h / 2;

    ctx.drawImage(entity_img, drawX, drawY, w, h);

    // Tint on an isolated transparent surface so compositing cannot clear the world or other entities.
    if (tintCanvas === null || tintCtx === null) return;

    const tintWidth = Math.max(1, Math.ceil(w));
    const tintHeight = Math.max(1, Math.ceil(h));
    tintCanvas.width = tintWidth;
    tintCanvas.height = tintHeight;

    tintCtx.clearRect(0, 0, tintWidth, tintHeight);
    tintCtx.globalCompositeOperation = 'source-over';
    tintCtx.drawImage(entity_img, 0, 0, tintWidth, tintHeight);
    tintCtx.globalCompositeOperation = 'source-in';
    tintCtx.fillStyle = owned_by_local_player ? DIPLOMATIC_TINTS.OWN : DIPLOMATIC_TINTS.ENEMY;
    tintCtx.fillRect(0, 0, tintWidth, tintHeight);
    tintCtx.globalCompositeOperation = 'source-over';

    ctx.drawImage(tintCanvas, drawX, drawY, w, h);

    // Selection indicator: green outline around the image, constant width in screen pixels.
    if (is_selected)
    {
        const padding = 4;
        ctx.strokeStyle = '#0c0';
        ctx.lineWidth = 2;
        ctx.strokeRect(drawX - padding, drawY - padding, w + 2 * padding, h + 2 * padding);
    }
}

// Draws a settlement entity icon at the given viewport location, sized 'scale' world tiles across.
function draw_entity_settlement(viewport_x: number, viewport_y: number, scale: number, owned_by_local_player: boolean = false, is_selected: boolean = false)
{
    if (RESOURCES == null) return;
    if (!RESOURCES.ENTITY.SETTLEMENT.loaded) return;

    draw_entity(RESOURCES.ENTITY.SETTLEMENT.img, viewport_x, viewport_y, scale, owned_by_local_player, is_selected);
}

// Draws a caravan entity icon at the given viewport location, sized 'scale' world tiles across.
function draw_entity_caravan(viewport_x: number, viewport_y: number, scale: number, owned_by_local_player: boolean = false, is_selected: boolean = false)
{
    if (RESOURCES == null) return;
    if (!RESOURCES.ENTITY.CARAVAN.loaded) return;

    draw_entity(RESOURCES.ENTITY.CARAVAN.img, viewport_x, viewport_y, scale, owned_by_local_player, is_selected);
}

// Draws an army entity icon at the given viewport location, sized 'scale' world tiles across.
function draw_entity_army(viewport_x: number, viewport_y: number, scale: number, owned_by_local_player: boolean = false, is_selected: boolean = false)
{
    if (RESOURCES == null) return;
    if (!RESOURCES.ENTITY.ARMY.loaded) return;

    draw_entity(RESOURCES.ENTITY.ARMY.img, viewport_x, viewport_y, scale, owned_by_local_player, is_selected);
}

// Fills the world's projected area white, leaving the out-of-world canvas area transparent/dark.
function draw_world_background()
{
    const left = (0 - RENDER_PARAMS.viewportBottomLeftX) * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;
    const right = (RENDER_PARAMS.WORLD_SIZE.width - RENDER_PARAMS.viewportBottomLeftX) * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;
    const top = canvas.height - (RENDER_PARAMS.WORLD_SIZE.height - RENDER_PARAMS.viewportBottomLeftY) * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;
    const bottom = canvas.height - (0 - RENDER_PARAMS.viewportBottomLeftY) * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;

    ctx.fillStyle = '#fff';
    ctx.fillRect(left, top, right - left, bottom - top);
}

// Draws the visible part of the terrain bitmap (1 pixel per tile, row 0 = world y 0 = bottom), snapped to whole tiles.
// Tile i covers [i, i+1] in world space.
function draw_terrain()
{
    if (canvas_offscreen_tiles === undefined) return;

    const s = RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;
    const vx = RENDER_PARAMS.viewportBottomLeftX;
    const vy = RENDER_PARAMS.viewportBottomLeftY;

    let x0 = Math.max(0, vx);
    let x1 = Math.min(RENDER_PARAMS.WORLD_SIZE.width, vx + canvas.width / s);
    let y0 = Math.max(0, vy);
    let y1 = Math.min(RENDER_PARAMS.WORLD_SIZE.height, vy + canvas.height / s);
    if (x1 <= x0 || y1 <= y0) return;

    x0 = Math.floor(x0);
    x1 = Math.ceil(x1);
    y0 = Math.floor(y0);
    y1 = Math.ceil(y1);

    ctx.imageSmoothingEnabled = false;

    // Flip vertically so bitmap row 0 (world bottom) ends up at the bottom of the canvas.
    ctx.save();
    ctx.translate(0, canvas.height);
    ctx.scale(1, -1);
    ctx.drawImage(canvas_offscreen_tiles, x0, y0, x1 - x0, y1 - y0,
        (x0 - vx) * s, (y0 - vy) * s, (x1 - x0) * s, (y1 - y0) * s);
    ctx.restore();
}

// Draws the world's edges, wherever they currently fall relative to the viewport (may be partly or fully off-canvas).
function draw_world_border()
{
    const left = (0 - RENDER_PARAMS.viewportBottomLeftX) * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;
    const right = (RENDER_PARAMS.WORLD_SIZE.width - RENDER_PARAMS.viewportBottomLeftX) * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;
    const top = canvas.height - (RENDER_PARAMS.WORLD_SIZE.height - RENDER_PARAMS.viewportBottomLeftY) * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;
    const bottom = canvas.height - (0 - RENDER_PARAMS.viewportBottomLeftY) * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;

    ctx.strokeStyle = '#000';
    ctx.lineWidth = 3;
    ctx.strokeRect(left, top, right - left, bottom - top);
}

// Draws the given render state (see backend.read_render_state()).
export function draw(render_state: Backend.RenderState | null)
{
    // Clear full canvas to background color.
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    if (render_state == null) return;

    RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE = canvas.width / render_state.viewport_width;
    RENDER_PARAMS.viewportBottomLeftX = render_state.viewport_bottom_left_x;
    RENDER_PARAMS.viewportBottomLeftY = render_state.viewport_bottom_left_y;
    RENDER_PARAMS.WORLD_SIZE = render_state.world_size;

    draw_world_background();
    draw_terrain();
    draw_world_border();

    const selectedGuid = get_selected_entity_guid();

    for (let i = 0; i < render_state.entity_count; i++)
    {
        const entity = render_state.entity_states[i];
        const isSelected = selectedGuid !== null && entity.guid === selectedGuid;

        if (entity.entity_type === Backend.ENTITY_TYPE.SETTLEMENT) {
            draw_entity_settlement(entity.viewport_x, entity.viewport_y, entity.size_viewport,
                entity.owner === render_state.controlled_player_id, isSelected);
        }
        else if (entity.entity_type == Backend.ENTITY_TYPE.CARAVAN) {
            draw_entity_caravan(entity.viewport_x, entity.viewport_y, entity.size_viewport,
                entity.owner == render_state.controlled_player_id, isSelected);
        }
        else if (entity.entity_type === Backend.ENTITY_TYPE.ARMY) {
            draw_entity_army(entity.viewport_x, entity.viewport_y, entity.size_viewport,
                entity.owner == render_state.controlled_player_id, isSelected);
        }
    }
}

// Matches the canvas's pixel size to its container's, and tells the camera. The container (not the canvas) is measured since
// the canvas can be hidden before a match is joined.
function resize_canvas()
{
    const container = canvas.parentElement as HTMLElement;
    const width = Math.max(1, container.clientWidth);
    const height = Math.max(1, container.clientHeight);

    if (canvas.width !== width) canvas.width = width;
    if (canvas.height !== height) canvas.height = height;
    FRONTEND_CAMERA.set_canvas_size(width, height);
}

export function init_render(canvas_element : HTMLCanvasElement) {
    canvas = canvas_element;
    ctx = canvas.getContext("2d") as CanvasRenderingContext2D;
    tintCanvas = document.createElement('canvas');
    tintCtx = tintCanvas.getContext('2d') as CanvasRenderingContext2D;

    // Canvas pixel size follows its container (1 canvas pixel = 1 CSS pixel, devicePixelRatio is ignored).
    resize_canvas();
    window.addEventListener('resize', resize_canvas);

    // LOAD RENDER RESOURCES
    console.log("Loading render resources...");
    RESOURCES = new RESOURCES_STORE();

    Core.register_on_match_joined_callback(() => 
    {
        if (Backend.LAST_RENDER_STATE == null) return;

        const tileTypes = Backend.get_terrain_tiles_view();
        if (tileTypes === null) return;
        console.log("Initializing backbuffer...");

        // Convert terrain type bytes to RGBA pixels, once.
        const pixels = new Uint8ClampedArray(tileTypes.length * 4);
        for (let i = 0; i < tileTypes.length; i++)
        {
            pixels.set(TERRAIN_COLORS[tileTypes[i]] ?? TERRAIN_COLOR_UNKNOWN, i * 4);
        }

        BACKBUFFER_TILES_TERRAIN = new ImageData(pixels, Backend.LAST_RENDER_STATE.world_size.width, Backend.LAST_RENDER_STATE.world_size.height);
        canvas_offscreen_tiles = new OffscreenCanvas(Backend.LAST_RENDER_STATE.world_size.width, Backend.LAST_RENDER_STATE.world_size.height); 

        let offscreenCtx = canvas_offscreen_tiles.getContext('2d') as OffscreenCanvasRenderingContext2D;
        offscreenCtx.putImageData(BACKBUFFER_TILES_TERRAIN, 0, 0);
    });

    // Register frame event.
    Core.register_frame_callback((dt) => draw(Backend.LAST_RENDER_STATE));
}

// Converts a position on the page (mouse event coordinates) to a world tile position, clamped inside the world.
export function page_to_world(clientX: number, clientY: number)
{
    const rect = canvas.getBoundingClientRect();
    const canvasX = (clientX - rect.left) * (canvas.width / rect.width);
    const canvasY = (clientY - rect.top) * (canvas.height / rect.height);

    const viewRect = FRONTEND_CAMERA.get_rect();
    const worldX = viewRect.min.x + canvasX / canvas.width * (viewRect.max.x - viewRect.min.x);
    const worldY = viewRect.min.y + (canvas.height - canvasY) / canvas.height * (viewRect.max.y - viewRect.min.y);

    return {
        x: Math.min(Math.max(Math.round(worldX), 0), RENDER_PARAMS.WORLD_SIZE.width - 1),
        y: Math.min(Math.max(Math.round(worldY), 0), RENDER_PARAMS.WORLD_SIZE.height - 1),
    };
}

// Converts a position on the page (mouse event coordinates) to viewport coordinates, assuming the pointer is inside it.
export function page_to_viewport(clientX: number, clientY: number)
{
    if (Backend.LAST_RENDER_STATE == null) return { x:0, y:0 };

    const rect = canvas.getBoundingClientRect();
    const canvasX = (clientX - rect.left) * (canvas.width / rect.width);
    const canvasY = (clientY - rect.top) * (canvas.height / rect.height);

    return {
        x: canvasX / RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE,
        y: (canvas.height - canvasY) / RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE
    };
}

// Converts a position on the page (mouse event coordinates) to a normalized [0, 1] fraction across the canvas.
// (0,0) = bottom-left.
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

