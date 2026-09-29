// Common function & type declarations imported in every other src file.
export const ByteBuffer = Uint8Array;
export class WorldLocation {
    x = 0;
    y = 0;
}
export class WorldSize {
    width = 0;
    height = 0;
}
const frameCallbacks = [];
let lastFrameTimeMs = null;
let frameLoopStarted = false;
export function register_frame_callback(callback) {
    frameCallbacks.push(callback);
}
function frame_tick(nowMs) {
    if (lastFrameTimeMs !== null) {
        const delta_time_s = (nowMs - lastFrameTimeMs) / 1000;
        for (const callback of frameCallbacks)
            callback(delta_time_s);
    }
    lastFrameTimeMs = nowMs;
    requestAnimationFrame(frame_tick);
}
// Starts the frame loop. Idempotent - safe to call more than once.
export function start_frame_loop() {
    if (frameLoopStarted)
        return;
    frameLoopStarted = true;
    requestAnimationFrame(frame_tick);
}
