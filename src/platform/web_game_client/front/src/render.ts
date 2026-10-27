// Rendering of the local match onto the canvas. Works from plain state objects, never touches wasm memory.
// Entity positions arrive already relative to the viewport (see backend.read_render_state()); this file only
// scales them to canvas pixels, using the viewport's current size, not the world's.

import * as Core from "./core.js"
import * as Backend from "./backend.js"
import { query_entity } from "./gdo.js"
import { FRONTEND_CAMERA } from "./camera.js"

// A filled dot drawn over the map, at least a few px wide at any zoom.
export interface OverlayDot
{
    x: number; // Tile.
    y: number;
    color: string; // CSS color, alpha included.
}

// What gets drawn. Set from the outside (modes), read by draw() every frame. Defaults draw the full game view.
export const RENDER_CONFIG =
{
    show_terrain: true,
    show_territory: true,
    show_hover_tile: true,
    show_world_border: true,
    show_infrastructure: true, // Roads.

    // Which entity types are drawn.
    show_entity_type: {
        [Backend.ENTITY_TYPE.SETTLEMENT]: true,
        [Backend.ENTITY_TYPE.ARMY]: true,
        [Backend.ENTITY_TYPE.WORKER]: true,
        [Backend.ENTITY_TYPE.BATTLE]: true,
        [Backend.ENTITY_TYPE.CARAVAN]: true,
    } as Record<number, boolean>,

    tint_entities: true, // Tint entities with the color given by entity_color.
    outline_entities: true, // Outline entities with the color given by entity_outline_color.
    // Color policies: the drawing code only asks these for a color and never decides one itself.
    // A mode may replace them (e.g. to paint all territories gray but one). Defaults: see player_color and default_outline_color.
    entity_color: ((owner: number, local_player: number): RGB => player_color(owner, local_player)),
    territory_color: ((owner: number, local_player: number, settlement_slot: number): RGB => player_color(owner, local_player)),
    // Entity outline color (CSS). Hook for diplomatic status: owner and local_player are given for that purpose.
    entity_outline_color: ((owner: number, local_player: number): string => default_outline_color),
    outlined_entity: null as number | null, // GUID of the entity drawn with a selection outline.
    // If set, draws the path from start through points (waypoints, as returned by Backend.query_path). Drawn red if not valid, or in color if given.
    path_preview: null as { start: { x: number, y: number }, points: { x: number, y: number }[], valid: boolean, color?: string } | null,
    overlay_dots: [] as OverlayDot[],
};

// Opaque RGB color, 0..255 per channel.
export type RGB = [number, number, number];

const OWN_COLOR: RGB = [0, 250, 0];
const UNKNOWN_OWNER_COLOR: RGB = [128, 128, 128];
const default_outline_color = '#000';
// Color per player, indexed by player index modulo table length. No bright green: too close to OWN_COLOR.
const PLAYER_COLORS: RGB[] = [
    [230, 25, 75], [0, 130, 200], [245, 130, 48], [145, 30, 180],
    [240, 230, 40], [70, 240, 240], [240, 50, 230], [170, 110, 40],
    [128, 0, 0], [0, 0, 128], [128, 128, 0], [0, 128, 128],
    [250, 190, 190], [230, 190, 255], [255, 255, 255], [90, 90, 90],
];

// Default color policy: local player's own color, otherwise a color from the table by player index.
function player_color(owner: number, local_player: number): RGB
{
    if (owner < 0) return UNKNOWN_OWNER_COLOR;
    if (owner === local_player) return OWN_COLOR;
    return PLAYER_COLORS[owner % PLAYER_COLORS.length];
}

let canvas: HTMLCanvasElement;

let canvas_offscreen_tiles_terrain: OffscreenCanvas;
let canvas_offscreen_tiles_territory: OffscreenCanvas;

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
        ARMY: new RESOURCE_SPRITE("art/entity_army.svg"),
        CARAVAN: new RESOURCE_SPRITE("art/entity_caravan.svg"),
    };

    // ...
}

let RESOURCES: RESOURCES_STORE | null = null; // Run constructor to load resources.

