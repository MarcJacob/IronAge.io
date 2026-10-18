// Browser input events, forwarded to the current mode (see core.ts), plus a library of input state and input command senders
// (selection, camera input, send_*) the modes build on.

import * as Core from "./core.js"
import * as Backend from "./backend.js"
import * as Render from "./render.js"
import { read_GDO_schema, read_GDO_values, query_entity, GAME_DATA_PROP_TYPE, type EntityData, type GDOPropDef } from "./gdo.js"
import { FRONTEND_CAMERA } from "./camera.js"

// Input sources (updated on events, used on tick)
const KB_HELD_KEYS = new Set<string>();

// Input state (updated on tick and events).
let MOUSE_WORLD_LOC: { x: number, y: number } | null = null;

// GUID of the currently selected entity, if any.
let SELECTED_ENTITY_GUID: number | null = null;

export function get_selected_entity_guid(): number | null
{
    return SELECTED_ENTITY_GUID;
}

export function set_selected_entity_guid(guid: number | null): void
{
    SELECTED_ENTITY_GUID = guid;
}

export function clear_selected_entity(): void
{
    SELECTED_ENTITY_GUID = null;
}

function update_viewport_move_input()
{
    const moveInput = { x: 0, y: 0 };

    if (KB_HELD_KEYS.has('KeyA') || KB_HELD_KEYS.has('ArrowLeft')) moveInput.x -= 1;
    if (KB_HELD_KEYS.has('KeyD') || KB_HELD_KEYS.has('ArrowRight')) moveInput.x += 1;
    if (KB_HELD_KEYS.has('KeyS') || KB_HELD_KEYS.has('ArrowDown')) moveInput.y -= 1;
    if (KB_HELD_KEYS.has('KeyW') || KB_HELD_KEYS.has('ArrowUp')) moveInput.y += 1;

    // Normalize when non-zero input on both axis.
    if (moveInput.x !== 0 && moveInput.y !== 0)
    {
        const invLength = 1 / Math.sqrt(2);
        moveInput.x *= invLength;
        moveInput.y *= invLength;
    }

    return moveInput;
}

// World tile under the mouse (not clamped to the world) as of the last mousemove event, or null if the mouse hasn't moved over the canvas yet.
export function get_mouse_world_location(): { x: number, y: number } | null
{
    return MOUSE_WORLD_LOC;
}

// QUERY FUNCTIONS
export function get_selected_entity_view(): EntityData | null
{
    return SELECTED_ENTITY_GUID != null ? query_entity(SELECTED_ENTITY_GUID) : null;
}


// COMMANDS BUILT BY PROPERTY NAME
// A command is built in the backend through a Game Data Object (see gdo.ts), identified by its bare enum name (see
// MATCH_COMMAND_TABLE in commands.h) with its properties named after the command struct's members. Each sender supplies the names it expects:
// a name the backend does not know is logged and the call fails, which means the front end is out of date with commands.h.

// Value of a command property: a number, or a pair of numbers for vec2 and world location types.
type COMMAND_PROP_VALUE = number | { x: number, y: number };

// Mirrors COMMAND_CREATE_RESULT (src/platform/web_game_client/web_game_client_input_commands.cpp).
enum COMMAND_CREATE_RESULT
{
    OK,
    UNKNOWN_NAME,
    NO_GDO,
    TOO_LARGE,
}

const TEXT_ENCODER = new TextEncoder();

// Writes a name as NUL-terminated bytes in the input event buffer. Returns false if it does not fit.
function write_name(name: string): boolean
{
    const view = Backend.ClientInput.begin_input_event(); // Zeroed, so the name is terminated as long as it leaves room.
    const bytes = new Uint8Array(view.buffer, view.byteOffset, view.byteLength);
    return TEXT_ENCODER.encodeInto(name, bytes.subarray(0, bytes.length - 1)).read === name.length;
}

// Starts building the named command in the backend, replacing any command in progress. Returns false (and logs why) if the backend refused.
function create_command(command: string): boolean
{
    if (!write_name(command)) { console.error(`Command name "${command}" does not fit in the input event buffer.`); return false; }

    const result: COMMAND_CREATE_RESULT = Backend.ClientInput.command_create();
    if (result !== COMMAND_CREATE_RESULT.OK)
    {
        console.error(`Command ${command} could not be created (${COMMAND_CREATE_RESULT[result]}) - is the front end out of sync with commands.h?`);
        return false;
    }
    return true;
}

