// Rendering of ONE worker (only workers walking on the map are sent by the backend). See entity_render_env.ts for the env.
// No config: nothing tunable yet.

import * as Backend from "../backend.js"
import type { RGB } from "../render.js"
import type { EntityRenderEnv } from "./entity_render_env.js"

// Canvas pixel size: fixed half a tile, no pixel floor.
export function worker_screen_size(pixels_per_tile: number, entity: Backend.RenderEntity): number
{
    return 0.5 * pixels_per_tile;
}

// Simple marker, no sprite yet: filled with the entity color (red if tinting is off), with its own selection box.
export function draw_worker(env: EntityRenderEnv, viewport_x: number, viewport_y: number, size: number, color: RGB | null, is_selected: boolean)
{
    const ctx = env.ctx;
    const center = env.canvas_center(viewport_x, viewport_y);
    ctx.fillStyle = color !== null ? `rgb(${color[0]}, ${color[1]}, ${color[2]})` : 'red';
    ctx.fillRect(center.x - size / 2, center.y - size / 2, size, size);
    if (is_selected) {
        ctx.strokeStyle = '#0c0';
        ctx.lineWidth = 2;
        ctx.strokeRect(center.x - size / 2 - 4, center.y - size / 2 - 4, size + 8, size + 8);
    }
}
