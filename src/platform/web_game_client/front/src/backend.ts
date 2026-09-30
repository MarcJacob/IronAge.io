// Client backend (wasm) wrapper. The only file that knows about wasm exports and wasm memory layouts.
// Everything it returns to other files is plain JS values / objects.

import { ByteBuffer } from "./core.js"
import * as Core from "./core.js";

// Mirrors GAME_MESSAGE_TYPE (include/game_common/game_messages.h). Update here if the enum changes.
export enum MESSAGE_TYPE
{
    MATCH_JOINED = 0,
    MATCH_ENDED = 1,
    SERVER_TICK = 2,
    SERVER_TICK_BUNDLE = 3,
    CLIENT_TICK = 4,

    TYPE_COUNT,
    INVALID,
};

// Mirrors INPUT_EVENT_TYPE (include/game_client/game_client_backend.h). Update here if the enum changes.
export enum INPUT_EVENT_TYPE
{
    VIEWPORT_CONTROL = 0,
    SET_TARGET_LOC = 1,
};

// Mirrors ENTITY_TYPE (include/game_common/match/world.h). Update here if the enum changes.
export enum ENTITY_TYPE
{
    INVALID = 0,
    SETTLEMENT = 1,
    CARAVAN = 2,
    ARMY = 3,
};

interface ClientExports extends WebAssembly.Exports
{
    memory: WebAssembly.Memory;

    // Lifecycle

    web_client_start(): number;
    web_client_tick(delta_time: number): void;

    // Net input & output

    client_get_net_message_buffer_size(): number;
    client_get_net_message_buffer_offset(): number;

    client_process_net_message(buff_size: number): MESSAGE_TYPE;

    client_get_net_output_message_size(): number;
    client_get_net_output_message_buffer_offset(): number;

    // Render state
    client_get_render_state(): number;

    // Input

    client_get_input_event_buffer_offset(): number;
    client_get_input_event_buffer_size(): number;
    client_send_input_event(code: INPUT_EVENT_TYPE): number; // wasm bool: 0 or 1.
}

let CLIENT_BACKEND: ClientExports;

// Loads and instantiates the client backend. Rejects if loading fails.
export async function load(wasm_url: string)
{
    console.log("Loading backend...");
    const result = await WebAssembly.instantiateStreaming(fetch(wasm_url), {});
    CLIENT_BACKEND = result.instance.exports as ClientExports;
}

// Initializes the backend's own memory. Call once, right after load().
export function start(): boolean
{
    console.log("Starting backend.");
    if (!CLIENT_BACKEND.web_client_start()) {
        console.error("Failed to initialize client backend.");
        return false;
    }

    ClientInput.init();
    return true;
}

// Ticks the backend: applies pending viewport input and rebuilds the render state. Call once per animation frame.
export function tick(delta_time: number): void
{
    CLIENT_BACKEND.web_client_tick(delta_time);
}

// Copies a received websocket message's bytes into wasm memory and has the backend parse & apply it.
// Returns the handled message type (see MESSAGE_TYPE), or null if the message could not be processed.
export function process_net_message(bytes: ByteBuffer)
{
    const bufferSize: number = CLIENT_BACKEND.client_get_net_message_buffer_size();
    if (bytes.length > bufferSize) {
        console.error(`Received message (${bytes.length} bytes) is larger than the client's net message buffer (${bufferSize} bytes). Dropping.`);
        return null; 
    }

    const bufferOffset: number = CLIENT_BACKEND.client_get_net_message_buffer_offset();
    new ByteBuffer(CLIENT_BACKEND.memory.buffer, bufferOffset, bytes.length).set(bytes);

    const messageType: MESSAGE_TYPE = CLIENT_BACKEND.client_process_net_message(bytes.length);
    return messageType;
}

// Reads the message the backend built in response to the last process_net_message call, if any.
// Call this right after process_net_message and send the result over the websocket. Returns null if there's nothing to send.
export function read_pending_output_message()
{
    const size: number = CLIENT_BACKEND.client_get_net_output_message_size();
    if (size === 0) return null;

    const bufferOffset: number = CLIENT_BACKEND.client_get_net_output_message_buffer_offset();
    // Copy out: the backend may overwrite this buffer the next time it builds an outgoing message.
    return new Core.ByteBuffer(CLIENT_BACKEND.memory.buffer, bufferOffset, size).slice() as Core.ByteBuffer;
}

