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

interface ClientExports extends WebAssembly.Exports
{
    memory: WebAssembly.Memory;

    // Net input & output

    client_get_net_message_buffer_size(): number;
    client_get_net_message_buffer_offset(): number;

    client_process_net_message(buff_size: number): MESSAGE_TYPE;

    client_get_net_output_message_size(): number;
    client_get_net_output_message_buffer_offset(): number;

    // Render state
    client_get_render_state(): number;
    client_refresh_render_state(): void;

    // Local match

    client_get_local_match_info(): number;

    client_get_controlled_player_id(): number;

    // Input

    client_input_set_target_loc(x: number, y: number): void;
    client_apply_viewport_input(pan_x: number, pan_y: number, zoom_delta: number, move_time: number): void;
}

let backend: ClientExports;

// Loads and instantiates the client backend. Rejects if loading fails.
export async function load(wasm_url: string)
{
    const result = await WebAssembly.instantiateStreaming(fetch(wasm_url), {});
    backend = result.instance.exports as ClientExports;
}

// Copies a received websocket message's bytes into wasm memory and has the backend parse & apply it.
// Returns the handled message type (see MESSAGE_TYPE), or null if the message could not be processed.
export function process_net_message(bytes: ByteBuffer)
{
    const bufferSize: number = backend.client_get_net_message_buffer_size();
    if (bytes.length > bufferSize) {
        console.error(`Received message (${bytes.length} bytes) is larger than the client's net message buffer (${bufferSize} bytes). Dropping.`);
        return null; 
    }

    const bufferOffset: number = backend.client_get_net_message_buffer_offset();
    new ByteBuffer(backend.memory.buffer, bufferOffset, bytes.length).set(bytes);

    const messageType: MESSAGE_TYPE = backend.client_process_net_message(bytes.length);
    return messageType;
}

// Reads the message the backend built in response to the last process_net_message call, if any.
// Call this right after process_net_message and send the result over the websocket. Returns null if there's nothing to send.
export function read_pending_output_message()
{
    const size: number = backend.client_get_net_output_message_size();
    if (size === 0) return null;

    const bufferOffset: number = backend.client_get_net_output_message_buffer_offset();
    // Copy out: the backend may overwrite this buffer the next time it builds an outgoing message.
    return new Core.ByteBuffer(backend.memory.buffer, bufferOffset, size).slice() as Core.ByteBuffer;
}

// Reads static info about the currently active local match into match_info. Call after a MATCH_JOINED message was processed.
// Mirrors web_client_match_info (src/client_web/game_client_backend.h) by hand. Keep both in sync.
export function read_match_info()
{
    // Layout of web_client_match_info: three ui32 (tick rate, world width, world height).
    const infoOffset: number = backend.client_get_local_match_info();
    const infoView: DataView = new DataView(backend.memory.buffer, infoOffset, 12);

    let matchInfo = new Core.MatchInfo();
    matchInfo.tick_rate = infoView.getUint32(0, true);
    matchInfo.world_size.width = infoView.getUint32(4, true);
    matchInfo.world_size.height = infoView.getUint32(8, true);

    return matchInfo;
}

// Reads which player id the client is currently in control of. Call after a MATCH_JOINED message was processed.
export function read_controlled_player_id()
{
    return backend.client_get_controlled_player_id();
}

export function set_target_loc(target_loc: Core.WorldLocation)
{
    backend.client_input_set_target_loc(target_loc.x, target_loc.y);
}

// pan_x / pan_y: normalized direction. zoom_delta: desired zoom level. Both need repeated calls to keep taking effect.
export function apply_viewport_input(pan_x: number, pan_y: number, zoom_delta: number, move_time: number): void
{
    backend.client_apply_viewport_input(pan_x, pan_y, zoom_delta, move_time);
}

export class BackendRenderEntity
{
    viewport_x: number = 0;
    viewport_y: number = 0;
    target_viewport_x: number = 0;
    target_viewport_y: number = 0;
}

export class BackendRenderState
{
    viewport_bottom_left_x: number = 0;
    viewport_bottom_left_y: number = 0;
    viewport_width: number = 0;
    viewport_height: number = 0;
    zoom_level: number = 0;
    entity_count: number = 0;
    entity_states: Array<BackendRenderEntity> = [];
}

// Rebuilds the render state from current match + viewport state. Call once per animation frame, before read_render_state.
export function refresh_render_state(): void
{
    backend.client_refresh_render_state();
}

// Reads the latest render state of the local match.
// Mirrors web_client_render_state (src/client_web/game_client_backend.h) by hand. Keep both in sync.
export function read_render_state()
{
    // Layout of web_client_render_state: viewport (bottom_left x/y ui16, width ui16, height ui16, zoom_level f32),
    // entity_count (ui16), entity_states pointer (ui32). Views are re-created on every read on purpose, as they
    // become invalid if wasm memory ever grows.
    const renderStateOffset: number = backend.client_get_render_state();
    const renderStateDataView: DataView = new DataView(backend.memory.buffer, renderStateOffset);

    let renderState = new BackendRenderState();
    renderState.viewport_bottom_left_x = renderStateDataView.getInt32(0, true);
    renderState.viewport_bottom_left_y = renderStateDataView.getInt32(4, true);
    renderState.viewport_width = renderStateDataView.getUint16(8, true);
    renderState.viewport_height = renderStateDataView.getUint16(10, true);
    renderState.zoom_level = renderStateDataView.getFloat32(12, true);
    renderState.entity_count = renderStateDataView.getUint16(16, true);

    const entityStatesOffset: number = renderStateDataView.getUint32(18, true);
    const entityStatesDataView: DataView = new DataView(backend.memory.buffer, entityStatesOffset);

    // Layout of render_entity: four f32 (viewport X, Y, target viewport X, Y).
    const ENTITY_MEM_SIZE: number = 16;

    for (let i = 0; i < renderState.entity_count; i++) {
        renderState.entity_states.push(
            {
                viewport_x: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i, true),
                viewport_y: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i + 4, true),
                target_viewport_x: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i + 8, true),
                target_viewport_y: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i + 12, true),
            });
    }

    return renderState;
}
