// Client backend (wasm) wrapper. The only file that knows about wasm exports and wasm memory layouts.
// Everything it returns to other files is plain JS values / objects.
import { ByteBuffer } from "./core.js";
import * as Core from "./core.js";
// Mirrors GAME_MESSAGE_TYPE (include/game_common/game_messages.h). Update here if the enum changes.
export var MESSAGE_TYPE;
(function (MESSAGE_TYPE) {
    MESSAGE_TYPE[MESSAGE_TYPE["MATCH_JOINED"] = 0] = "MATCH_JOINED";
    MESSAGE_TYPE[MESSAGE_TYPE["MATCH_ENDED"] = 1] = "MATCH_ENDED";
    MESSAGE_TYPE[MESSAGE_TYPE["SERVER_TICK"] = 2] = "SERVER_TICK";
    MESSAGE_TYPE[MESSAGE_TYPE["SERVER_TICK_BUNDLE"] = 3] = "SERVER_TICK_BUNDLE";
    MESSAGE_TYPE[MESSAGE_TYPE["CLIENT_TICK"] = 4] = "CLIENT_TICK";
    MESSAGE_TYPE[MESSAGE_TYPE["TYPE_COUNT"] = 5] = "TYPE_COUNT";
    MESSAGE_TYPE[MESSAGE_TYPE["INVALID"] = 6] = "INVALID";
})(MESSAGE_TYPE || (MESSAGE_TYPE = {}));
;
// Mirrors INPUT_EVENT_TYPE (include/game_client/game_client_backend.h). Update here if the enum changes.
export var INPUT_EVENT_TYPE;
(function (INPUT_EVENT_TYPE) {
    INPUT_EVENT_TYPE[INPUT_EVENT_TYPE["VIEWPORT_CONTROL"] = 0] = "VIEWPORT_CONTROL";
    INPUT_EVENT_TYPE[INPUT_EVENT_TYPE["SET_TARGET_LOC"] = 1] = "SET_TARGET_LOC";
})(INPUT_EVENT_TYPE || (INPUT_EVENT_TYPE = {}));
;
let CLIENT_BACKEND;
// Loads and instantiates the client backend. Rejects if loading fails.
export async function load(wasm_url) {
    console.log("Loading backend...");
    const result = await WebAssembly.instantiateStreaming(fetch(wasm_url), {});
    CLIENT_BACKEND = result.instance.exports;
}
// Initializes the backend's own memory. Call once, right after load().
export function start() {
    console.log("Starting backend.");
    if (!CLIENT_BACKEND.web_client_start()) {
        console.error("Failed to initialize client backend.");
        return false;
    }
    ClientInput.init();
    return true;
}
// Ticks the backend: applies pending viewport input and rebuilds the render state. Call once per animation frame.
export function tick(delta_time) {
    CLIENT_BACKEND.web_client_tick(delta_time);
}
// Copies a received websocket message's bytes into wasm memory and has the backend parse & apply it.
// Returns the handled message type (see MESSAGE_TYPE), or null if the message could not be processed.
export function process_net_message(bytes) {
    const bufferSize = CLIENT_BACKEND.client_get_net_message_buffer_size();
    if (bytes.length > bufferSize) {
        console.error(`Received message (${bytes.length} bytes) is larger than the client's net message buffer (${bufferSize} bytes). Dropping.`);
        return null;
    }
    const bufferOffset = CLIENT_BACKEND.client_get_net_message_buffer_offset();
    new ByteBuffer(CLIENT_BACKEND.memory.buffer, bufferOffset, bytes.length).set(bytes);
    const messageType = CLIENT_BACKEND.client_process_net_message(bytes.length);
    return messageType;
}
// Reads the message the backend built in response to the last process_net_message call, if any.
// Call this right after process_net_message and send the result over the websocket. Returns null if there's nothing to send.
export function read_pending_output_message() {
    const size = CLIENT_BACKEND.client_get_net_output_message_size();
    if (size === 0)
        return null;
    const bufferOffset = CLIENT_BACKEND.client_get_net_output_message_buffer_offset();
    // Copy out: the backend may overwrite this buffer the next time it builds an outgoing message.
    return new Core.ByteBuffer(CLIENT_BACKEND.memory.buffer, bufferOffset, size).slice();
}
// Input subsystem.
export var ClientInput;
(function (ClientInput) {
    let bufferOffset = 0;
    let bufferMaxSize = 0;
    // Initializes Buffer offset/size.
    function init() {
        bufferOffset = CLIENT_BACKEND.client_get_input_event_buffer_offset();
        bufferMaxSize = CLIENT_BACKEND.client_get_input_event_buffer_size();
    }
    ClientInput.init = init;
    // Fresh DataView over the input event buffer, after it gets zeroed.
    // Payload bytes can be written directly into the view. Call commit_input_event once it is ready.
    function begin_input_event() {
        new Uint8Array(CLIENT_BACKEND.memory.buffer, bufferOffset, bufferMaxSize).fill(0);
        return new DataView(CLIENT_BACKEND.memory.buffer, bufferOffset, bufferMaxSize);
    }
    ClientInput.begin_input_event = begin_input_event;
    // Reads outstanding input bytes written in the input buffer and uses them as the payload associated with the specified code.
    // Returns whether the event was interpret as a valid input. If it wasn't, it's likely the written bytes are not in sync with whatever backend input struct
    // they're supposed to mirror.
    function commit_input_event(code) {
        const valid = CLIENT_BACKEND.client_send_input_event(code) !== 0;
        if (!valid)
            console.error(`Input event ${INPUT_EVENT_TYPE[code]} was rejected by the backend - is backend.ts out of sync with game_client_backend.h?`);
        return valid;
    }
    ClientInput.commit_input_event = commit_input_event;
})(ClientInput || (ClientInput = {}));
export class BackendRenderEntity {
    viewport_x = 0;
    viewport_y = 0;
    target_viewport_x = 0;
    target_viewport_y = 0;
}
export class BackendRenderState {
    viewport_bottom_left_x = 0;
    viewport_bottom_left_y = 0;
    viewport_width = 0;
    viewport_height = 0;
    controlled_player_id = 0;
    world_size = new Core.WorldSize();
    entity_count = 0;
    entity_states = [];
}
// Reads the latest render state of the local match. Call once per animation frame, after tick().
// Mirrors client_render_state (include/game_client/game_client_backend.h) by hand. Keep both in sync.
export function read_render_state() {
    // Layout of client_render_state: viewport (bottom_left x/y i32, width ui16, height ui16), controlled_player_id
    // (ui16), world_size (ui16 width/height), entity_count (ui16), entity_states pointer (ui32). Views are
    // re-created on every read on purpose, as they become invalid if wasm memory ever grows.
    const renderStateOffset = CLIENT_BACKEND.client_get_render_state();
    const renderStateDataView = new DataView(CLIENT_BACKEND.memory.buffer, renderStateOffset);
    let renderState = new BackendRenderState();
    renderState.viewport_bottom_left_x = renderStateDataView.getInt32(0, true);
    renderState.viewport_bottom_left_y = renderStateDataView.getInt32(4, true);
    renderState.viewport_width = renderStateDataView.getUint16(8, true);
    renderState.viewport_height = renderStateDataView.getUint16(10, true);
    renderState.controlled_player_id = renderStateDataView.getUint16(12, true);
    renderState.world_size.width = renderStateDataView.getUint16(14, true);
    renderState.world_size.height = renderStateDataView.getUint16(16, true);
    renderState.entity_count = renderStateDataView.getUint16(18, true);
    const entityStatesOffset = renderStateDataView.getUint32(20, true);
    const entityStatesDataView = new DataView(CLIENT_BACKEND.memory.buffer, entityStatesOffset);
    // Layout of render_entity: four f32 (viewport X, Y, target viewport X, Y).
    const ENTITY_MEM_SIZE = 16;
    for (let i = 0; i < renderState.entity_count; i++) {
        renderState.entity_states.push({
            viewport_x: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i, true),
            viewport_y: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i + 4, true),
            target_viewport_x: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i + 8, true),
            target_viewport_y: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i + 12, true),
        });
    }
    return renderState;
}
