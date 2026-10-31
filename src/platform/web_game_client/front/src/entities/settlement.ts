// Rendering of ONE settlement: its icon. See entity_render_env.ts for the env.

import * as Backend from "../backend.js"
import type { RGB } from "../render.js"
import { ENTITY_SCALE_UNIT, loaded_image, type EntityRenderEnv } from "./entity_render_env.js"

// Defaults of the settlement fields of RENDER_CONFIG (render.ts spreads this object into it).
export const SETTLEMENT_RENDER_CONFIG =
{
    // Smallest settlement size (px). Must be at least render.ts' square threshold so a settlement is never culled or squared, at any zoom.
    settlement_min_px: 5,
};

export type SettlementRenderConfig = typeof SETTLEMENT_RENDER_CONFIG;

// Canvas pixel size: 1 tile below 128 inhabitants, then + 1 unit per power of 2 ([128, 256) +1, [256, 512) +2...). Never below settlement_min_px.
export function settlement_screen_size(config: SettlementRenderConfig, pixels_per_tile: number, entity: Backend.RenderEntity): number
{
    const units = entity.significance / 500 + Math.log2(entity.significance) * 4 - 6;
    return Math.max(config.settlement_min_px, (1 + units * ENTITY_SCALE_UNIT) * pixels_per_tile);
}

// Draws the settlement icon at the given viewport position.
export function draw_settlement(env: EntityRenderEnv, viewport_x: number, viewport_y: number, size: number, color: RGB | null, outline_color: string | null, is_selected: boolean)
{
    const sprite = loaded_image(env.resources?.ENTITY.SETTLEMENT);
    if (sprite === null) return;
    env.draw_sprite(sprite, viewport_x, viewport_y, size, color, outline_color, is_selected);
}