// Sets a property of the command in progress. Returns false (and logs why) if the backend refused.
function set_command_property(name: string, value: COMMAND_PROP_VALUE): boolean
{
    const x = typeof value === "number" ? value : value.x;
    const y = typeof value === "number" ? 0 : value.y;

    if (!write_name(name) || !Backend.ClientInput.command_set_property(x, y))
    {
        const schema = read_GDO_schema(Backend.ClientInput.command_get_buffer_offset());
        console.error(`Command property "${name}" was rejected by the backend - is the front end out of sync with commands.h? The command has: `
            + schema.map(prop => `${prop.name} (${GAME_DATA_PROP_TYPE[prop.type]} @${prop.offset})`).join(", "));
        return false;
    }
    return true;
}

// Builds the named command in the backend from name: value pairs. Unset properties are zero. Returns false if anything was refused.
function build_command(command: string, props: { [name: string]: COMMAND_PROP_VALUE }): boolean
{
    if (!create_command(command)) return false;
    for (const name in props)
    {
        if (!set_command_property(name, props[name])) return false;
    }
    return true;
}

// Queues the named command with the given properties.
export function send_command(command: string, props: { [name: string]: COMMAND_PROP_VALUE }): boolean
{
    if (!build_command(command, props)) return false;

    const accepted = Backend.ClientInput.command_send();
    if (!accepted) console.error(`Command ${command} was rejected by the backend.`);
    return accepted;
}

// Whether the named command with the given properties would currently be valid for the local player (same check as the server's).
export function is_command_valid(command: string, props: { [name: string]: COMMAND_PROP_VALUE }): boolean
{
    return build_command(command, props) && Backend.ClientInput.command_query_valid();
}

// Bits of the feedback's error_flags. Mirrors COMMAND_ERROR (commands.h) by hand: keep both in sync.
export const COMMAND_ERROR = {
    UNSPECIFIED: 1 << 0,
    INVALID_ENTITY: 1 << 1,
    NOT_OWNER: 1 << 2,
    TARGET_OUT_OF_BOUNDS: 1 << 3,
    INVALID_PATH: 1 << 4,
    TARGET_OUT_OF_TERRITORY: 1 << 5,
    NOT_ENOUGH_POPULATION: 1 << 6,
    ENTITY_LIMIT_REACHED: 1 << 7,
    ARMY_RAISING: 1 << 8,
    NO_RAISE_TARGET: 1 << 9,
} as const;

// Values of the feedback's invalid_path_reason. Mirrors COMMAND_INVALID_PATH_REASON (commands.h) by hand.
export const COMMAND_INVALID_PATH = {
    NONE: 0,
    ORIGIN_IS_DESTINATION: 1,
    ORIGIN_INVALID: 2,
    DESTINATION_INVALID: 3,
} as const;

// What the validity check found out about a command: every property of match_command_validity_feedback (commands.h) by name (error_flags, ...),
// plus valid (no error flag set). Empty values (valid false only) if the command could not be built.
export type CommandFeedback = { valid: boolean, values: { [name: string]: number } };

let FEEDBACK_SCHEMA: GDOPropDef[] | null = null;

// Asks the validity check why the named command with the given properties would or would not be valid for the local player.
export function get_command_feedback(command: string, props: { [name: string]: COMMAND_PROP_VALUE }): CommandFeedback
{
    if (!build_command(command, props)) return { valid: false, values: {} };

    Backend.ClientInput.command_query_valid();

    // The feedback's schema never changes: read it once.
    const offset = Backend.ClientInput.command_get_feedback_offset();
    if (FEEDBACK_SCHEMA === null) FEEDBACK_SCHEMA = read_GDO_schema(offset);

    const values = read_GDO_values(offset, FEEDBACK_SCHEMA);
    return { valid: values.error_flags === 0, values };
}

// EVENT HANDLERS

