// What render.ts provides to the entity modules of this folder (a leaf: types and shared constants only, no runtime import of render.ts).
// Modules draw or measure ONE entity from this env, so they do not depend on render.ts internals.

import type { RGB, RESOURCES_STORE, RESOURCE_SPRITE } from "../render.js"

export interface EntityRenderEnv
{
    ctx: CanvasRenderingContext2D;
    pixels_per_tile: number;
    canvas_center(viewport_x: number, viewport_y: number): { x: number, y: number }; // Canvas px of the tile center at a render state viewport position.
    resources: RESOURCES_STORE | null; // The sprite store of render.ts (null before it is loaded). Modules pick their own sprites from resources.ENTITY, see loaded_image.
    // Sprite centered on the viewport position, with optional owner tint (alpha 0.45), optional outline (CSS color) and the green selection box.
    draw_sprite(img: HTMLImageElement, viewport_x: number, viewport_y: number, size: number, color: RGB | null, outline_color: string | null, is_selected: boolean): void;
    draw_label(text: string, x: number, y: number): void;
}

// The image of a store sprite, or null while it is not loaded (or the store / sprite is missing).
export function loaded_image(sprite: RESOURCE_SPRITE | undefined | null): HTMLImageElement | null
{
    return sprite !== undefined && sprite !== null && sprite.loaded ? sprite.img : null;
}

// Size step (tiles) of the significance-based sizes: one unit per power of 2.
export const ENTITY_SCALE_UNIT = 0.1;
