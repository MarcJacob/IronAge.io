// Frontend-owned camera state. Produces the world-space rectangle sent to the client backend.
// NOTE(Marc): For now this is all AI - generated and only reviewed by me, we'll see if I need to take a more direct hand in designing good-feeling camera movements.

import * as Core from "./core.js"
import * as Backend from "./backend.js"

export interface CameraPoint
{
    x: number;
    y: number;
}

export interface CameraRect
{
    min: CameraPoint;
    max: CameraPoint;
}

export interface CameraFraction
{
    x: number;
    y: number;
}

// The view's height in world tiles is what zoom controls; its width follows from the canvas aspect ratio (square tiles).
const MIN_VIEW_HEIGHT = 50;
const MAX_VIEW_HEIGHT = 1000;
const ZOOM_EASE_RATE = 12;
const PAN_SPEED_FRACTION = 0.5;
const CAMERA_CENTER_WORLD_MARGIN = 1000;

export class Camera
{
    private canvasWidth = 1;
    private canvasHeight = 1;

    private center: CameraPoint = { x: 0, y: 0 };
    private currentViewHeight = MAX_VIEW_HEIGHT;
    private targetViewHeight = MAX_VIEW_HEIGHT;

    private zoomAnchor: { world: CameraPoint, fraction: CameraFraction } | null = null;
    private worldSize: CameraPoint | null = null;

    set_canvas_size(width: number, height: number): void
    {
        this.canvasWidth = Math.max(1, width);
        this.canvasHeight = Math.max(1, height);

        // The center is kept as is on resize, only re-clamped.
        this.clamp_center();
    }

    set_view_center(world: CameraPoint): void
    {
        this.center = { x: world.x, y: world.y };
        this.clamp_center();
        this.zoomAnchor = null;
    }

    set_world_size(width: number, height: number): void
    {
        if (this.worldSize !== null) return;

        this.worldSize = { x: width, y: height };

        // Start with the world dimension corresponding to the smaller canvas axis filling that axis.
        // This avoids opening small worlds at the full 1000-tile zoom-out height.
        const smallestCanvasIsWidth = this.canvasWidth <= this.canvasHeight;
        const initialHeight = smallestCanvasIsWidth
            ? width * this.canvasHeight / this.canvasWidth
            : height;
        this.currentViewHeight = this.clamp_view_height(initialHeight);
        this.targetViewHeight = this.currentViewHeight;
    }

    pan(direction: CameraPoint, deltaTime: number): void
    {
        if (direction.x === 0 && direction.y === 0) return;

        const movement = this.currentViewHeight * PAN_SPEED_FRACTION * Math.max(0, deltaTime);
        this.center.x += direction.x * movement;
        this.center.y += direction.y * movement;
        this.clamp_center();
        this.zoomAnchor = null;
    }

    zoom_at(fraction: CameraFraction, wheelDelta: number): void
    {
        const clampedFraction = this.clamp_fraction(fraction);
        const world = this.world_at_fraction(clampedFraction);
        const zoomFactor = Math.exp(wheelDelta * 0.001);

        this.targetViewHeight = this.clamp_view_height(this.targetViewHeight * zoomFactor);
        this.zoomAnchor = { world, fraction: clampedFraction };
    }

    update(deltaTime: number): void
    {
        const interpolation = 1 - Math.exp(-ZOOM_EASE_RATE * Math.max(0, deltaTime));
        this.currentViewHeight += (this.targetViewHeight - this.currentViewHeight) * interpolation;

        if (this.zoomAnchor !== null)
        {
            const size = this.viewport_size();
            this.center.x = this.zoomAnchor.world.x + size.x * (0.5 - this.zoomAnchor.fraction.x);
            this.center.y = this.zoomAnchor.world.y + size.y * (0.5 - this.zoomAnchor.fraction.y);
            this.clamp_center();

            if (Math.abs(this.currentViewHeight - this.targetViewHeight) < 0.01)
            {
                this.currentViewHeight = this.targetViewHeight;
                this.zoomAnchor = null;
            }
        }
    }

    get_rect(): CameraRect
    {
        const size = this.viewport_size();
        return {
            min: {
                x: this.center.x - size.x / 2,
                y: this.center.y - size.y / 2,
            },
            max: {
                x: this.center.x + size.x / 2,
                y: this.center.y + size.y / 2,
            },
        };
    }

    world_at_fraction(fraction: CameraFraction): CameraPoint
    {
        const rect = this.get_rect();
        const clampedFraction = this.clamp_fraction(fraction);
        return {
            x: rect.min.x + (rect.max.x - rect.min.x) * clampedFraction.x,
            y: rect.min.y + (rect.max.y - rect.min.y) * clampedFraction.y,
        };
    }

    private clamp_center(): void
    {
        if (this.worldSize === null) return;

        this.center.x = Math.min(this.worldSize.x + CAMERA_CENTER_WORLD_MARGIN,
            Math.max(-CAMERA_CENTER_WORLD_MARGIN, this.center.x));
        this.center.y = Math.min(this.worldSize.y + CAMERA_CENTER_WORLD_MARGIN,
            Math.max(-CAMERA_CENTER_WORLD_MARGIN, this.center.y));
    }

    private viewport_size(): CameraPoint
    {
        return {
            x: this.currentViewHeight * this.canvasWidth / this.canvasHeight,
            y: this.currentViewHeight,
        };
    }

    private clamp_view_height(viewHeight: number): number
    {
        return Math.min(MAX_VIEW_HEIGHT, Math.max(MIN_VIEW_HEIGHT, viewHeight));
    }

    private clamp_fraction(fraction: CameraFraction): CameraFraction
    {
        return {
            x: Math.min(1, Math.max(0, fraction.x)),
            y: Math.min(1, Math.max(0, fraction.y)),
        };
    }
}

export const FRONTEND_CAMERA = new Camera();

function on_match_joined()
{
    // Assume backend has read its first render state.
    const worldSize = (Backend.LAST_RENDER_STATE as Backend.RenderState).world_size;

    // Init camera world size and center on world center.
    FRONTEND_CAMERA.set_world_size(worldSize.width, worldSize.height);
    FRONTEND_CAMERA.set_view_center({ x: worldSize.width / 2, y: worldSize.height / 2 });
}

export function init_camera_system()
{
    Core.register_on_match_joined_callback(on_match_joined);
}