// What mouse clicks and keys mean is up to the modes: these handlers only forward to them (see the dispatch functions in core.ts).
function on_pointer_click(clickEvent: MouseEvent)
{
    Core.dispatch_click(clickEvent);
}

function on_context_menu(menuEvent: MouseEvent)
{
    menuEvent.preventDefault();
    Core.dispatch_context_menu(menuEvent);
}

function on_mouse_move(moveEvent: MouseEvent)
{
    MOUSE_WORLD_LOC = Render.page_to_tile(moveEvent.clientX, moveEvent.clientY);
    Render.set_hover_page_pos(moveEvent.clientX, moveEvent.clientY);
}

function on_mouse_leave()
{
    Render.set_hover_page_pos(null, null);
}

function on_mouse_wheel(wheelEvent: WheelEvent)
{
    FRONTEND_CAMERA.zoom_at(Render.page_to_viewport_fraction(wheelEvent.clientX, wheelEvent.clientY), wheelEvent.deltaY);
    wheelEvent.preventDefault();
}

// Main lifecycle functions
export function init_input(canvas_element: HTMLCanvasElement)
{
    // Register front-end canvas input events

    // Mouse. Only events targeting the canvas itself count, so HUD widgets overlaid on it don't pan / zoom / select / order.
    const canvas_only = <E extends Event>(handler: (event: E) => void) =>
        (event: E) => { if (event.target === canvas_element) handler(event); };

    canvas_element.addEventListener('click', canvas_only(on_pointer_click));
    canvas_element.addEventListener('contextmenu', canvas_only(on_context_menu));
    canvas_element.addEventListener('mousemove', canvas_only(on_mouse_move));
    canvas_element.addEventListener('mouseleave', on_mouse_leave);
    canvas_element.addEventListener('wheel', canvas_only(on_mouse_wheel), { passive: false });

    // Keyboard
    const movementKeyCodes = new Set(['KeyA', 'KeyD', 'KeyS', 'KeyW', 'ArrowLeft', 'ArrowRight', 'ArrowDown', 'ArrowUp', 'NumpadAdd', 'NumpadSubtract']);
    window.addEventListener('keydown', (keyEvent) => {
        if (movementKeyCodes.has(keyEvent.code)) {
            KB_HELD_KEYS.add(keyEvent.code);
            keyEvent.preventDefault();
        }

        if (keyEvent.code === 'Escape') Core.dispatch_escape();
        else Core.dispatch_key_down(keyEvent);
    });
    window.addEventListener('keyup', (keyEvent) => KB_HELD_KEYS.delete(keyEvent.code));
    window.addEventListener('blur', () => KB_HELD_KEYS.clear());
}

// Applies held camera keys, updates the camera and sends the resulting viewport to the backend. Call once per frame in modes with a map view.
export function update_camera_input(delta_time: number)
{
    FRONTEND_CAMERA.pan(update_viewport_move_input(), delta_time);
    
    // Handle held zoom keys (numpad + and -)
    if (KB_HELD_KEYS.has('NumpadAdd')) {
        FRONTEND_CAMERA.zoom_at({ x: 0.5, y: 0.5 }, 250 * delta_time); // Zoom in at a constant rate
    } else if (KB_HELD_KEYS.has('NumpadSubtract')) {
        FRONTEND_CAMERA.zoom_at({ x: 0.5, y: 0.5 }, -250 * delta_time); // Zoom out at a constant rate
    }
    
    FRONTEND_CAMERA.update(delta_time);

    // Layout of input_event_payload_viewport_control (game_client_backend.h):
    // f32 view_rect_min_x, view_rect_min_y, view_rect_max_x, view_rect_max_y.
    const viewRect = FRONTEND_CAMERA.get_rect();
    const input_event_buff = Backend.ClientInput.begin_input_event();
    input_event_buff.setFloat32(0, viewRect.min.x, true);
    input_event_buff.setFloat32(4, viewRect.min.y, true);
    input_event_buff.setFloat32(8, viewRect.max.x, true);
    input_event_buff.setFloat32(12, viewRect.max.y, true);

    Backend.ClientInput.commit_input_event(Backend.INPUT_EVENT_TYPE.VIEWPORT_CONTROL);
}

