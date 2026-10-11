// Rendering of the local match onto the canvas. Works from plain state objects, never touches wasm memory.
// Entity positions arrive already relative to the viewport (see backend.read_render_state()); this file only
// scales them to canvas pixels, using the viewport's current size, not the world's.

import * as Core from "./core.js"
import * as Backend from "./backend.js"
import { FRONTEND_CAMERA } from "./camera.js"
import { get_selected_entity_guid } from "./input.js"

let canvas: HTMLCanvasElement;

let canvas_offscreen_tiles_terrain: OffscreenCanvas;
let canvas_offscreen_tiles_influence: OffscreenCanvas;

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
let BACKBUFFER_TILES_INFLUENCE: ImageData | null = null; // Off-screen buffer of pixels, one pixel per tile to represent influence & contestation.

// Fill styles according to diplomatic status with local player.
let DIPLOMATIC_TINTS =
{
    OWN: 'rgba(0, 250, 0, 0.45)',
    FRIENDLY: 'rgba(40, 40, 200, 0.45)',
    NEUTRAL: 'rgba(200, 40, 40, 0.45)',
    ENEMY: 'rgba(200, 40, 40, 0.45)',
}

// Minimum view size (in canvas pixels) of entities before scale multiplier is taken into account.
let ENTITY_MIN_VIEW_SIZE = 10;

// Base scale value of entities according to their type.
let ENTITY_BASE_SCALE_MULTIPLIERS =
{
    [Backend.ENTITY_TYPE.SETTLEMENT]: 5,
    [Backend.ENTITY_TYPE.CARAVAN]: 2,
    [Backend.ENTITY_TYPE.ARMY]: 2,
}

// RGBA color per terrain type, indexed by TERRAIN_TILE_TYPE value (0..4). MUST mirror TERRAIN_TILE_TYPE in include/game_common/match/world.h.
let TERRAIN_COLORS: Array<[number, number, number, number]> =
[
    [80, 120, 80, 255],     // LAND_PLAINS
    [140, 130, 80, 255],  // LAND_HILLS
    [170, 170, 170, 255], // LAND_MOUNTAINS
    [70, 170, 210, 255], // WATER_COAST
    [30, 80, 190, 255],   // WATER_SEA
];
const TERRAIN_COLOR_UNKNOWN: [number, number, number, number] = [255, 0, 255, 255];

// Fill style of the world tile under the mouse.
const HOVERED_TILE_COLOR = 'rgba(255, 255, 0, 0.35)';

// Page position of the mouse over the canvas, or null if it isn't over it. Resolved to a tile every draw so the highlight follows camera moves too.
let HOVER_PAGE_POS: { x: number, y: number } | null = null;

export function set_hover_page_pos(clientX: number | null, clientY: number | null)
{
    HOVER_PAGE_POS = clientX === null || clientY === null ? null : { x: clientX, y: clientY };
}

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

// COORDINATE CONVERSIONS. The only place that does page / canvas / world math (and the only Y flip between canvas and world).
// Spaces: page = mouse event client px; canvas = canvas px, origin top-left, Y down; world = tile units, origin bottom-left, Y up.
// Tile (i, j) covers world [i, i+1) x [j, j+1). All use the viewport of the last draw() call.

// Page (client px) -> canvas px.
function page_to_canvas(clientX: number, clientY: number)
{
    const rect = canvas.getBoundingClientRect();
    return {
        x: (clientX - rect.left) * (canvas.width / rect.width),
        y: (clientY - rect.top) * (canvas.height / rect.height),
    };
}

// Canvas px -> world (fractional).
function canvas_to_world(canvasX: number, canvasY: number)
{
    const s = RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;
    return {
        x: RENDER_PARAMS.viewportBottomLeftX + canvasX / s,
        y: RENDER_PARAMS.viewportBottomLeftY + (canvas.height - canvasY) / s,
    };
}

// World -> canvas px.
function world_to_canvas(worldX: number, worldY: number)
{
    const s = RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;
    return {
        x: (worldX - RENDER_PARAMS.viewportBottomLeftX) * s,
        y: canvas.height - (worldY - RENDER_PARAMS.viewportBottomLeftY) * s,
    };
}

// Canvas rect (top-left corner and square size, px) of a world tile.
function tile_to_canvas_rect(tileX: number, tileY: number)
{
    const topLeft = world_to_canvas(tileX, tileY + 1);
    return { x: topLeft.x, y: topLeft.y, size: RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE };
}