let BACKBUFFER_TILES_TERRAIN: ImageData | null = null; // Off-screen buffer of pixels, one pixel per tile to represent terrain.
let BACKBUFFER_TILES_TERRITORY: ImageData | null = null; // Off-screen buffer of pixels, one pixel per tile to represent territory ownership.

// Minimum view size (in canvas pixels) of entities before scale multiplier is taken into account.
let ENTITY_MIN_VIEW_SIZE = 10;

// Entity size in tiles, per unit of scale multiplier. Halved to match the doubled tile scale (see MIN_VIEW_HEIGHT in camera.ts).
const ENTITY_TILE_SCALE = 0.5;

// Base scale value of entities according to their type.
let ENTITY_BASE_SCALE_MULTIPLIERS =
{
    [Backend.ENTITY_TYPE.SETTLEMENT]: 5,
    [Backend.ENTITY_TYPE.ARMY]: 2,
    [Backend.ENTITY_TYPE.WORKER]: 1, // Only the workers walking on the map are sent by the backend.
    [Backend.ENTITY_TYPE.BATTLE]: 3,
    [Backend.ENTITY_TYPE.CARAVAN]: 2,
}

// RGBA color per terrain type, indexed by TERRAIN_TILE_TYPE value (0..4). MUST mirror TERRAIN_TILE_TYPE in include/game_common/match/world/world_terrain.h.
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

// Stroke style of the path preview line.
const PATH_PREVIEW_COLOR = 'rgba(255, 255, 255, 0.9)';
const PATH_PREVIEW_COLOR_INVALID = 'rgba(255, 40, 40, 0.9)';

// Mirror RENDER_TILE_FLAGS in include/game_client/game_client_backend.h.
export const TILE_FLAG_BORDER = 1 << 0;
export const TILE_FLAG_ROAD = 1 << 1;
export const TILE_FLAG_OWNED = 1 << 2;

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
    return Math.max(scale_mult * ENTITY_MIN_VIEW_SIZE, scale_mult * ENTITY_TILE_SCALE * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE);
}

