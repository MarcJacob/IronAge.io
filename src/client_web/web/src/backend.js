// Client backend (wasm) wrapper. The only file that knows about wasm exports and wasm memory layouts.
// Everything it returns to other files is plain JS values / objects.

/** @type {WebAssembly.Exports} */
let backend = null;

// Mirrors GAME_MESSAGE_TYPE (include/game_common/game_messages.h). Update here if the enum changes.
export const MESSAGE_TYPE = {
    MATCH_JOINED: 0,
    MATCH_ENDED: 1,
    SERVER_TICK: 2,
    SERVER_TICK_BUNDLE: 3,
    CLIENT_TICK: 4,
};
const UNHANDLED_MESSAGE_TYPE = 5; // GAME_MESSAGE_TYPE::TYPE_COUNT: returned by the backend when a message could not be processed.

// Static info about the local match. Filled by read_match_info(), once a MATCH_JOINED message has been processed.
export const match_info = {
    tick_rate: 0,
    world_width: 0,
    world_height: 0,
};

// Loads and instantiates the client backend. Rejects if loading fails.
export async function load_backend(wasm_url) {
    const result = await WebAssembly.instantiateStreaming(fetch(wasm_url), {});
    backend = result.instance.exports;
}

// Copies a received websocket message's bytes into wasm memory and has the backend parse & apply it.
// Returns the handled message type (see MESSAGE_TYPE), or null if the message could not be processed.
export function process_net_message(bytes) {
    const bufferSize = backend.client_get_net_message_buffer_size();
    if (bytes.length > bufferSize) {
        console.error(`Received message (${bytes.length} bytes) is larger than the client's net message buffer (${bufferSize} bytes). Dropping.`);
        return null;
    }

    const bufferOffset = backend.client_get_net_message_buffer();
    new Uint8Array(backend.memory.buffer, bufferOffset, bytes.length).set(bytes);

    const messageType = backend.client_process_net_message(bytes.length);
    return messageType === UNHANDLED_MESSAGE_TYPE ? null : messageType;
}

// Reads the message the backend built in response to the last process_net_message call, if any.
// Call this right after process_net_message and send the result over the websocket. Returns null if there's nothing to send.
export function read_pending_output_message() {
    const size = backend.client_get_pending_output_message_size();
    if (size === 0) return null;

    const bufferOffset = backend.client_get_pending_output_message_buffer();
    // Copy out: the backend may overwrite this buffer the next time it builds an outgoing message.
    return new Uint8Array(backend.memory.buffer, bufferOffset, size).slice();
}

// Reads static info about the currently active local match into match_info. Call after a MATCH_JOINED message was processed.
export function read_match_info() {
    // Layout of web_client_match_info: three ui32 (tick rate, world width, world height).
    const infoOffset = backend.client_get_local_match_info();
    const infoView = new DataView(backend.memory.buffer, infoOffset, 12);

    match_info.tick_rate = infoView.getUint32(0, true);
    match_info.world_width = infoView.getUint32(4, true);
    match_info.world_height = infoView.getUint32(8, true);
}

// Reads which player id the client is currently in control of. Call after a MATCH_JOINED message was processed.
export function read_controlled_player_id() {
    return backend.client_get_controlled_player_id();
}

export function set_target_loc(x, y) {
    backend.client_input_set_target_loc(x, y);
}

// Reads the latest render state of the local match.
export function read_render_state() {
    // Layout of web_client_render_state: four i32 (entity X, Y, target X, Y). Views are re-created on every read on purpose,
    // as they become invalid if wasm memory ever grows.
    const renderStateOffset = backend.client_get_render_state();
    const renderStateDataView = new DataView(backend.memory.buffer, renderStateOffset);

    let renderState =
    {
        entity_count: renderStateDataView.getUint16(0, true),
        entity_states: [],
    };

    const entityStatesOffset = renderStateDataView.getUint32(2, true);
    const entityStatesDataView = new DataView(backend.memory.buffer, entityStatesOffset);

    const ENTITY_MEM_SIZE = 8;

    for (let i = 0; i < renderState.entity_count; i++) {
        renderState.entity_states.push({
            loc: {
                x: entityStatesDataView.getUint16(ENTITY_MEM_SIZE * i, true),
                y: entityStatesDataView.getUint16(ENTITY_MEM_SIZE * i + 2, true),
            },
            target_loc: {
                x: entityStatesDataView.getUint16(ENTITY_MEM_SIZE * i + 4, true),
                y: entityStatesDataView.getUint16(ENTITY_MEM_SIZE * i + 6, true),
            },
            });
    }

    return renderState;
}