// Canvas px of the center of the tile an entity is in, from its render state viewport position (world location - viewport bottom-left).
function entity_canvas_center(viewport_x: number, viewport_y: number)
{
    return world_to_canvas(viewport_x + RENDER_PARAMS.viewportBottomLeftX + 0.5, viewport_y + RENDER_PARAMS.viewportBottomLeftY + 0.5);
}

// Page -> world tile (integer) under it. Not clamped: may be outside the world, see is_tile_in_world().
export function page_to_tile(clientX: number, clientY: number)
{
    const canvasPos = page_to_canvas(clientX, clientY);
    const world = canvas_to_world(canvasPos.x, canvasPos.y);
    return { x: Math.floor(world.x), y: Math.floor(world.y) };
}

export function is_tile_in_world(tile: { x: number, y: number }): boolean
{
    return tile.x >= 0 && tile.y >= 0 && tile.x < RENDER_PARAMS.WORLD_SIZE.width && tile.y < RENDER_PARAMS.WORLD_SIZE.height;
}

// World position -> offset from the viewport's bottom-left corner, as the game client backend expects for move targets.
export function world_to_viewport(worldX: number, worldY: number)
{
    return { x: worldX - RENDER_PARAMS.viewportBottomLeftX, y: worldY - RENDER_PARAMS.viewportBottomLeftY };
}

// Returns an entity's canvas square size from current render parameters and provided type.
function compute_entity_canvas_size(entity_type: Backend.ENTITY_TYPE)
{
    if (entity_type == Backend.ENTITY_TYPE.INVALID) return 0;

    const scale_mult = ENTITY_BASE_SCALE_MULTIPLIERS[entity_type];
    return Math.max(scale_mult * ENTITY_MIN_VIEW_SIZE, scale_mult * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE);
}

function draw_entity(entity_img: HTMLImageElement, viewport_x: number, viewport_y: number, size:number, owned_by_local_player: boolean = false, is_selected: boolean = false)
{
    const center = entity_canvas_center(viewport_x, viewport_y);
    const drawX = center.x - size / 2;
    const drawY = center.y - size / 2;

    ctx.drawImage(entity_img, drawX, drawY, size, size);

    if (tintCanvas === null || tintCtx === null) return;

    const tintWidth = Math.max(1, Math.ceil(size));
    const tintHeight = Math.max(1, Math.ceil(size));
    tintCanvas.width = tintWidth;
    tintCanvas.height = tintHeight;

    tintCtx.clearRect(0, 0, tintWidth, tintHeight);
    tintCtx.globalCompositeOperation = 'source-over';
    tintCtx.drawImage(entity_img, 0, 0, tintWidth, tintHeight);
    tintCtx.globalCompositeOperation = 'source-in';
    tintCtx.fillStyle = owned_by_local_player ? DIPLOMATIC_TINTS.OWN : DIPLOMATIC_TINTS.ENEMY;
    tintCtx.fillRect(0, 0, tintWidth, tintHeight);
    tintCtx.globalCompositeOperation = 'source-over';

    ctx.drawImage(tintCanvas, drawX, drawY, size, size);

    // Selection indicator: green outline around the image, constant width in screen pixels.
    if (is_selected)
    {
        const padding = 4;
        ctx.strokeStyle = '#0c0';
        ctx.lineWidth = 2;
        ctx.strokeRect(drawX - padding, drawY - padding, size + 2 * padding, size + 2 * padding);
    }
}

// Draws a settlement entity icon at the given viewport location, sized 'scale' world tiles across.
function draw_entity_settlement(viewport_x: number, viewport_y: number, owned_by_local_player: boolean = false, is_selected: boolean = false)
{
    if (RESOURCES == null) return;
    if (!RESOURCES.ENTITY.SETTLEMENT.loaded) return;

    draw_entity(RESOURCES.ENTITY.SETTLEMENT.img, viewport_x, viewport_y, compute_entity_canvas_size(Backend.ENTITY_TYPE.SETTLEMENT), owned_by_local_player, is_selected);
}

// Draws a caravan entity icon at the given viewport location, sized 'scale' world tiles across.
function draw_entity_caravan(viewport_x: number, viewport_y: number, owned_by_local_player: boolean = false, is_selected: boolean = false)
{
    if (RESOURCES == null) return;
    if (!RESOURCES.ENTITY.CARAVAN.loaded) return;

    draw_entity(RESOURCES.ENTITY.CARAVAN.img, viewport_x, viewport_y, compute_entity_canvas_size(Backend.ENTITY_TYPE.CARAVAN), owned_by_local_player, is_selected);
}

