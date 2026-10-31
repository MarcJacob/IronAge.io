// Rendering of ONE army: its icon and troop count. Grouping of nearby armies stays in render.ts, which passes the displayed position.
// See entity_render_env.ts for the env.

import * as Backend from "../backend.js"
import { query_entity } from "../gdo.js"
import type { RGB } from "../render.js"
import { ENTITY_SCALE_UNIT, loaded_image, type EntityRenderEnv } from "./entity_render_env.js"

// Defaults of the army fields of RENDER_CONFIG (render.ts spreads this object into it).
export const ARMY_RENDER_CONFIG =
{
    // Armies closer than this (tiles, transitive) are drawn as a row of separate icons instead of stacked. Matches the minimum engagement join radius.
    army_group_radius: 2,
};

// Canvas pixel size: max(1, floor(log2(troops))) units, no pixel floor.
export function army_screen_size(pixels_per_tile: number, entity: Backend.RenderEntity): number
{
    return Math.max(1, Math.floor(Math.log2(Math.max(1, entity.significance)))) * ENTITY_SCALE_UNIT * pixels_per_tile;
}

// Draws the army icon at the given viewport position, with its troop count right below.
export function draw_army(env: EntityRenderEnv, entity: Backend.RenderEntity, viewport_x: number, viewport_y: number, size: number, color: RGB | null, outline_color: string | null, is_selected: boolean)
{
    const sprite = loaded_image(env.resources?.ENTITY.ARMY);
    if (sprite !== null) env.draw_sprite(sprite, viewport_x, viewport_y, size, color, outline_color, is_selected);
    draw_troop_count(env, viewport_x, viewport_y, size, entity.guid);
}

// Total troops of an army as text right below its marker. Not drawn for an army in a engagement (the engagement shows the totals instead).
function draw_troop_count(env: EntityRenderEnv, viewport_x: number, viewport_y: number, size: number, army_guid: number)
{
    const army = query_entity(army_guid);
    if (army === null || army.get_number("engagement") !== 0) return;

    const total = army.get_number("composition_levies") + army.get_number("composition_men_at_arms")
        + army.get_number("composition_horsemen") + army.get_number("composition_knights");
    const center = env.canvas_center(viewport_x, viewport_y);
    env.draw_label(String(total), center.x, center.y + size / 2 + 2);
}
