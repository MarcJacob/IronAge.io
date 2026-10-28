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

    REQUEST_AUTHENTICATION = 8,
    AUTHENTICATED = 9,
    SERVER_LOBBY_LIST = 10,
    REQUEST_JOIN_LOBBY = 11,
    JOIN_LOBBY_RESPONSE = 12,
    SIGNAL_LEAVE_LOBBY = 13,
    LOBBY_CLIENT_KICKED = 14,

    LOBBY_ADMIN_START_MATCH = 15,
    LOBBY_ADMIN_SET_PLAYER_COUNT = 16,
    LOBBY_ADMIN_SET_MAP_SIZE = 17,
    LOBBY_ADMIN_SET_SEED = 18,
    LOBBY_ADMIN_SET_TICK_RATE = 19,

    TYPE_COUNT,
    INVALID,
};

// Mirrors CLIENT_STATE (include/game_client/game_client_backend.h). Update here if the enum changes.
export enum CLIENT_STATE
{
    CONNECTING = 0,
    AUTHENTICATING = 1,
    LOBBY_LIST = 2,
    JOINING_LOBBY = 3,
    IN_LOBBY = 4,
    IN_MATCH = 5,
};

// Mirrors JOIN_LOBBY_RESULT (include/game_common/game_messages.h). Update here if the enum changes.
export enum JOIN_LOBBY_RESULT
{
    SUCCESS = 0,
    NOT_AUTHENTICATED = 1,
    INVALID_LOBBY = 2,
    LOBBY_FULL = 3,
    ALREADY_IN_LOBBY = 4,
};

// Mirrors LOBBY_KICK_REASON (include/game_common/game_messages.h). Update here if the enum changes.
export enum LOBBY_KICK_REASON
{
    PLAYER_CAP_LOWERED = 0,
};

// Mirror constants of include/game_common/game_messages.h. Update here if they change.
export const PLAYER_NAME_MAX_LENGTH = 32;
export const LOBBY_MAX_PLAYER_COUNT = 32;
export const LOBBY_MAX_MAP_SIZE_REGIONS = 8;
export const LOBBY_MIN_TICK_RATE = 5;
export const LOBBY_MAX_TICK_RATE = 120;

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
    CARAVAN = 5,
};

// Mirrors WORK_TYPE (include/game_common/match/world/world_entities.h), the worker GDO's work_type. Update here if the enum changes.
export enum WORK_TYPE
{
    FOUND_SETTLEMENT = 0,
    BUILD_ROAD = 1,
    BUILD_SETTLEMENT_BUILDING = 2,
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

    // Client state (web_game_client_state.cpp)

    client_get_state(): CLIENT_STATE;
    client_get_own_name_offset(): number; // Pointer to a player_name.
    client_get_player_name_offset(player_index: number): number; // Pointer to the characters of a match player's name (0 if none).
    client_get_player_name_length(player_index: number): number; // 0 for an out of range index.
    client_get_lobby_list_offset(): number; // Pointer to game_message_payload_server_lobby_list.
    client_get_lobby_state_offset(): number; // Pointer to game_lobby_state.
    client_get_join_response_offset(): number; // Pointer to game_message_payload_join_lobby_response.
    client_was_kicked(): number; // wasm bool: 0 or 1.
    client_get_kick_reason(): LOBBY_KICK_REASON;

    // Requests: each returns the size of the message built in the request buffer, 0 if not allowed in the current state.
    client_get_net_request_buffer_offset(): number;
    client_request_authentication(name_length: number): number; // Name bytes read from the input event buffer.
    client_request_join_lobby(lobby_id: number): number;
    client_signal_leave_lobby(): number;
    client_lobby_admin_start_match(): number;
    client_lobby_admin_set_player_count(player_count: number): number;
    client_lobby_admin_set_map_size(width_regions: number, height_regions: number): number;
    client_lobby_admin_set_seed(random_seed: number): number;
    client_lobby_admin_set_tick_rate(tick_rate: number): number;

    // Render state
    client_render_state_get_viewport(): number; // Pointer to client_render_state::viewport_state.
    client_render_state_get_entities(): number; // Pointer to render_entity array.
    client_render_state_get_entity_count(): number; // Value (ui32).