// Draws the sprite in 'color' (tint overlay, alpha 0.45) with an optional outline (CSS color, null = none). Any color is accepted.
function draw_entity(entity_img: HTMLImageElement, viewport_x: number, viewport_y: number, size:number, color: RGB | null, outline_color: string | null, is_selected: boolean = false)
{
    const center = entity_canvas_center(viewport_x, viewport_y);
    const drawX = center.x - size / 2;
    const drawY = center.y - size / 2;

    // Fills the sprite's silhouette with a CSS color in tintCanvas.
    const silhouette = (fill: string) =>
    {
        const w = Math.max(1, Math.ceil(size));
        tintCanvas!.width = w;
        tintCanvas!.height = w;
        tintCtx!.drawImage(entity_img, 0, 0, w, w);
        tintCtx!.globalCompositeOperation = 'source-in';
        tintCtx!.fillStyle = fill;
        tintCtx!.fillRect(0, 0, w, w);
        tintCtx!.globalCompositeOperation = 'source-over';
    };

    // Outline: the silhouette drawn around the sprite in 8 directions, constant width in screen pixels.
    if (outline_color !== null && tintCanvas !== null && tintCtx !== null)
    {
        silhouette(outline_color);
        for (let a = 0; a < 8; a++)
        {
            ctx.drawImage(tintCanvas, drawX + Math.cos(a * Math.PI / 4) * 1.5, drawY + Math.sin(a * Math.PI / 4) * 1.5, size, size);
        }
    }

    ctx.drawImage(entity_img, drawX, drawY, size, size);

    if (color !== null && tintCanvas !== null && tintCtx !== null)
    {
        silhouette(`rgba(${color[0]}, ${color[1]}, ${color[2]}, 0.45)`);
        ctx.drawImage(tintCanvas, drawX, drawY, size, size);
    }

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
function draw_entity_settlement(viewport_x: number, viewport_y: number, color: RGB | null, outline_color: string | null, is_selected: boolean = false)
{
    if (RESOURCES == null) return;
    if (!RESOURCES.ENTITY.SETTLEMENT.loaded) return;

    draw_entity(RESOURCES.ENTITY.SETTLEMENT.img, viewport_x, viewport_y, compute_entity_canvas_size(Backend.ENTITY_TYPE.SETTLEMENT), color, outline_color, is_selected);
}

// Draws an army entity icon at the given viewport location, sized 'scale' world tiles across.
function draw_entity_army(viewport_x: number, viewport_y: number, color: RGB | null, outline_color: string | null, is_selected: boolean = false)
{
    if (RESOURCES == null) return;
    if (!RESOURCES.ENTITY.ARMY.loaded) return;

    draw_entity(RESOURCES.ENTITY.ARMY.img, viewport_x, viewport_y, compute_entity_canvas_size(Backend.ENTITY_TYPE.ARMY), color, outline_color, is_selected);
}

// Draws a caravan entity icon at the given viewport location.
function draw_entity_caravan(viewport_x: number, viewport_y: number, color: RGB | null, outline_color: string | null, is_selected: boolean = false)
{
    if (RESOURCES == null) return;
    if (!RESOURCES.ENTITY.CARAVAN.loaded) return;

    draw_entity(RESOURCES.ENTITY.CARAVAN.img, viewport_x, viewport_y, compute_entity_canvas_size(Backend.ENTITY_TYPE.CARAVAN), color, outline_color, is_selected);
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
function draw_territory(render_state: Backend.RenderState)
{
    const tileCount = render_state.world_size.width * render_state.world_size.height;
    const ownerSettlementBytes = Backend.get_owner_settlement_tiles_view();
    const flagBytes = Backend.get_tile_flags_view();

    if (ownerSettlementBytes === null || flagBytes === null
        || canvas_offscreen_tiles_territory === undefined) return;

    // Owner player per settlement slot, from the visible settlement entities (-1 = unknown). The slot is the index
    // part of the settlement's macro entity_guid (bits 8-19). Settlements touching the viewport are always visible.
    const settlementOwners = new Int32Array(0x1000).fill(-1);
    for (const entity of render_state.entity_states)
    {
        if (entity.entity_type === Backend.ENTITY_TYPE.SETTLEMENT) settlementOwners[(entity.guid >>> 8) & 0xFFF] = entity.owner;
    }

    // Color per settlement slot, from the territory color policy.
    const slotColors: RGB[] = [];
    for (let slot = 0; slot < settlementOwners.length; slot++)
    {
        slotColors.push(RENDER_CONFIG.territory_color(settlementOwners[slot], render_state.controlled_player_id, slot));
    }

    // Settlement owning the tile under the mouse, if any. All tiles of that settlement get highlighted.
    let hoveredSettlement = -1;
    if (HOVER_PAGE_POS !== null)
    {
        const hoveredTile = page_to_tile(HOVER_PAGE_POS.x, HOVER_PAGE_POS.y);
        if (is_tile_in_world(hoveredTile))
        {
            const hoveredIndex = hoveredTile.y * render_state.world_size.width + hoveredTile.x;
            if ((flagBytes[hoveredIndex] & TILE_FLAG_OWNED) !== 0) hoveredSettlement = ownerSettlementBytes[hoveredIndex];
        }
    }

    // Build explicit RGBA bytes. Owned tiles use the territory color policy's color for their settlement at OWNED_ALPHA,
    // unowned tiles remain transparent over the terrain. Tiles of the hovered settlement use HOVERED_ALPHA instead.
    const OWNED_ALPHA = 0.15;
    const HOVERED_ALPHA = 0.35;

    const drawBytes = new Uint8ClampedArray(tileCount * 4);
    for (let i = 0; i < tileCount; i++)
    {
        const isOwned = (flagBytes[i] & TILE_FLAG_OWNED) !== 0;
        const ownerColor = isOwned ? slotColors[ownerSettlementBytes[i]] : UNKNOWN_OWNER_COLOR;
        const ownedAlpha = isOwned && ownerSettlementBytes[i] === hoveredSettlement ? HOVERED_ALPHA : OWNED_ALPHA;
        const pixelOffset = i * 4;

        drawBytes[pixelOffset] = ownerColor[0];
        drawBytes[pixelOffset + 1] = ownerColor[1];
        drawBytes[pixelOffset + 2] = ownerColor[2];
        drawBytes[pixelOffset + 3] = isOwned ? 255 * ownedAlpha : 0;

        if ((flagBytes[i] & TILE_FLAG_BORDER) !== 0)
        {
            drawBytes[pixelOffset] = 0;
            drawBytes[pixelOffset + 1] = 0;
            drawBytes[pixelOffset + 2] = 0;
            drawBytes[pixelOffset + 3] = 196;
        }
    }

    const territoryImg = new ImageData(drawBytes, render_state.world_size.width, render_state.world_size.height);
    const offscreenCtx = canvas_offscreen_tiles_territory.getContext('2d') as OffscreenCanvasRenderingContext2D;
    offscreenCtx.putImageData(territoryImg, 0, 0);

    draw_tile_layer(canvas_offscreen_tiles_territory);
}

// Same translucent gray the road tile coloration used.
const ROAD_COLOR = 'rgba(150, 150, 150, 0.67)';

// Neighbour offsets (x, y in world, Y up) of the 8 neighbours. The first 4 are the ones a segment is drawn toward
// from each road tile (E, N, NE, NW): the opposite ones are drawn by the neighbour itself, so each segment is drawn once.
const ROAD_NEIGHBOUR_DX = [1, 0, 1, -1, -1, 0, -1, 1];
const ROAD_NEIGHBOUR_DY = [0, 1, 1, 1, 0, -1, -1, -1];

function is_road_tile(flagBytes: Uint8Array, worldW: number, worldH: number, x: number, y: number): boolean
{
    return x >= 0 && y >= 0 && x < worldW && y < worldH && (flagBytes[y * worldW + x] & TILE_FLAG_ROAD) !== 0;
}

// Draws roads as lines between the centers of adjacent road tiles (8-neighbourhood), isolated road tiles as a dot.
// Visible tiles only (plus a 1 tile margin so segments coming from just off-screen tiles are drawn). The neighbour mask
// is read straight from the tile flags (no per-tile allocation). Everything is one path / one stroke so
// overlapping segments don't stack their translucency.
function draw_roads(render_state: Backend.RenderState)
{
    const flagBytes = Backend.get_tile_flags_view();
    if (flagBytes === null) return;

    const worldW = render_state.world_size.width;
    const worldH = render_state.world_size.height;
    const viewMin = canvas_to_world(0, canvas.height);
    const viewMax = canvas_to_world(canvas.width, 0);
    const x0 = Math.max(0, Math.floor(viewMin.x) - 1);
    const x1 = Math.min(worldW - 1, Math.ceil(viewMax.x) + 1);
    const y0 = Math.max(0, Math.floor(viewMin.y) - 1);
    const y1 = Math.min(worldH - 1, Math.ceil(viewMax.y) + 1);

    const s = RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE;
    ctx.strokeStyle = ROAD_COLOR;
    ctx.lineWidth = Math.max(2, 0.2 * s);
    ctx.lineCap = 'round'; // Joins the segments meeting at a tile center, and makes a zero-length segment a dot.
    ctx.beginPath();
    for (let y = y0; y <= y1; y++)
    {
        for (let x = x0; x <= x1; x++)
        {
            if ((flagBytes[y * worldW + x] & TILE_FLAG_ROAD) === 0) continue;

            const center = world_to_canvas(x + 0.5, y + 0.5);
            let hasNeighbour = false;
            for (let d = 0; d < 8; d++)
            {
                const nx = x + ROAD_NEIGHBOUR_DX[d];
                const ny = y + ROAD_NEIGHBOUR_DY[d];
                if (nx < 0 || ny < 0 || nx >= worldW || ny >= worldH) continue;
                if ((flagBytes[ny * worldW + nx] & TILE_FLAG_ROAD) === 0) continue;

                hasNeighbour = true;
                // A diagonal is skipped if either orthogonal cell it cuts across is a road (avoids triangles at junctions).
                if (d < 4 && (d < 2 || (!is_road_tile(flagBytes, worldW, worldH, x + ROAD_NEIGHBOUR_DX[d], y)
                    && !is_road_tile(flagBytes, worldW, worldH, x, y + ROAD_NEIGHBOUR_DY[d]))))
                {
                    ctx.moveTo(center.x, center.y);
                    ctx.lineTo(center.x + ROAD_NEIGHBOUR_DX[d] * s, center.y - ROAD_NEIGHBOUR_DY[d] * s);
                }
            }
            if (!hasNeighbour)
            {
                ctx.moveTo(center.x, center.y);
                ctx.lineTo(center.x, center.y);
            }
        }
    }
    ctx.stroke();
    ctx.lineCap = 'butt';
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

// World tile under the mouse, or null if the mouse isn't over the canvas or the tile is outside the world.
export function get_hovered_tile(): { x: number, y: number } | null
{
    if (HOVER_PAGE_POS === null) return null;

    const tile = page_to_tile(HOVER_PAGE_POS.x, HOVER_PAGE_POS.y);
    return is_tile_in_world(tile) ? tile : null;
}

// Draws the configured path preview, if any.
function draw_path_preview()
{
    const preview = RENDER_CONFIG.path_preview;
    if (preview === null) return;

    const startCenter = world_to_canvas(preview.start.x + 0.5, preview.start.y + 0.5);

    ctx.strokeStyle = preview.color ?? (preview.valid ? PATH_PREVIEW_COLOR : PATH_PREVIEW_COLOR_INVALID);
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.moveTo(startCenter.x, startCenter.y);
    for (const point of preview.points)
    {
        const center = world_to_canvas(point.x + 0.5, point.y + 0.5);
        ctx.lineTo(center.x, center.y);
    }
    ctx.stroke();
}

// Draws the configured overlay dots.
function draw_overlay_dots()
{
    const radius = Math.max(6, 2 * RENDER_PARAMS.VIEWPORT_TO_CANVAS_SCALE);
    for (const dot of RENDER_CONFIG.overlay_dots)
    {
        const center = world_to_canvas(dot.x + 0.5, dot.y + 0.5);
        ctx.fillStyle = dot.color;
        ctx.beginPath();
        ctx.arc(center.x, center.y, radius, 0, 2 * Math.PI);
        ctx.fill();
    }
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

    if (RENDER_CONFIG.show_terrain) draw_terrain();
    if (RENDER_CONFIG.show_territory) draw_territory(render_state);
    if (RENDER_CONFIG.show_infrastructure) draw_roads(render_state);
    if (RENDER_CONFIG.show_hover_tile) draw_hovered_tile();
    draw_path_preview(); // Only draws with a path_preview_start.
    if (RENDER_CONFIG.show_world_border) draw_world_border();
    draw_overlay_dots();

    const selectedGuid = RENDER_CONFIG.outlined_entity;

    for (let i = 0; i < render_state.entity_count; i++)
    {
        const entity = render_state.entity_states[i];
        if (!RENDER_CONFIG.show_entity_type[entity.entity_type]) continue;
        // Moving entities sit between tile centers: fractional offset is in thousandths of a tile, i.e. world units / 1000 (same axes as the viewport).
        const vx = entity.viewport_x + entity.fractional_x / 1000;
        const vy = entity.viewport_y + entity.fractional_y / 1000;
        const isSelected = selectedGuid !== null && entity.guid === selectedGuid;
        const local = render_state.controlled_player_id;
        const color = RENDER_CONFIG.tint_entities ? RENDER_CONFIG.entity_color(entity.owner, local) : null;
        const outline = RENDER_CONFIG.outline_entities ? RENDER_CONFIG.entity_outline_color(entity.owner, local) : null;

        if (entity.entity_type === Backend.ENTITY_TYPE.SETTLEMENT) {
            draw_entity_settlement(vx, vy, color, outline, isSelected);
        }
        else if (entity.entity_type === Backend.ENTITY_TYPE.ARMY) {
            draw_entity_army(vx, vy, color, outline, isSelected);
            draw_army_troop_count(vx, vy, entity.guid);
        }
        else if (entity.entity_type === Backend.ENTITY_TYPE.CARAVAN) {
            draw_entity_caravan(vx, vy, color, outline, isSelected);
        }
        else if (entity.entity_type === Backend.ENTITY_TYPE.WORKER) {
            // Simple marker, no sprite yet: filled with the entity color (red if tinting is off).
            const center = entity_canvas_center(vx, vy);
            const size = compute_entity_canvas_size(entity.entity_type);
            ctx.fillStyle = color !== null ? `rgb(${color[0]}, ${color[1]}, ${color[2]})` : 'red';
            ctx.fillRect(center.x - size / 2, center.y - size / 2, size, size);
            if (isSelected) {
                ctx.strokeStyle = '#0c0';
                ctx.lineWidth = 2;
                ctx.strokeRect(center.x - size / 2 - 4, center.y - size / 2 - 4, size + 8, size + 8);
            }
        }
        else if (entity.entity_type === Backend.ENTITY_TYPE.BATTLE) {
            // Simple red marker.
            const center = entity_canvas_center(vx, vy);
            const size = compute_entity_canvas_size(entity.entity_type);
            ctx.fillStyle = 'red';
            ctx.fillRect(center.x - size / 2, center.y - size / 2, size, size);
            draw_battle_balance(center.x, center.y - size, size * 3, entity.guid);
        }
    }
}

// Small centered text with a dark outline so it reads on any terrain.
function draw_label(text: string, x: number, y: number)
{
    ctx.font = "12px sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "top";
    ctx.lineWidth = 3;
    ctx.strokeStyle = 'rgba(0, 0, 0, 0.8)';
    ctx.strokeText(text, x, y);
    ctx.fillStyle = '#fff';
    ctx.fillText(text, x, y);
}

// Total troops of an army as text right below its marker. Not drawn for an army in a battle (the battle shows the totals instead).
function draw_army_troop_count(viewport_x: number, viewport_y: number, army_guid: number)
{
    const army = query_entity(army_guid);
    if (army === null || army.get_number("battle") !== 0) return;

    const total = army.get_number("composition_levies") + army.get_number("composition_men_at_arms")
        + army.get_number("composition_horsemen") + army.get_number("composition_knights");
    const center = entity_canvas_center(viewport_x, viewport_y);
    draw_label(String(total), center.x, center.y + compute_entity_canvas_size(Backend.ENTITY_TYPE.ARMY) / 2 + 2);
}

// Bar above a battle marker: attackers' share in red on the left, defenders' in blue on the right. Replaceable by an image.
// "N vs N" total men text right above the bar.
function draw_battle_balance(x: number, y: number, width: number, battle_guid: number)
{
    const battle = query_entity(battle_guid);
    if (battle === null) return;

    // Men of each side: the sum of its men_side<side>_<unit type> properties.
    let attackerMen = 0, defenderMen = 0;
    for (const property of battle.properties)
    {
        if (typeof property.value !== "number") continue;
        if (property.name.startsWith("men_side0_")) attackerMen += property.value;
        else if (property.name.startsWith("men_side1_")) defenderMen += property.value;
    }

    const total = attackerMen + defenderMen;
    const redWidth = total > 0 ? width * attackerMen / total : width / 2;
    ctx.fillStyle = 'red';
    ctx.fillRect(x - width / 2, y - 6, redWidth, 4);
    ctx.fillStyle = 'blue';
    ctx.fillRect(x - width / 2 + redWidth, y - 6, width - redWidth, 4);

    draw_label(`${attackerMen} vs ${defenderMen}`, x, y - 22);
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

    // TERRITORY

    // Initialize offscreen canvas.
    canvas_offscreen_tiles_territory =new OffscreenCanvas(render_state.world_size.width, render_state.world_size.height);
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

        const center = entity_canvas_center(entity.viewport_x + entity.fractional_x / 1000, entity.viewport_y + entity.fractional_y / 1000);
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