// Draws an army entity icon at the given viewport location, sized 'scale' world tiles across.
function draw_entity_army(viewport_x: number, viewport_y: number, owned_by_local_player: boolean = false, is_selected: boolean = false)
{
    if (RESOURCES == null) return;
    if (!RESOURCES.ENTITY.ARMY.loaded) return;

    draw_entity(RESOURCES.ENTITY.ARMY.img, viewport_x, viewport_y, compute_entity_canvas_size(Backend.ENTITY_TYPE.ARMY), owned_by_local_player, is_selected);
}

// Draws the visible part of a world-sized tile bitmap (1 pixel per tile, row 0 = world y 0 = bottom), snapped to whole tiles.
function draw_tile_layer(layer: OffscreenCanvas)
{
    const s = RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;
    const viewMin = canvas_to_world(0, canvas.height);
    const viewMax = canvas_to_world(canvas.width, 0);

    let x0 = Math.max(0, viewMin.x);
    let x1 = Math.min(RENDER_PARAMS.WORLD_SIZE.width, viewMax.x);
    let y0 = Math.max(0, viewMin.y);
    let y1 = Math.min(RENDER_PARAMS.WORLD_SIZE.height, viewMax.y);
    if (x1 <= x0 || y1 <= y0) return;

    x0 = Math.floor(x0);
    x1 = Math.ceil(x1);
    y0 = Math.floor(y0);
    y1 = Math.ceil(y1);

    ctx.imageSmoothingEnabled = false;

    // Origin at the canvas position of the block's bottom-left corner, flipped vertically so bitmap row 0 ends up at the bottom.
    const origin = world_to_canvas(x0, y0);
    ctx.save();
    ctx.translate(origin.x, origin.y);
    ctx.scale(1, -1);
    ctx.drawImage(layer, x0, y0, x1 - x0, y1 - y0, 0, 0, (x1 - x0) * s, (y1 - y0) * s);
    ctx.restore();
}

function draw_terrain()
{
    if (canvas_offscreen_tiles_terrain === undefined) return;
    draw_tile_layer(canvas_offscreen_tiles_terrain);
}

// @TODO(Marc): Consider moving the logic to WASM and / or to GPU accelerated objects.
function draw_influence(render_state: Backend.RenderState)
{
    const tileCount = render_state.world_size.width * render_state.world_size.height;
    const influenceBytes = Backend.get_influence_tiles_view();
    const contestationBytes = Backend.get_contestation_tiles_view();

    if (influenceBytes === null || contestationBytes === null || canvas_offscreen_tiles_influence === undefined) return;

    // Build explicit RGBA bytes. Influence is green, contestation is red,
    // and the combined strength controls opacity so zero-valued tiles remain
    // transparent over the terrain.
    const drawBytes = new Uint8ClampedArray(tileCount * 4);
    for (let i = 0; i < tileCount; i++)
    {
        const influenceVal = influenceBytes[i] - contestationBytes[i];
        const contestationVal = contestationBytes[i];
        const pixelOffset = i * 4;

        drawBytes[pixelOffset] = contestationVal;
        drawBytes[pixelOffset + 1] = influenceVal;
        drawBytes[pixelOffset + 2] = 0;
        drawBytes[pixelOffset + 3] = Math.max(influenceVal, contestationVal) / 2;
    }

    const influenceImg = new ImageData(drawBytes, render_state.world_size.width, render_state.world_size.height);
    const offscreenCtx = canvas_offscreen_tiles_influence.getContext('2d') as OffscreenCanvasRenderingContext2D;
    offscreenCtx.putImageData(influenceImg, 0, 0);

    draw_tile_layer(canvas_offscreen_tiles_influence);
}

// Highlights the world tile under the mouse, if any.
function draw_hovered_tile()
{
    if (HOVER_PAGE_POS === null) return;

    const tile = page_to_tile(HOVER_PAGE_POS.x, HOVER_PAGE_POS.y);
    if (!is_tile_in_world(tile)) return;

    const rect = tile_to_canvas_rect(tile.x, tile.y);
    ctx.fillStyle = HOVERED_TILE_COLOR;
    ctx.fillRect(rect.x, rect.y, rect.size, rect.size);
}