    client_render_state_get_terrain_tiles(): number; // Pointer to TERRAIN_TILE_TYPE array (1 byte each).
    client_render_state_get_owner_settlement_tiles(): number; // Pointer to ui16 array with owner settlement slot per tile (valid if TILE_FLAG_OWNED).
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

    client_inspect_entity(guid: number): number; // Byte size written in the inspection buffer (entity GDO + extra data GDO if any), 0 on failure.
    client_get_inspected_entity_gdo(): number; // Offset of the entity GDO, at the start of the inspection buffer.
    client_get_inspected_entity_extra_data(): number; // Offset of the extra data GDO, right after the entity GDO (only there if the inspected size is above the GDO's size).

    // Settlement building definitions

    client_get_building_definition_gdo(type: number): number; // Fills and returns the offset of the GDO of a row of SETTLEMENT_BUILDING_TABLE, 0 if there is no such type.

    // Game parameters and world state values

    client_get_game_parameters_gdo_offset(): number; // Fills and returns the offset of the game_parameters GDO buffer.
    client_query_world_age(): number; // Ticks, as a double (exact to 2^53). 0 without a match.
    client_get_player_state_gdo_offset(): number; // Fills and returns the offset of the local player's world_player_state GDO buffer (zeroed without a match).

    // Path query

    client_get_path_buffer_offset(): number;
    client_query_path(start_x: number, start_y: number, end_x: number, end_y: number, territory_guid: number, path_flags: number): number; // path_flags: bit 0 tile mode, bit 1 water. Point count written to the path buffer.
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
    procedural_map: boolean;
    map_gen: MapGenParams; // Only used if procedural_map is set.
}

// Mirrors game_map_gen_params (include/game_common/match/map_gen_params.h).
export interface MapGenParams
{
    sea_level_bias: number; // Signed.
    hills_bias: number; // Signed.
    mountains_bias: number; // Signed.
    feature_scale_tiles: number; // 0 = default.
}

export interface LobbyState
{
    countdown_remaining_ms: number; // 0 when no countdown runs (always in a test lobby).
    player_count: number; // Humans currently in the lobby. AI players fill the rest of match_start_params.player_count (the player cap).
    match_slot_index: number; // Lobby id: what client_request_join_lobby takes.
    match_start_params: MatchStartParams; // Player cap, world size, seed...
    test_mode: boolean; // Test lobby: no automatic start, admin requests accepted.
}

// Mirrors game_lobby_state (include/game_common/game_messages.h), packed, 25 bytes:
// countdown_remaining_ms (ui32 @0), human_count (ui16 @4), lobby_id (ui8 @6),
// match_start_params @7: world_size_regions (2x ui16 @7, @9), player_count (ui16 @11), tick_rate (ui8 @13), random_seed (ui32 @14),
// procedural_map (ui8 @18), map_gen @19: sea_level_bias (i8 @19), hills_bias (i8 @20), mountains_bias (i8 @21), feature_scale_tiles (ui16 @22),
// test_mode (ui8 @24).
const LOBBY_STATE_SIZE: number = 25;
function read_lobby_state_at(offset: number): LobbyState
{
    const view = new DataView(CLIENT_BACKEND.memory.buffer, offset, LOBBY_STATE_SIZE);
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
            procedural_map: view.getUint8(18) !== 0,
            map_gen: {
                sea_level_bias: view.getInt8(19),
                hills_bias: view.getInt8(20),
                mountains_bias: view.getInt8(21),
                feature_scale_tiles: view.getUint16(22, true),
            },
        },
        test_mode: view.getUint8(24) !== 0,
    };
}

// State of the lobby the client is in, kept by the backend from the last LOBBY_JOINED message (zeroed outside of a lobby).
export function read_lobby_joined_payload(): LobbyState
{
    return read_lobby_state_at(CLIENT_BACKEND.client_get_lobby_state_offset());
}

// CLIENT STATE

export function get_client_state(): CLIENT_STATE { return CLIENT_BACKEND.client_get_state(); }

// Name kept by the server (empty before authentication).
// Mirrors player_name (ia_static_string<PLAYER_NAME_MAX_LENGTH>, game_messages.h), 36 bytes: chars (not zero-terminated) @0, length (ui32 @32).
export function read_own_name(): string
{
    const offset = CLIENT_BACKEND.client_get_own_name_offset();
    const length = Math.min(new DataView(CLIENT_BACKEND.memory.buffer, offset + PLAYER_NAME_MAX_LENGTH, 4).getUint32(0, true), PLAYER_NAME_MAX_LENGTH);
    return new TextDecoder().decode(new Uint8Array(CLIENT_BACKEND.memory.buffer, offset, length));
}

