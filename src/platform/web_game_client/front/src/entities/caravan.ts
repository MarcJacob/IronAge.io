// Rendering of ONE caravan (a ship sprite when it has the ship flag). See entity_render_env.ts for the env.
// No config: nothing tunable yet.

import * as Backend from "../backend.js"
import type { RGB } from "../render.js"
import { ENTITY_SCALE_UNIT, loaded_image, type EntityRenderEnv } from "./entity_render_env.js"

function is_ship(entity: Backend.RenderEntity): boolean
{
    return (entity.flags & Backend.CARAVAN_FLAG_IS_SHIP) !== 0;
}

// Canvas pixel size: max(1, floor(log2(carried value))) units, no pixel floor.
function caravan_size(pixels_per_tile: number, entity: Backend.RenderEntity): number
{
    return Math.max(1, Math.floor(Math.log2(Math.max(1, entity.significance)))) * ENTITY_SCALE_UNIT * pixels_per_tile;
}

// Ships: same as caravans for now (kept apart to change independently).
function ship_size(pixels_per_tile: number, entity: Backend.RenderEntity): number
{
    return Math.max(1, Math.floor(Math.log2(Math.max(1, entity.significance)))) * ENTITY_SCALE_UNIT * pixels_per_tile;
}

export function caravan_screen_size(pixels_per_tile: number, entity: Backend.RenderEntity): number
{
    return is_ship(entity) ? ship_size(pixels_per_tile, entity) : caravan_size(pixels_per_tile, entity);
}

// Draws the caravan (or ship) icon at the given viewport position.
export function draw_caravan(env: EntityRenderEnv, entity: Backend.RenderEntity, viewport_x: number, viewport_y: number, size: number, color: RGB | null, outline_color: string | null, is_selected: boolean)
{
    const sprites = env.resources?.ENTITY;
    const sprite = loaded_image(is_ship(entity) ? sprites?.SHIP : sprites?.CARAVAN);
    if (sprite === null) return;
    env.draw_sprite(sprite, viewport_x, viewport_y, size, color, outline_color, is_selected);
}