// Input subsystem.
export namespace ClientInput
{
    let bufferOffset: number = 0;
    let bufferMaxSize: number = 0;

    // Initializes Buffer offset/size.
    export function init(): void
    {
        bufferOffset = CLIENT_BACKEND.client_get_input_event_buffer_offset();
        bufferMaxSize = CLIENT_BACKEND.client_get_input_event_buffer_size();
    }

    // Fresh DataView over the input event buffer, after it gets zeroed.
    // Payload bytes can be written directly into the view. Call commit_input_event once it is ready.
    export function begin_input_event(): DataView
    {
        new Uint8Array(CLIENT_BACKEND.memory.buffer, bufferOffset, bufferMaxSize).fill(0);
        return new DataView(CLIENT_BACKEND.memory.buffer, bufferOffset, bufferMaxSize);
    }

    // Reads outstanding input bytes written in the input buffer and uses them as the payload associated with the specified code.
    // Returns whether the event was interpret as a valid input. If it wasn't, it's likely the written bytes are not in sync with whatever backend input struct
    // they're supposed to mirror.
    export function commit_input_event(code: INPUT_EVENT_TYPE): boolean
    {
        const valid = CLIENT_BACKEND.client_send_input_event(code) !== 0;
        if (!valid) console.error(`Input event ${INPUT_EVENT_TYPE[code]} was rejected by the backend - is backend.ts out of sync with game_client_backend.h?`);
        return valid;
    }
}

export class BackendRenderEntity
{
    entity_type: ENTITY_TYPE = ENTITY_TYPE.INVALID;
    owner: number = 0;
    viewport_x: number = 0;
    viewport_y: number = 0;
    size_viewport: number = 0;
}

export class BackendRenderState
{
    viewport_bottom_left_x: number = 0;
    viewport_bottom_left_y: number = 0;
    viewport_width: number = 0;
    viewport_height: number = 0;
    controlled_player_id: number = 0;
    world_size: Core.WorldSize = new Core.WorldSize();
    entity_count: number = 0;
    entity_states: Array<BackendRenderEntity> = [];
}

// Reads the latest render state of the local match. Call once per animation frame, after tick().
// Mirrors client_render_state (include/game_client/game_client_backend.h) by hand. Keep both in sync.
export function read_render_state()
{
    // Layout of client_render_state: viewport (bottom_left x/y f32, width f32, height f32), controlled_player_id
    // (ui16), world_size (ui16 width/height), entity_count (ui16), entity_states pointer (ui32). Views are
    // re-created on every read on purpose, as they become invalid if wasm memory ever grows.
    const renderStateOffset: number = CLIENT_BACKEND.client_get_render_state();
    const renderStateDataView: DataView = new DataView(CLIENT_BACKEND.memory.buffer, renderStateOffset);

    let renderState = new BackendRenderState();
    renderState.viewport_bottom_left_x = renderStateDataView.getFloat32(0, true);
    renderState.viewport_bottom_left_y = renderStateDataView.getFloat32(4, true);
    renderState.viewport_width = renderStateDataView.getFloat32(8, true);
    renderState.viewport_height = renderStateDataView.getFloat32(12, true);
    renderState.controlled_player_id = renderStateDataView.getUint16(16, true);
    renderState.world_size.width = renderStateDataView.getUint16(18, true);
    renderState.world_size.height = renderStateDataView.getUint16(20, true);
    renderState.entity_count = renderStateDataView.getUint16(22, true);

    const entityStatesOffset: number = renderStateDataView.getUint32(24, true);
    const entityStatesDataView: DataView = new DataView(CLIENT_BACKEND.memory.buffer, entityStatesOffset);

    // Layout of render_entity: entity_type (ui8), owner (ui16), viewport_location (f32 x/y), size_viewport (ui8).
    const ENTITY_MEM_SIZE: number = 12;

    for (let i = 0; i < renderState.entity_count; i++) {
        renderState.entity_states.push(
            {
                entity_type: entityStatesDataView.getUint8(ENTITY_MEM_SIZE * i),
                owner: entityStatesDataView.getUint16(ENTITY_MEM_SIZE * i + 1, true),
                viewport_x: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i + 3, true),
                viewport_y: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i + 7, true),
                size_viewport: entityStatesDataView.getUint8(ENTITY_MEM_SIZE * i + 11),
            });
    }

    return renderState;
}