// Draws the world's edges, wherever they currently fall relative to the viewport (may be partly or fully off-canvas).
function draw_world_border()
{
    const bottomLeft = world_to_canvas(0, 0);
    const topRight = world_to_canvas(RENDER_PARAMS.WORLD_SIZE.width, RENDER_PARAMS.WORLD_SIZE.height);

    ctx.strokeStyle = '#000';
    ctx.lineWidth = 3;
    ctx.strokeRect(bottomLeft.x, topRight.y, topRight.x - bottomLeft.x, bottomLeft.y - topRight.y);
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

    draw_terrain();
    draw_influence(render_state);
    draw_hovered_tile();
    draw_world_border();

    const selectedGuid = get_selected_entity_guid();

    for (let i = 0; i < render_state.entity_count; i++)
    {
        const entity = render_state.entity_states[i];
        const isSelected = selectedGuid !== null && entity.guid === selectedGuid;

        if (entity.entity_type === Backend.ENTITY_TYPE.SETTLEMENT) {
            draw_entity_settlement(entity.viewport_x, entity.viewport_y,
                entity.owner === render_state.controlled_player_id, isSelected);
        }
        else if (entity.entity_type == Backend.ENTITY_TYPE.CARAVAN) {
            draw_entity_caravan(entity.viewport_x, entity.viewport_y,
                entity.owner == render_state.controlled_player_id, isSelected);
        }
        else if (entity.entity_type === Backend.ENTITY_TYPE.ARMY) {
            draw_entity_army(entity.viewport_x, entity.viewport_y,
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

function init_offscreen_canvases(render_state: Backend.RenderState)
{
    console.log("Initializing offscreen canvases...");

    // TERRAIN
    const tileTypes = Backend.get_terrain_tiles_view();
    if (tileTypes === null) return;

    // Convert terrain type bytes to RGBA pixels, once.
    const pixels = new Uint8ClampedArray(tileTypes.length * 4);
    for (let i = 0; i < tileTypes.length; i++)
    {
        pixels.set(TERRAIN_COLORS[tileTypes[i]] ?? TERRAIN_COLOR_UNKNOWN, i * 4);
    }

    BACKBUFFER_TILES_TERRAIN = new ImageData(pixels, render_state.world_size.width, render_state.world_size.height);

    // Initialize and draw offscreen canvas.
    canvas_offscreen_tiles_terrain = new OffscreenCanvas(render_state.world_size.width, render_state.world_size.height);

    let offscreenCtx = canvas_offscreen_tiles_terrain.getContext('2d') as OffscreenCanvasRenderingContext2D;
    offscreenCtx.putImageData(BACKBUFFER_TILES_TERRAIN, 0, 0);

    // INFLUENCE

    // Initialize offscreen canvas.
    canvas_offscreen_tiles_influence = new OffscreenCanvas(render_state.world_size.width, render_state.world_size.height);
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
        init_offscreen_canvases(Backend.LAST_RENDER_STATE);
   });

    // Register frame event.
    Core.register_frame_callback((dt) => draw(Backend.LAST_RENDER_STATE));
}

// Converts a position on the page (mouse event coordinates) to a normalized [0, 1] fraction across the canvas.
// (0,0) = bottom-left. Camera zoom anchor, not a world conversion.
export function page_to_viewport_fraction(clientX: number, clientY: number)
{
    const canvasPos = page_to_canvas(clientX, clientY);

    return {
        x: Math.min(Math.max(canvasPos.x / canvas.width, 0), 1),
        y: Math.min(Math.max(1 - canvasPos.y / canvas.height, 0), 1),
    };
}

// Returns the GUID of the entity under the given page position, or null if none.
// If previous_pick is given, the previously-picked entity will only be returned if there is nothing else to select.
// If exclude_owner is given, entities owned by that player are ignored.
export function pick_entity_at(clientX: number, clientY: number, previous_pick: number | null, exclude_owner: number | null = null): number | null
{
    if (Backend.LAST_RENDER_STATE === null) return null;

    const clickPos = page_to_canvas(clientX, clientY);

    const hitGuids: number[] = []; // In render-list order.
    let nearestGuid: number | null = null;
    let nearestDistSq = Infinity;
    for (let i = 0; i < Backend.LAST_RENDER_STATE.entity_count; i++)
    {
        const entity = Backend.LAST_RENDER_STATE.entity_states[i];
        if (entity.entity_type == Backend.ENTITY_TYPE.INVALID) continue;
        if (exclude_owner !== null && entity.owner === exclude_owner) continue;

        const center = entity_canvas_center(entity.viewport_x, entity.viewport_y);
        const dx = clickPos.x - center.x;
        const dy = clickPos.y - center.y;
        const halfSize = compute_entity_canvas_size(entity.entity_type) / 2;
        if (Math.abs(dx) > halfSize || Math.abs(dy) > halfSize) continue;

        hitGuids.push(entity.guid);
        if (dx * dx + dy * dy < nearestDistSq) {
            nearestDistSq = dx * dx + dy * dy;
            nearestGuid = entity.guid;
        }
    }

    const selectedHitIndex = previous_pick === null ? -1 : hitGuids.indexOf(previous_pick);
    if (selectedHitIndex === -1) return nearestGuid;
    return hitGuids[(selectedHitIndex + 1) % hitGuids.length];
}


