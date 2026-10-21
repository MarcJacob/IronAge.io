// Client backend (wasm) wrapper. The only file that knows about wasm exports and wasm memory layouts.
// Everything it returns to other files is plain JS values / objects.

import { ByteBuffer } from "./core.js"
import * as Core from "./core.js";

// Mirrors GAME_MESSAGE_TYPE (include/game_common/game_messages.h). Update here if the enum changes.
export enum MESSAGE_TYPE
{
    LOBBY_JOINED = 0,
    MATCH_STARTED = 1,
    MATCH_ENDED = 2,
    CLIENT_PLACEMENT_PICK = 3,
    SERVER_PLACEMENT_PICK = 4,
    SERVER_TICK = 5,
    SERVER_TICK_BUNDLE = 6,
    CLIENT_TICK = 7,

    TYPE_COUNT,
    INVALID,
};

// Mirrors INPUT_EVENT_TYPE (include/game_client/game_client_backend.h). Update here if the enum changes.
export enum INPUT_EVENT_TYPE
{
    VIEWPORT_CONTROL,
};

// Mirrors ENTITY_TYPE (include/game_common/match/world/world_entities.h). Update here if the enum changes.
export enum ENTITY_TYPE
{
    INVALID = 0,
    SETTLEMENT = 1,
    ARMY = 2,
    WORKER = 3,
    BATTLE = 4,
};

interface ClientExports extends WebAssembly.Exports
{
    memory: WebAssembly.Memory;

    // Lifecycle

    web_client_start(): number;
    web_client_tick(delta_time: number): void;
    web_client_end_match(): void;

    // Net input & output

    client_get_net_message_buffer_size(): number;
    client_get_net_message_buffer_offset(): number;

    client_process_net_message(buff_size: number): MESSAGE_TYPE;

    client_get_net_output_message_size(): number;
    client_get_net_output_message_buffer_offset(): number;

    // Render state
    client_render_state_get_viewport(): number; // Pointer to client_render_state::viewport_state.
    client_render_state_get_entities(): number; // Pointer to render_entity array.
    client_render_state_get_entity_count(): number; // Value (ui32).

    client_render_state_get_terrain_tiles(): number; // Pointer to TERRAIN_TILE_TYPE array (1 byte each).
    client_render_state_get_owner_settlement_tiles(): number; // Pointer to ui16 array with owner settlement slot per tile (valid if TILE_FLAG_OWNED).
    client_render_state_get_contestation_tiles(): number; // Pointer to byte array with contestation values.
    client_render_state_get_tile_flags(): number; // Pointer to byte array with TILE_FLAGS bits.

    web_client_get_world_size(): number; // Pointer to vec2<ui16> (x = width, y = height).
    web_client_get_controlled_player_index(): number; // Value (match_player_id, ui16).

    // Input

    client_get_input_event_buffer_offset(): number;
    client_get_input_event_buffer_size(): number;
    client_send_input_event(code: INPUT_EVENT_TYPE): number; // wasm bool: 0 or 1.

    // Commands built by property name (see gdo.ts). Names are NUL-terminated bytes written in the input event buffer.

    client_command_create(): number; // COMMAND_CREATE_RESULT, see gdo.ts.
    client_command_get_buffer_offset(): number;
    client_command_set_property(x: number, y: number): number; // wasm bool: 0 or 1.
    client_command_send(): number; // wasm bool: 0 or 1.
    client_command_query_valid(): number; // wasm bool: 0 or 1. Also fills the feedback GDO.
    client_command_get_feedback_offset(): number; // Offset of the match_command_validity_feedback GDO filled by the last query.

    // Entity inspection

    client_query_entity_gdo(guid: number): number; // Byte size of the entity GDO written in its buffer, 0 on failure.
    client_get_entity_gdo_buffer_offset(): number;

    // Game parameters and world state values

    client_get_game_parameters_gdo_offset(): number; // Fills and returns the offset of the game_parameters GDO buffer.
    client_query_world_age(): number; // Ticks, as a double (exact to 2^53). 0 without a match.
    client_get_player_state_gdo_offset(): number; // Fills and returns the offset of the local player's world_player_state GDO buffer (zeroed without a match).

