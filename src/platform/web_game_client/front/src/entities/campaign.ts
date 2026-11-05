// Rendering of ONE campaign: a banner on a pole in its owner's color, and a circle showing its join radius (the campaign GDO's "join_radius", tiles).
// Same split as the other entity modules: render.ts orchestrates, this draws one entity from an env.

import * as Backend from "../backend.js"
import { query_entity } from "../gdo.js"
import type { RGB } from "../render.js"
import type { EntityRenderEnv } from "./entity_render_env.js"

// Defaults of the campaign fields of RENDER_CONFIG (render.ts spreads this object into it).
export const CAMPAIGN_RENDER_CONFIG =
{
    show_campaign_join_radius: true,
    campaign_join_radius_color: 'rgba(255, 220, 120, 0.6)',
    campaign_join_radius_line_width: 1.5,
    campaign_join_radius_dash: [3, 3] as number[],

    // Banner size: at least this many tiles, growing with the troops, and never below this many canvas px so it stays visible fully zoomed out.
    campaign_min_tiles: 4,
    campaign_growth_tiles_per_doubling: 0.75, // Per power of 2 of significance (the campaign's troops / 10).
    campaign_min_px: 28,
};

export type CampaignRenderConfig = typeof CAMPAIGN_RENDER_CONFIG;

function campaign_viewport(entity: Backend.RenderEntity)
{
    return { x: entity.viewport_x, y: entity.viewport_y };
}

// Canvas pixel size of the banner (also its click area): large for its significance, campaigns are a big deal.
export function campaign_screen_size(config: CampaignRenderConfig, pixels_per_tile: number, entity: Backend.RenderEntity): number
{
    const tiles = config.campaign_min_tiles + Math.log2(1 + entity.significance) * config.campaign_growth_tiles_per_doubling;
    return Math.max(config.campaign_min_px, tiles * pixels_per_tile);
}

// Background layer: the join radius circle, before all icons.
export function draw_campaign_background(env: EntityRenderEnv, config: CampaignRenderConfig, entity: Backend.RenderEntity)
{
    const data = query_entity(entity.guid);
    const radius = data === null ? 0 : data.get_number("join_radius");
    if (radius <= 0) return;

    const position = campaign_viewport(entity);
    const center = env.canvas_center(position.x, position.y);
    const ctx = env.ctx;
    ctx.save();
    ctx.strokeStyle = config.campaign_join_radius_color;
    ctx.lineWidth = config.campaign_join_radius_line_width;
    ctx.setLineDash(config.campaign_join_radius_dash);
    ctx.beginPath();
    ctx.arc(center.x, center.y, radius * env.pixels_per_tile, 0, Math.PI * 2);
    ctx.stroke();
    ctx.restore();
}

// Icon layer: a banner on a pole, standing on the campaign's location, tinted by its owner. 'size' is its canvas height.
export function draw_campaign_banner(env: EntityRenderEnv, entity: Backend.RenderEntity, size: number, color: RGB | null, outline_color: string | null, is_selected: boolean)
{
    const position = campaign_viewport(entity);
    const center = env.canvas_center(position.x, position.y);
    const ctx = env.ctx;
    const fill = color === null ? 'rgb(200, 200, 200)' : `rgb(${color[0]}, ${color[1]}, ${color[2]})`;

    // The pole's foot is the campaign's location, the flag sticks out upwards from it.
    const poleX = center.x;
    const top = center.y - size;
    const bottom = center.y;

    ctx.save();
    ctx.lineWidth = 2;
    ctx.strokeStyle = outline_color ?? '#000';
    ctx.fillStyle = fill;

    // Pole.
    ctx.beginPath();
    ctx.moveTo(poleX, bottom);
    ctx.lineTo(poleX, top);
    ctx.stroke();

    // Banner: a swallow-tailed flag.
    const width = size * 0.75;
    const height = size * 0.5;
    ctx.beginPath();
    ctx.moveTo(poleX, top);
    ctx.lineTo(poleX + width, top);
    ctx.lineTo(poleX + width * 0.75, top + height / 2);
    ctx.lineTo(poleX + width, top + height);
    ctx.lineTo(poleX, top + height);
    ctx.closePath();
    ctx.fill();
    ctx.stroke();

    if (is_selected)
    {
        ctx.strokeStyle = '#0f0';
        ctx.lineWidth = 2;
        ctx.strokeRect(poleX - 3, top - 2, width + 6, size + 4);
    }
    ctx.restore();
}
