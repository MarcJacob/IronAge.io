// Client backend (wasm) wrapper. The only file that knows about wasm exports and wasm memory layouts.
// Everything it returns to other files is plain JS values / objects.

/** @type {WebAssembly.Exports} */
let backend = null;

// Static info about the local match. Filled by begin_match().
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

// Starts a local match and reads its static info into match_info. Returns false if anything went wrong.
export function begin_match() {
    if (!backend.client_begin_match()) {
        console.error("Failed to start local match.");
        return false;
    }

    // Layout of web_client_match_info: three ui32 (tick rate, world width, world height).
    const infoOffset = backend.client_get_local_match_info();
    const infoView = new DataView(backend.memory.buffer, infoOffset, 12);

    match_info.tick_rate = infoView.getUint32(0, true);
    match_info.world_width = infoView.getUint32(4, true);
    match_info.world_height = infoView.getUint32(8, true);

    if (match_info.tick_rate == 0 || match_info.world_width == 0 || match_info.world_height == 0) {
        console.error("Invalid local match info.", match_info);
        return false;
    }

    return true;
}

export function tick_match() {
    backend.client_tick_match();
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
                x: entityStatesDataView.getUint16(ENTITY_MEM_SIZE * i),
                y: entityStatesDataView.getUint16(ENTITY_MEM_SIZE * i + 2),
            },
            target_loc: {
                x: entityStatesDataView.getUint16(ENTITY_MEM_SIZE * i + 4),
                y: entityStatesDataView.getUint16(ENTITY_MEM_SIZE * i + 6),
            },
            });
    }

    return renderState;
}
