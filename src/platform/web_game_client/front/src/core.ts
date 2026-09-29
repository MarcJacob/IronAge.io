// Common function & type declarations imported in every other src file.

// Commonly used type for bytes received / being sent to network.
export type ByteBuffer = Uint8Array<ArrayBuffer>;
export const ByteBuffer = Uint8Array;

export class WorldLocation
{
    x: number = 0;
    y: number = 0;
}

export class WorldSize
{
    width: number = 0;
    height: number = 0;
}

// Per-frame delegate: a requestAnimationFrame loop that calls every registered callback once per frame, with
// the elapsed time since the last frame in seconds. Lets independent components (camera input, later
// interpolation, ...) each hook into the frame loop without owning one, or knowing about each other.

type FrameCallback = (delta_time_s: number) => void;

const frameCallbacks: FrameCallback[] = [];
let lastFrameTimeMs: number | null = null;
let frameLoopStarted = false;

export function register_frame_callback(callback: FrameCallback): void
{
    frameCallbacks.push(callback);
}

function frame_tick(nowMs: number): void
{
    if (lastFrameTimeMs !== null)
    {
        const delta_time_s = (nowMs - lastFrameTimeMs) / 1000;
        for (const callback of frameCallbacks) callback(delta_time_s);
    }

    lastFrameTimeMs = nowMs;
    requestAnimationFrame(frame_tick);
}

// Starts the frame loop. Idempotent - safe to call more than once.
export function start_frame_loop(): void
{
    if (frameLoopStarted) return;
    frameLoopStarted = true;

    requestAnimationFrame(frame_tick);
}


