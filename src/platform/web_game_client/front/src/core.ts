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

// Sending bytes to the server. Set once by main.ts when the socket exists, used by modes to send messages out of the reply cycle.
let MESSAGE_SENDER: (bytes: ByteBuffer) => void = () => {};

export function set_message_sender(sender: (bytes: ByteBuffer) => void): void
{
    MESSAGE_SENDER = sender;
}

export function send_message(bytes: ByteBuffer): void
{
    MESSAGE_SENDER(bytes);
}

// MODES
// A mode is an object that owns what the player can currently see and do (UI shown, render config, what input means).
// Modes live in a stack: the top one receives input events, every one receives frame events and net messages (see Mode.is_top), the bottom one is the "base" mode, which is never
// popped (only replaced by start_mode_stack). Escape returns to the previous mode.
// Subclass Mode and override the events needed (see src/modes/).

export class Mode
{
    // Whether this mode is currently the top of the stack. Maintained by the stack functions, read-only for modes.
    // Modes below the top keep receiving frame updates and net messages: they are aware of their children and adapt to this.
    is_top: boolean = false;

    // Called when the mode becomes part of the stack (pushed, or started as base). is_top is already set.
    on_entry(): void {}
    // Called when the mode leaves the stack (popped, replaced, or the stack is restarted). Undo what on_entry set up.
    on_exit(): void {}
    // Called when a mode was pushed above this one (is_top is already false).
    on_suspended(): void {}
    // Called when the mode above this one was popped and this one is the top again (is_top is already true).
    on_resumed(): void {}
    // Called once per frame on every mode of the stack, bottom first.
    on_frame_update(delta_time_s: number): void {}

    // Input events, offered to the modes from the top of the stack down until one returns true (handled).
    // Returning false (the default: no handler) passes the event to the mode below. A mode that must keep an event from reaching the modes below returns true.
    on_click(event: MouseEvent): boolean { return false; }
    on_context_menu(event: MouseEvent): boolean { return false; }
    on_key_down(event: KeyboardEvent): boolean { return false; }
    // Escape pressed. Returns to the previous mode by default (handled). The base mode overrides it to do nothing (still handled).
    on_escape(): boolean { pop_mode(); return true; }

    // Server message received (see Backend.MESSAGE_TYPE). Sent to every mode of the stack, top first.
    on_net_message(message_type: number): void {}
}

const MODE_STACK: Mode[] = [];

export function get_top_mode(): Mode | null
{
    return MODE_STACK.length > 0 ? MODE_STACK[MODE_STACK.length - 1] : null;
}

// Exits the whole current stack (top first) and starts a new one with the given base mode.
export function start_mode_stack(base_mode: Mode): void
{
    while (MODE_STACK.length > 0)
    {
        const mode = MODE_STACK.pop()!;
        mode.is_top = false;
        mode.on_exit();
    }

    MODE_STACK.push(base_mode);
    base_mode.is_top = true;
    base_mode.on_entry();
}

// Puts a mode on top of the stack, started by the given parent mode (the mode calling this, which must be in the stack).
// Modes above the parent are popped first, one at a time (pop_mode: on_exit, then on_resumed of the one below), so a mode never ends up
// stacked on top of a sibling. If the parent is not in the stack, logs an error and pushes nothing.
export function push_mode(mode: Mode, parent: Mode): void
{
    if (!MODE_STACK.includes(parent))
    {
        console.error("push_mode: the parent mode is not in the mode stack, the mode was not pushed.");
        return;
    }

    while (get_top_mode() !== parent) pop_mode();

    const previousTop = get_top_mode();
    if (previousTop !== null)
    {
        previousTop.is_top = false;
        previousTop.on_suspended();
    }

    MODE_STACK.push(mode);
    mode.is_top = true;
    mode.on_entry();
}

// Removes the top mode and resumes the one below. Does nothing if only the base mode is left.
export function pop_mode(): void
{
    if (MODE_STACK.length <= 1) return;

    const popped = MODE_STACK.pop()!;
    popped.is_top = false;
    popped.on_exit();

    const newTop = MODE_STACK[MODE_STACK.length - 1];
    newTop.is_top = true;
    newTop.on_resumed();
}

// Swaps the top mode for another. The one below is not notified (it stays suspended). Does nothing on the base mode.
export function replace_top_mode(mode: Mode): void
{
    if (MODE_STACK.length <= 1) return;

    const replaced = MODE_STACK.pop()!;
    replaced.is_top = false;
    replaced.on_exit();

    MODE_STACK.push(mode);
    mode.is_top = true;
    mode.on_entry();
}

// Updates every mode of the stack, bottom first. Iterates over a copy: modes may change the stack during their update.
export function dispatch_frame_update(delta_time_s: number): void
{
    const modes = MODE_STACK.slice();
    for (const mode of modes) mode.on_frame_update(delta_time_s);
}

// Offers an input event to the modes of the stack, top first, until one handles it. Iterates over a copy: modes may change the stack while handling it.
function dispatch_input_event(handler: (mode: Mode) => boolean): void
{
    const modes = MODE_STACK.slice().reverse();
    for (const mode of modes)
    {
        if (handler(mode)) return;
    }
}

export function dispatch_click(event: MouseEvent): void { dispatch_input_event(mode => mode.on_click(event)); }
export function dispatch_context_menu(event: MouseEvent): void { dispatch_input_event(mode => mode.on_context_menu(event)); }
export function dispatch_key_down(event: KeyboardEvent): void { dispatch_input_event(mode => mode.on_key_down(event)); }
export function dispatch_escape(): void { dispatch_input_event(mode => mode.on_escape()); }

// Sends a server message to every mode of the stack, top first. Iterates over a copy: modes may change the stack while handling it.
export function dispatch_net_message(message_type: number): void
{
    const modes = MODE_STACK.slice().reverse();
    for (const mode of modes) mode.on_net_message(message_type);
}

// Per-frame delegate: a requestAnimationFrame loop that calls every registered callback once per frame, with
// the elapsed time since the last frame in seconds. Lets independent components (camera input, later
// interpolation, ...) each hook into the frame loop without owning one, or knowing about each other.

type FrameCallback = (delta_time_s: number) => void;
type OnMatchJoinedCallback = () => void;
type OnMatchEndedCallback = () => void;

const FRAME_CALLBACKS: FrameCallback[] = [];
let LAST_FRAME_TIME_MS: number | null = null;
let FRAME_LOOP_STARTED = false;

const ON_MATCH_JOINED_CALLBACKS: OnMatchJoinedCallback[] = [];
const ON_MATCH_ENDED_CALLBACKS: OnMatchEndedCallback[] = [];

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
        dispatch_frame_update(delta_time_s);
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

export function register_on_match_ended_callback(callback: OnMatchEndedCallback): void
{
    ON_MATCH_ENDED_CALLBACKS.push(callback);
}

export function broadcast_on_match_ended(): void
{
    for (const callback of ON_MATCH_ENDED_CALLBACKS) callback();
}

