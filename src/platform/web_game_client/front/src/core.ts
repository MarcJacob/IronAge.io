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
type OnMatchJoinedCallback = () => void;

const FRAME_CALLBACKS: FrameCallback[] = [];
let LAST_FRAME_TIME_MS: number | null = null;
let FRAME_LOOP_STARTED = false;

const ON_MATCH_JOINED_CALLBACKS: OnMatchJoinedCallback[] = [];

export function register_frame_callback(callback: FrameCallback): void
{
    FRAME_CALLBACKS.push(callback);
}

export function register_on_match_joined_callback(callback: OnMatchJoinedCallback): void
{
    ON_MATCH_JOINED_CALLBACKS.push(callback);
}

function frame_tick(nowMs: number): void
{
    if (LAST_FRAME_TIME_MS !== null)
    {
        const delta_time_s = (nowMs - LAST_FRAME_TIME_MS) / 1000;
        for (const callback of FRAME_CALLBACKS) callback(delta_time_s);
    }

    LAST_FRAME_TIME_MS = nowMs;
    requestAnimationFrame(frame_tick);
}

// Starts the frame loop. Idempotent - safe to call more than once.
export function start_frame_loop(): void
{
    if (FRAME_LOOP_STARTED) return;
    FRAME_LOOP_STARTED = true;

    requestAnimationFrame(frame_tick);
}

export function broadcast_on_match_joined(): void
{
    for (const callback of ON_MATCH_JOINED_CALLBACKS) callback();
}

export function broadcast_on_match_ended(): void
{
    // ... (match ended event for cleanup / menu nav).
}