    // Path query

    client_get_path_buffer_offset(): number;
    client_query_path(start_x: number, start_y: number, end_x: number, end_y: number, territory_guid: number): number; // Waypoint count written to the path buffer.
}

let CLIENT_BACKEND: ClientExports;

export let LAST_RENDER_STATE: RenderState | null = null;

// Loads and instantiates the client backend. Rejects if loading fails.
export async function load(wasm_url: string)
{
    console.log("Loading backend...");
    const result = await WebAssembly.instantiateStreaming(fetch(wasm_url), {});
    CLIENT_BACKEND = result.instance.exports as ClientExports;
}

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

// Ticks the backend client's own logic.
export function tick_web_client(delta_time: number): void
{
    CLIENT_BACKEND.web_client_tick(delta_time);
}

// Updates LAST_RENDER_STATE object.
export function update_render_state()
{
    LAST_RENDER_STATE = read_render_state();
    if (LAST_RENDER_STATE == null) console.error("Attempted to update Render State at an invalid time.");
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

// Payloads of the last processed message are read straight from the net message buffer (valid until the next process_net_message).
// game_message_header is packed: message_type (ui8), payload_size (ui16), so payloads start at offset 3.
const GAME_MESSAGE_PAYLOAD_OFFSET: number = 3;

// Mirrors game_match_start_params (include/game_common/match/start_params.h).
export interface MatchStartParams
{
    world_size_regions_x: number;
    world_size_regions_y: number;
    player_count: number;
    tick_rate: number;
    random_seed: number;
}

export interface LobbyState
{
    countdown_remaining_ms: number;
    player_count: number; // Clients currently in the lobby.
    match_slot_index: number;
    match_start_params: MatchStartParams;
}

// Mirrors game_message_payload_lobby_joined (include/game_common/game_messages.h), packed, 18 bytes:
// countdown_remaining_ms (ui32 @0), lobby_player_count (ui16 @4), match_slot_index (ui8 @6),
// match_start_params @7: world_size_regions (2x ui16 @7, @9), player_count (ui16 @11), tick_rate (ui8 @13), random_seed (ui32 @14).
// Only call after process_net_message returned LOBBY_JOINED.
// NOTE: reading game state straight from TS is a stopgap. High-level game state should later be managed by the client backend.
export function read_lobby_joined_payload(): LobbyState
{
    const view = new DataView(CLIENT_BACKEND.memory.buffer, CLIENT_BACKEND.client_get_net_message_buffer_offset() + GAME_MESSAGE_PAYLOAD_OFFSET, 18);
    return {
        countdown_remaining_ms: view.getUint32(0, true),
        player_count: view.getUint16(4, true),
        match_slot_index: view.getUint8(6),
        match_start_params: {
            world_size_regions_x: view.getUint16(7, true),
            world_size_regions_y: view.getUint16(9, true),
            player_count: view.getUint16(11, true),
            tick_rate: view.getUint8(13),
            random_seed: view.getUint32(14, true),
        },
    };
}

// Mirrors game_message_payload_match_ended: winner_id (ui16). Returns null if there is no winner (0xFFFF).
// Only call after process_net_message returned MATCH_ENDED.
export function read_match_ended_winner(): number | null
{
    const view = new DataView(CLIENT_BACKEND.memory.buffer, CLIENT_BACKEND.client_get_net_message_buffer_offset() + GAME_MESSAGE_PAYLOAD_OFFSET, 2);
    const winnerId = view.getUint16(0, true);
    return winnerId === 0xFFFF ? null : winnerId;
}

// Mirrors game_message_payload_match_started (game_messages.h), packed: placement_countdown_ms (ui32 @0), join_tick (ui32 @4),
// controlled_player_id (ui16 @8), match_start_params @10.
// Only call after process_net_message returned MATCH_STARTED.
export function read_match_started_placement_countdown_ms(): number
{
    const view = new DataView(CLIENT_BACKEND.memory.buffer, CLIENT_BACKEND.client_get_net_message_buffer_offset() + GAME_MESSAGE_PAYLOAD_OFFSET, 4);
    return view.getUint32(0, true);
}

export interface PlacementPick
{
    player_id: number;
    x: number;
    y: number;
}

// Mirrors game_message_payload_placement_pick, packed, 6 bytes: player_id (ui16 @0), picked_loc x / y (ui16 @2, @4), in tiles.
// Only call after process_net_message returned SERVER_PLACEMENT_PICK.
export function read_server_placement_pick(): PlacementPick
{
    const view = new DataView(CLIENT_BACKEND.memory.buffer, CLIENT_BACKEND.client_get_net_message_buffer_offset() + GAME_MESSAGE_PAYLOAD_OFFSET, 6);
    return { player_id: view.getUint16(0, true), x: view.getUint16(2, true), y: view.getUint16(4, true) };
}

// Builds a CLIENT_PLACEMENT_PICK message (header: type ui8, payload_size ui16; payload as above) for the given tile.
export function build_client_placement_pick(player_id: number, x: number, y: number): ByteBuffer
{
    const bytes = new ByteBuffer(9);
    const view = new DataView(bytes.buffer);
    view.setUint8(0, MESSAGE_TYPE.CLIENT_PLACEMENT_PICK);
    view.setUint16(1, 6, true);
    view.setUint16(3, player_id, true);
    view.setUint16(5, x, true);
    view.setUint16(7, y, true);
    return bytes;
}

// Has the backend drop the local match and free its resources, and forgets the last render state.
// Call after process_net_message returned MATCH_ENDED (and after reading its payload).
export function end_local_match(): void
{
    CLIENT_BACKEND.web_client_end_match();
    LAST_RENDER_STATE = null;
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

    // Commands built by property name: thin wrappers over the exports, see gdo.ts for how they are used.
    // The names (command, property) must have been written in the input event buffer with begin_input_event first.
    // Returns a COMMAND_CREATE_RESULT.
    export function command_create(): number { return CLIENT_BACKEND.client_command_create(); }
    export function command_set_property(x: number, y: number): boolean { return CLIENT_BACKEND.client_command_set_property(x, y) !== 0; }
    export function command_send(): boolean { return CLIENT_BACKEND.client_command_send() !== 0; }
    export function command_query_valid(): boolean { return CLIENT_BACKEND.client_command_query_valid() !== 0; }
    export function command_get_buffer_offset(): number { return CLIENT_BACKEND.client_command_get_buffer_offset(); }
    export function command_get_feedback_offset(): number { return CLIENT_BACKEND.client_command_get_feedback_offset(); }
}

// Entity inspection through Game Data Objects (see gdo.ts). The buffer is overwritten by every query: read it right away.
// Returns the byte size of the GDO written, 0 if the entity doesn't exist (anymore), has no GDO yet or it doesn't fit the buffer.
export function entity_gdo_query(guid: number): number { return CLIENT_BACKEND.client_query_entity_gdo(guid); }
export function entity_gdo_get_buffer_offset(): number { return CLIENT_BACKEND.client_get_entity_gdo_buffer_offset(); }

// Offset of the game_parameters GDO (include/game_common/match/game_parameters.h), refreshed by this call. Read it with gdo.ts (get_game_parameters).
export function game_parameters_gdo_get_offset(): number { return CLIENT_BACKEND.client_get_game_parameters_gdo_offset(); }

// Age of the local match's world in ticks (a JS number: the backend gives a double, so there is no 64 bit issue). 0 without a match.
export function get_world_age(): number { return CLIENT_BACKEND.client_query_world_age(); }

// Offset of the local player's world_player_state GDO (include/game_common/match/world/world.h), refreshed by this call. Read it with gdo.ts (get_local_player_state).
export function player_state_gdo_get_offset(): number { return CLIENT_BACKEND.client_get_player_state_gdo_offset(); }

// Fresh view of the whole wasm memory. Do NOT hold the result across calls into the backend, as memory growth detaches it.
export function get_memory_bytes(): Uint8Array<ArrayBuffer>
{
    return new Uint8Array(CLIENT_BACKEND.memory.buffer);
}

// Asks the backend for a land path between two world tiles. Returns its waypoints in order, start tile excluded:
// move in a straight line from the start to the first waypoint, then to the second, and so on. Empty if there is nothing to follow.
// If territory_guid is a settlement's guid, the path is limited to that settlement's territory.
// The path buffer holds (x, y) ui16 pairs (see web_game_client_query.cpp).
export function query_path(start: { x: number, y: number }, end: { x: number, y: number }, territory_guid: number = 0): { x: number, y: number }[]
{
    const count = CLIENT_BACKEND.client_query_path(start.x, start.y, end.x, end.y, territory_guid);
    const view = new DataView(CLIENT_BACKEND.memory.buffer, CLIENT_BACKEND.client_get_path_buffer_offset(), count * 4);

    const points: { x: number, y: number }[] = [];
    for (let i = 0; i < count; i++) points.push({ x: view.getUint16(i * 4, true), y: view.getUint16(i * 4 + 2, true) });
    return points;
}

export class RenderEntity
{
    entity_type: ENTITY_TYPE = ENTITY_TYPE.INVALID;
    owner: number = 0;
    guid: number = 0;
    viewport_x: number = 0;
    viewport_y: number = 0;
}

export class RenderState
{
    viewport_bottom_left_x: number = 0;
    viewport_bottom_left_y: number = 0;
    viewport_width: number = 0;
    viewport_height: number = 0;

    controlled_player_id: number = 0;
    world_size: Core.WorldSize = new Core.WorldSize();

    entity_count: number = 0;
    entity_states: Array<RenderEntity> = [];

    terrain_tiles_pointer: number = 0; // WASM memory offset of the terrain tile type bytes.
    owner_settlement_tiles_pointer: number = 0; // WASM memory offset of the tile owner settlement ui16s.
    contestation_tiles_pointer: number = 0; // WASM memory offset of the tile contestation bytes.
    tile_flags_pointer: number = 0; // WASM memory offset of the tile flags bytes.
}

// Fresh view of terrain type bytes (one TERRAIN_TILE_TYPE per tile, row 0 = world y 0) over wasm memory, or null if there is no match.
// Created anew on every call as wasm memory growth detaches older views: do NOT hold the result across frames.
export function get_terrain_tiles_view(): Uint8Array<ArrayBuffer> | null
{
    if (LAST_RENDER_STATE === null || LAST_RENDER_STATE.terrain_tiles_pointer === 0) return null;

    const world = LAST_RENDER_STATE.world_size;
    const length = world.width * world.height;
    return new Uint8Array(CLIENT_BACKEND.memory.buffer, LAST_RENDER_STATE.terrain_tiles_pointer, length);
}

// Fresh view of tile owner settlement slots (one ui16 per tile) over wasm memory, or null if there is no match.
// Only meaningful for tiles with TILE_FLAG_OWNED set.
// Created anew on every call as wasm memory growth detaches older views: do NOT hold the result across frames.
export function get_owner_settlement_tiles_view(): Uint16Array<ArrayBuffer> | null
{
    if (LAST_RENDER_STATE === null || LAST_RENDER_STATE.owner_settlement_tiles_pointer === 0) return null;

    const world = LAST_RENDER_STATE.world_size;
    const length = world.width * world.height;
    return new Uint16Array(CLIENT_BACKEND.memory.buffer, LAST_RENDER_STATE.owner_settlement_tiles_pointer, length);
}

// Fresh view of tile contestation bytes over wasm memory, or null if there is no match.
// Created anew on every call as wasm memory growth detaches older views: do NOT hold the result across frames.
export function get_contestation_tiles_view(): Uint8Array<ArrayBuffer> | null
{
    if (LAST_RENDER_STATE === null || LAST_RENDER_STATE.contestation_tiles_pointer === 0) return null;

    const world = LAST_RENDER_STATE.world_size;
    const length = world.width * world.height;
    return new Uint8Array(CLIENT_BACKEND.memory.buffer, LAST_RENDER_STATE.contestation_tiles_pointer, length);
}

// Fresh view of tile flags bytes (TILE_FLAGS bits) over wasm memory, or null if there is no match.
// Created anew on every call as wasm memory growth detaches older views: do NOT hold the result across frames.
export function get_tile_flags_view(): Uint8Array<ArrayBuffer> | null
{
    if (LAST_RENDER_STATE === null || LAST_RENDER_STATE.tile_flags_pointer === 0) return null;

    const world = LAST_RENDER_STATE.world_size;
    const length = world.width * world.height;
    return new Uint8Array(CLIENT_BACKEND.memory.buffer, LAST_RENDER_STATE.tile_flags_pointer, length);
}

// Reads the latest render state of the local match. Called once per animation frame, during tick().
// Mirrors client_render_state (include/game_client/game_client_backend.h) by hand. Keep both in sync.
function read_render_state()
{
    // Read through the client_render_state_get_* getters. Views are re-created on every read on purpose,
    // as they become invalid if wasm memory ever grows.
    let renderState = new RenderState();

    // viewport_state: bottom_left x/y (f32), dimensions width/height (f32). Packed, offsets 0 / 4 / 8 / 12.
    const viewportDataView: DataView = new DataView(CLIENT_BACKEND.memory.buffer, CLIENT_BACKEND.client_render_state_get_viewport());
    renderState.viewport_bottom_left_x = viewportDataView.getFloat32(0, true);
    renderState.viewport_bottom_left_y = viewportDataView.getFloat32(4, true);
    renderState.viewport_width = viewportDataView.getFloat32(8, true);
    renderState.viewport_height = viewportDataView.getFloat32(12, true);

    // world_size: vec2<ui16>, x = width, y = height.
    const worldSizeDataView: DataView = new DataView(CLIENT_BACKEND.memory.buffer, CLIENT_BACKEND.web_client_get_world_size());
    renderState.world_size.width = worldSizeDataView.getUint16(0, true);
    renderState.world_size.height = worldSizeDataView.getUint16(2, true);

    renderState.controlled_player_id = CLIENT_BACKEND.web_client_get_controlled_player_index();
    renderState.entity_count = CLIENT_BACKEND.client_render_state_get_entity_count();

    renderState.terrain_tiles_pointer = CLIENT_BACKEND.client_render_state_get_terrain_tiles();
    renderState.owner_settlement_tiles_pointer = CLIENT_BACKEND.client_render_state_get_owner_settlement_tiles();
    renderState.contestation_tiles_pointer = CLIENT_BACKEND.client_render_state_get_contestation_tiles();
    renderState.tile_flags_pointer = CLIENT_BACKEND.client_render_state_get_tile_flags();

    // ENTITIES

    const entityStatesOffset: number = CLIENT_BACKEND.client_render_state_get_entities();
    const entityStatesDataView: DataView = new DataView(CLIENT_BACKEND.memory.buffer, entityStatesOffset);

    // Layout of render_entity: entity_type (ui8), owner (ui16), guid (ui32, entity_guid), viewport_location (f32 x/y). 
    // Packed, offsets 0 / 1 / 3 / 7, sizeof 15 (static_assert'd in game_client_backend.h).
    const ENTITY_MEM_SIZE: number = 15;

    for (let i = 0; i < renderState.entity_count; i++) {
        renderState.entity_states.push(
            {
                entity_type: entityStatesDataView.getUint8(ENTITY_MEM_SIZE * i),
                owner: entityStatesDataView.getUint16(ENTITY_MEM_SIZE * i + 1, true),
                guid: entityStatesDataView.getUint32(ENTITY_MEM_SIZE * i + 3, true),
                viewport_x: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i + 7, true),
                viewport_y: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i + 11, true),
            });
    }

    return renderState;
}
