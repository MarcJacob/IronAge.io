// Rendering of ONE engagement (field battle or siege). Entity modules in this folder draw a single entity given its render state entity, size and an env;
// render.ts stays the orchestrator (loops, visibility, draw order, hit-testing) and calls them.
// Engagement data comes from the engagement GDO (query_entity): join_radius, and viewer_status (diplomatic status of the engagement for the local player, computed by the sim).

import * as Backend from "../backend.js"
import { query_entity, type EntityData } from "../gdo.js"
import type { RGB } from "../render.js"
import { loaded_image, type EntityRenderEnv } from "./entity_render_env.js"

// Defaults of the engagement fields of RENDER_CONFIG (render.ts spreads this object into it, so modes read and write them there).
export const ENGAGEMENT_RENDER_CONFIG =
{
    // Circle around each engagement showing its join radius (the "join_radius" property, tiles).
    show_engagement_join_radius: true,
    engagement_join_radius_color: 'rgba(255, 255, 255, 0.5)',
    engagement_join_radius_line_width: 1.5,
    engagement_join_radius_dash: [6, 4] as number[],

    // Icon tint by the engagement's status for the local player: hostile = it could join the defenders, friendly = it could join the attackers, neutral = it cannot join.
    engagement_color_hostile: [220, 50, 50] as RGB,
    engagement_color_neutral: [150, 150, 150] as RGB,
    engagement_color_friendly: [60, 120, 230] as RGB,

    // Icon size: this fraction of the join radius circle's diameter, clamped to [min, max] canvas pixels.
    engagement_icon_fraction: 0.4,
    engagement_icon_min_px: 14,
    engagement_icon_max_px: 72,
};

export type EngagementRenderConfig = typeof ENGAGEMENT_RENDER_CONFIG;

export type EngagementRenderEnv = EntityRenderEnv;

// Render state viewport position of an engagement (fractional offset is in thousandths of a tile).
function engagement_viewport(entity: Backend.RenderEntity)
{
    return { x: entity.viewport_x + entity.fractional_x / 1000, y: entity.viewport_y + entity.fractional_y / 1000 };
}

function join_radius_of(data: EntityData | null): number
{
    return data === null ? 0 : data.get_number("join_radius");
}

// Canvas pixel size of an engagement's icon (also its click area): a fraction of its join radius circle's diameter, clamped.
export function engagement_screen_size(config: EngagementRenderConfig, pixels_per_tile: number, entity: Backend.RenderEntity): number
{
    const diameter = 2 * join_radius_of(query_entity(entity.guid)) * pixels_per_tile;
    return Math.min(config.engagement_icon_max_px, Math.max(config.engagement_icon_min_px, diameter * config.engagement_icon_fraction));
}

function status_color(config: EngagementRenderConfig, data: EntityData | null): RGB
{
    switch (data === null ? Backend.DIPLOMATIC_STATUS.NEUTRAL : data.get_number("viewer_status"))
    {
        case Backend.DIPLOMATIC_STATUS.HOSTILE: return config.engagement_color_hostile;
        case Backend.DIPLOMATIC_STATUS.FRIENDLY: return config.engagement_color_friendly;
        default: return config.engagement_color_neutral;
    }
}

// Background layer of one engagement: its join radius circle. Meant to be drawn before all entity icons.
export function draw_engagement_background(env: EngagementRenderEnv, config: EngagementRenderConfig, entity: Backend.RenderEntity)
{
    const radius = join_radius_of(query_entity(entity.guid));
    if (radius <= 0) return;

    const ctx = env.ctx;
    const position = engagement_viewport(entity);
    const center = env.canvas_center(position.x, position.y);
    ctx.save();
    ctx.strokeStyle = config.engagement_join_radius_color;
    ctx.lineWidth = config.engagement_join_radius_line_width;
    ctx.setLineDash(config.engagement_join_radius_dash);
    ctx.beginPath();
    ctx.arc(center.x, center.y, radius * env.pixels_per_tile, 0, Math.PI * 2);
    ctx.stroke();
    ctx.restore();
}

// Icon layer of one engagement: catapult while a siege is idle, crossing swords for any fighting, tinted by its status for the local player, plus its overlay.
// 'size' is the icon's canvas size (engagement_screen_size, at least the selection minimum).
export function draw_engagement_icon(env: EngagementRenderEnv, config: EngagementRenderConfig, entity: Backend.RenderEntity, size: number, is_selected: boolean)
{
    const data = query_entity(entity.guid);
    const color = status_color(config, data);
    const position = engagement_viewport(entity);
    const center = env.canvas_center(position.x, position.y);

    // Catapult while idle, crossing swords otherwise.
    const sprites = env.resources?.ENTITY;
    const sprite = loaded_image((entity.flags & Backend.ENGAGEMENT_FLAG_IDLE) !== 0 ? sprites?.SIEGE : sprites?.ENGAGEMENT);
    if (sprite !== null) env.draw_sprite(sprite, position.x, position.y, size, color, null, is_selected);
    else
    {
        env.ctx.fillStyle = `rgb(${color[0]}, ${color[1]}, ${color[2]})`;
        env.ctx.fillRect(center.x - size / 2, center.y - size / 2, size, size);
    }

    // FUTURE (richer engagement rendering), to add here around the icon:
    // - fighting animation of the inner armies, inside the join radius circle
    // - top strength bar (total strength of the engagement)
    // - side bars of the attackers' and defenders' armies, and a bottom bar of the armies in the area not involved
    draw_engagement_balance(env, center.x, center.y - size, size * 3, data);
}

// Bar above a engagement marker: attackers' share in red on the left, defenders' in blue on the right. Replaceable by an image.
// "N vs N" total men text right above the bar.
function draw_engagement_balance(env: EngagementRenderEnv, x: number, y: number, width: number, engagement: EntityData | null)
{
    if (engagement === null) return;

    // Men of each side: the sum of its <side>_men_<unit type> properties.
    let attackerMen = 0, defenderMen = 0;
    for (const property of engagement.properties)
    {
        if (typeof property.value !== "number") continue;
        if (property.name.startsWith("attacker_men_")) attackerMen += property.value;
        else if (property.name.startsWith("defender_men_")) defenderMen += property.value;
    }

    const total = attackerMen + defenderMen;
    const redWidth = total > 0 ? width * attackerMen / total : width / 2;
    env.ctx.fillStyle = 'red';
    env.ctx.fillRect(x - width / 2, y - 6, redWidth, 4);
    env.ctx.fillStyle = 'blue';
    env.ctx.fillRect(x - width / 2 + redWidth, y - 6, width - redWidth, 4);

    env.draw_label(`${attackerMen} vs ${defenderMen}`, x, y - 22);
}