// Name of a player of the current match, received at match start. "" for an out of range, neutral or unowned (0xFFFF) index.
export function get_player_name(player_index: number): string
{
    const length = CLIENT_BACKEND.client_get_player_name_length(player_index);
    if (length === 0) return "";
    return new TextDecoder().decode(new Uint8Array(CLIENT_BACKEND.memory.buffer, CLIENT_BACKEND.client_get_player_name_offset(player_index), length));
}

// Last lobby list received. Mirrors game_message_payload_server_lobby_list, packed: lobby_count (ui8 @0), then game_lobby_state entries from @1.
export function read_lobby_list(): LobbyState[]
{
    const offset = CLIENT_BACKEND.client_get_lobby_list_offset();
    const count = new DataView(CLIENT_BACKEND.memory.buffer, offset, 1).getUint8(0);

    const lobbies: LobbyState[] = [];
    for (let i = 0; i < count; i++) lobbies.push(read_lobby_state_at(offset + 1 + i * LOBBY_STATE_SIZE));
    return lobbies;
}

// Whether the lobby is full of humans (joining it would fail).
export function is_lobby_full(lobby: LobbyState): boolean { return lobby.player_count >= lobby.match_start_params.player_count; }

// Last join response received. Mirrors game_message_payload_join_lobby_response, packed: lobby_id (ui8 @0), result (ui8 @1).
export function read_join_response(): { lobby_id: number, result: JOIN_LOBBY_RESULT }
{
    const view = new DataView(CLIENT_BACKEND.memory.buffer, CLIENT_BACKEND.client_get_join_response_offset(), 2);
    return { lobby_id: view.getUint8(0), result: view.getUint8(1) };
}

// Reason of the last kick from a lobby, null if not kicked since the last join request.
export function read_kick_notice(): LOBBY_KICK_REASON | null
{
    return CLIENT_BACKEND.client_was_kicked() !== 0 ? CLIENT_BACKEND.client_get_kick_reason() : null;
}

// Copies out the request the backend just built (size from the export that built it). Null if it refused (state does not allow it).
function take_request(size: number): ByteBuffer | null
{
    if (size === 0) return null;
    return new ByteBuffer(CLIENT_BACKEND.memory.buffer, CLIENT_BACKEND.client_get_net_request_buffer_offset(), size).slice() as ByteBuffer;
}

// Request builders: each returns the message to send right away (Core.send_message), or null if the current state does not allow it.

// Empty name: the server picks one. Cut to PLAYER_NAME_MAX_LENGTH bytes (UTF-8).
export function build_request_authentication(name: string): ByteBuffer | null
{
    // Name bytes go through the input event buffer (at least 64 bytes, see ClientInput).
    const nameBytes = new TextEncoder().encode(name).subarray(0, PLAYER_NAME_MAX_LENGTH);
    new Uint8Array(CLIENT_BACKEND.memory.buffer, CLIENT_BACKEND.client_get_input_event_buffer_offset(), nameBytes.length).set(nameBytes);
    return take_request(CLIENT_BACKEND.client_request_authentication(nameBytes.length));
}

export function build_request_join_lobby(lobby_id: number): ByteBuffer | null { return take_request(CLIENT_BACKEND.client_request_join_lobby(lobby_id)); }
export function build_signal_leave_lobby(): ByteBuffer | null { return take_request(CLIENT_BACKEND.client_signal_leave_lobby()); }

// Test lobbies only. Values are clamped by the server (see the LOBBY_MAX_* constants).
export function build_lobby_admin_start_match(): ByteBuffer | null { return take_request(CLIENT_BACKEND.client_lobby_admin_start_match()); }
export function build_lobby_admin_set_player_count(player_count: number): ByteBuffer | null { return take_request(CLIENT_BACKEND.client_lobby_admin_set_player_count(player_count)); }
export function build_lobby_admin_set_map_size(width_regions: number, height_regions: number): ByteBuffer | null { return take_request(CLIENT_BACKEND.client_lobby_admin_set_map_size(width_regions, height_regions)); }
export function build_lobby_admin_set_seed(random_seed: number): ByteBuffer | null { return take_request(CLIENT_BACKEND.client_lobby_admin_set_seed(random_seed)); }
// Rejected by the server outside [LOBBY_MIN_TICK_RATE, LOBBY_MAX_TICK_RATE].
export function build_lobby_admin_set_tick_rate(tick_rate: number): ByteBuffer | null { return take_request(CLIENT_BACKEND.client_lobby_admin_set_tick_rate(tick_rate)); }

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

// Entity inspection through Game Data Objects (see gdo.ts). The inspection buffer is overwritten by every inspection: read it right away.
// Returns the total byte size written (entity GDO + extra data GDO if the entity type has some), 0 if the entity doesn't exist (anymore), has no GDO yet or it doesn't fit the buffer.
export function entity_inspect(guid: number): number { return CLIENT_BACKEND.client_inspect_entity(guid); }
export function inspected_entity_gdo_get_offset(): number { return CLIENT_BACKEND.client_get_inspected_entity_gdo(); }
export function inspected_entity_extra_data_get_offset(): number { return CLIENT_BACKEND.client_get_inspected_entity_extra_data(); }

// Offset of the GDO of a settlement building type (SETTLEMENT_BUILDING_TABLE row, world_entities.h), refreshed by this call. 0 if there is no such type. Read it with gdo.ts (get_building_definition).
export function building_definition_gdo_get_offset(type: number): number { return CLIENT_BACKEND.client_get_building_definition_gdo(type); }

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
// If tile_mode is true, returns instead the tiles covered by the path's segments, in order, start excluded (for tile-bound uses such as roads).
// If water is true, the path crosses water instead of land (ships).
// The path buffer holds (x, y) ui16 pairs (see web_game_client_query.cpp).
export function query_path(start: { x: number, y: number }, end: { x: number, y: number }, territory_guid: number = 0, tile_mode: boolean = false, water: boolean = false): { x: number, y: number }[]
{
    const count = CLIENT_BACKEND.client_query_path(start.x, start.y, end.x, end.y, territory_guid, (tile_mode ? 1 : 0) | (water ? 2 : 0));
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
    fractional_x: number = 0; // Offset from the tile center, in thousandths of a tile.
    fractional_y: number = 0;
    flags: number = 0; // Caravans: WORLD_ENTITY_CARAVAN_FLAGS.
}

export const CARAVAN_FLAG_IS_SHIP = 1 << 0; // Mirrors WORLD_ENTITY_CARAVAN_FLAGS::IS_SHIP.

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
    renderState.tile_flags_pointer = CLIENT_BACKEND.client_render_state_get_tile_flags();

    // ENTITIES

    const entityStatesOffset: number = CLIENT_BACKEND.client_render_state_get_entities();
    const entityStatesDataView: DataView = new DataView(CLIENT_BACKEND.memory.buffer, entityStatesOffset);

    // Layout of render_entity: entity_type (ui8), owner (ui16), guid (ui32, entity_guid), viewport_location (f32 x/y), fractional_offset (i16 x/y), flags (ui8, caravans: bit 0 = ship).
    // Packed, offsets 0 / 1 / 3 / 7 / 15 / 19, sizeof 20 (static_assert'd in game_client_backend.h).
    const ENTITY_MEM_SIZE: number = 20;

    for (let i = 0; i < renderState.entity_count; i++) {
        renderState.entity_states.push(
            {
                entity_type: entityStatesDataView.getUint8(ENTITY_MEM_SIZE * i),
                owner: entityStatesDataView.getUint16(ENTITY_MEM_SIZE * i + 1, true),
                guid: entityStatesDataView.getUint32(ENTITY_MEM_SIZE * i + 3, true),
                viewport_x: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i + 7, true),
                viewport_y: entityStatesDataView.getFloat32(ENTITY_MEM_SIZE * i + 11, true),
                fractional_x: entityStatesDataView.getInt16(ENTITY_MEM_SIZE * i + 15, true),
                fractional_y: entityStatesDataView.getInt16(ENTITY_MEM_SIZE * i + 17, true),
                flags: entityStatesDataView.getUint8(ENTITY_MEM_SIZE * i + 19),
            });
    }

    return renderState;
}
