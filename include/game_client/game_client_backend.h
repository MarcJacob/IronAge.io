// Main bridge between Game Client platform code and the platform-independent Game Client Backend.
// The backend is used as a service by the platform / frontend, by sending it input events, network messages to process...
// In return the backend can output abstract presentation data structures allowing the frontend to do its work.

#ifndef GAME_CLIENT_PLATFORM_INCLUDED
#define GAME_CLIENT_PLATFORM_INCLUDED

#include "core.h"

#include "game_common/match/match.h"

#include "game_common/game_messages.h"
#include "game_common/match/world/world_terrain.h"
#include "game_common/match/world/world_pathing.h"

// Structure definitions for resources sent from the client backend to frontend.

// Packed: made for external exposition, need to avoid padding.
#pragma pack(push, 1)

// Entity's location and target location in viewport space.
struct render_entity
{
	ENTITY_TYPE entity_type; // Type of entity being rendered.
	match_player_id owner; // ID of owner player if any.
	entity_guid guid; // GUID of the entity, so the frontend can name what it hit-tested.

	vec2f viewport_location; // Location of center of entity.
	vec2<i16> fractional_offset; // Offset of the entity from the center of its tile, in thousandths of a tile (same axes as the world). Zero for entities without one.
	ui8 flags; // Caravans: WORLD_ENTITY_CARAVAN_FLAGS (bit 0 = ship). Zero for other entities.
	ui32 significance; // How much the entity matters, for the platform's rendering. Always 0 from the backend: each platform fills it its own way.
};
static_assert(sizeof(render_entity) == 24, "render_entity struct size changed. Adapt TS reading code, then this assertion.");

enum class RENDER_TILE_FLAGS : ui8
{
    IS_BORDER = 1 << 0,
    IS_ROAD = 1 << 1,
    IS_OWNED = 1 << 2, // Tile has an owner settlement. Owner bitmaps are only meaningful when set.
    // ...
};

// Render state exposed to the frontend.
struct client_render_state
{
	// Viewport space definition: bottom left corner location + width & height in world tiles.
	// Used to have an idea of the size of world elements compared to viewport.
	struct viewport_state
	{
		// Kept as floats (not rounded to whole tiles): rounding here caused visible per-frame jitter in
		// scaleX/scaleY (canvas pixels per tile) on the frontend as the eased zoom crossed tile boundaries.
		vec2f bottom_left; // World-space coordinates of the viewport.
        vec2f dimensions; // Width & Height of the viewport in world tiles.
	} viewport;

	vec2<ui16> world_size; // Dimensions of the match world, in tiles. May move elsewhere once match info grows.

    struct
    {
        mem_arena memory; // Memory within which the frame data is rebuilt every frame.

        ui16 entity_count; // Entities currently visible in the viewport only.
        render_entity* entity_states;
    } frame;

    struct
    {
        // One TERRAIN_TILE_TYPE value per world tile. Row major.
        TERRAIN_TILE_TYPE* terrain_tiles;
        // One ui16 per world tile: slot index of the settlement owning the tile. Only valid if IS_OWNED is set. Row major.
        // Settlements whose influence bounds touch the viewport are always in the frame's render entities, so the
        // frontend can resolve the owning player through them.
        ui16* owner_settlement_tiles_bitmap;
        // One RENDER_TILE_FLAGS bit set per world tile. Row major.
        ui8* tile_flags_bitmap;
    } tiles;
};
static_assert(sizeof(TERRAIN_TILE_TYPE) == 1, "terrain_tiles holds one byte per tile (read by backend.ts).");

#pragma pack(pop)

// INPUT SYSTEM

// Identifies which payload struct below a generalized input event's bytes should be read as.
enum class INPUT_EVENT_TYPE : ui16
{
	VIEWPORT_CONTROL,
};

// Max size in bytes of any one input event's payload.
static constexpr ui32 INPUT_EVENT_MAX_PAYLOAD_SIZE = 64;

#define INPUT_EVENT_PAYLOAD_SIZE_GUARD(payload_struct) \
static_assert(sizeof(payload_struct) <= INPUT_EVENT_MAX_PAYLOAD_SIZE, "Input event payload struct \"" #payload_struct "\" is too large.");

#pragma pack(push, 1)

struct input_event_payload_viewport_control
{
	float view_rect_min_x, view_rect_min_y;
	float view_rect_max_x, view_rect_max_y;
};
INPUT_EVENT_PAYLOAD_SIZE_GUARD(input_event_payload_viewport_control);

#pragma pack(pop)

#undef INPUT_EVENT_PAYLOAD_SIZE_GUARD

// CLIENT STATE

// Where the client stands. Mostly follows the server connection flow: connect -> authenticate -> lobby list -> join -> lobby -> match.
// The host reports nothing but the connection itself: the backend moves through the rest on sent requests and received messages.
enum class CLIENT_STATE : ui8
{
	CONNECTING,		// Not authenticated yet: waiting for the host to open the connection and send REQUEST_AUTHENTICATION.
	AUTHENTICATING,	// REQUEST_AUTHENTICATION sent, waiting for AUTHENTICATED.
	LOBBY_LIST,		// Authenticated, in no lobby. The lobby list is refreshed by the server.
	JOINING_LOBBY,	// REQUEST_JOIN_LOBBY sent, waiting for JOIN_LOBBY_RESPONSE.
	IN_LOBBY,		// In a lobby, receiving its state.
	IN_MATCH,		// The lobby's match started.
};

// BACKEND INTERFACE DEFINITION.

struct game_client;
struct game_match_start_params;

// INPUT

// Interprets payload_bytes (at most INPUT_EVENT_MAX_PAYLOAD_SIZE bytes, owned by the caller) as the payload struct
// matching code, and applies it. Returns false if code isn't recognized (frontend out of sync with this header).
bool game_client_process_input_event(game_client& backend, INPUT_EVENT_TYPE code, ui8 payload_bytes[INPUT_EVENT_MAX_PAYLOAD_SIZE]);

// Queues a match command of the given type (a MATCH_COMMAND_TYPE value), whose payload bytes (command payload layout, see commands.h,
// at most INPUT_EVENT_MAX_PAYLOAD_SIZE bytes, owned by the caller) are copied as is. The server validates it.
// Returns false if the type is unknown or its payload doesn't fit in an input event payload (frontend out of sync with this header).
// With no local match or a full queue the command is silently dropped.
bool game_client_input_push_match_command(game_client& backend, ui8 match_command_type, const ui8* payload_bytes);

// RENDERING

// Gets current render state for presentation.
client_render_state* game_client_get_render_state(game_client& backend);

// ENTITY INSPECTION

// Writes the current state of the entity as a Game Data Object (see game_data_object.h) into out_buffer, if it fits in buffer_size bytes.
// Returns the byte size written, or 0 if there is no local match, the GUID is invalid / stale, its type has no GDO defined yet, or the GDO does not fit.
ui32 game_client_query_entity_gdo(game_client& backend, entity_guid entity, ui8* out_buffer, ui32 buffer_size);

// WORLD STATE QUERIES

// Age of the local match's world in ticks. 0 if there is no local match.
ui64 game_client_query_world_age(game_client& backend);

// State of the controlled player in the local match's world. Zeroed if there is no local match or the player is not in it.
world_player_state game_client_query_controlled_player_state(game_client& backend);

// PATH QUERY

// Computes a path of the given mode between two tiles of the local match's world into out_path, whose points and points_capacity the caller must set.
// Leaves out_path empty (count 0) if there is no local match or a location is outside the world. See world_path_find for the rest.
// If within_settlement_territory is a settlement, the path is limited to its territory.
void game_client_query_path(game_client& backend, world_location start, world_location end, path& out_path,
	entity_guid within_settlement_territory = INVALID_ENTITY_GUID, PATH_MODE mode = PATH_MODE_LAND);

// COMMAND VALIDITY QUERY

// Whether a command of the given type (a MATCH_COMMAND_TYPE value) with the given payload bytes (command payload layout, see commands.h)
// would currently be valid for the controlled player. Same check the server applies. False if there is no local match or the type is unknown.
// If out_feedback is given it is filled with what the check found out (COMMAND_ERROR_UNSPECIFIED alone if the check could not run).
bool game_client_query_command_valid(game_client& backend, ui8 command_type, const ui8* payload_bytes, struct match_command_validity_feedback* out_feedback = nullptr);

// MATCH

// If the backend has an ongoing local match, retrieves its start parameters.
game_match_start_params* game_client_get_local_match_params(game_client& backend);

// Name of a player of the current match, as received at match start. Empty if the ID is out of range (INVALID_MATCH_PLAYER_ID included) or no names were received.
// The view points into the backend: valid until the next match starts.
ia_string_view game_client_get_player_name(game_client& backend, match_player_id player_id);

// MESSAGING

// Processes the game message bytes present in the backend reception buffer and returns the processed message type (or INVALID if none).
GAME_MESSAGE_TYPE game_client_process_game_message(game_client& backend, ui32 message_size);

// Has the backend output its queued input actions in the form of a CLIENT_TICK game message in its messaging send buffer.
void game_client_output_client_tick_message(game_client& backend);

// Returns pointer to the backend game message reception buffer.
ui8* game_client_get_net_msg_buffer(game_client& backend);

// Returns size of the backend game message reception buffer.
ui32 game_client_get_net_msg_buffer_size(game_client& backend);

// Returns pointer to the backend game message output buffer.
ui8* game_client_get_net_msg_output(game_client& backend);

// Returns size of the backend game message output buffer.
ui32 game_client_get_net_msg_output_size(game_client& backend);

// CLIENT STATE REQUESTS
// Each builds one message in the request buffer (game_client_get_net_request_buffer) for the host to send right away,
// moves the client state accordingly and returns the message size. Returns 0 (nothing to send) when the current state does not allow it.

// From CONNECTING. Names longer than PLAYER_NAME_MAX_LENGTH are cut. Empty: the server picks one.
ui32 game_client_request_authentication(game_client& backend, const ia_string_view& name);
// From LOBBY_LIST. Clears the last kick notice.
ui32 game_client_request_join_lobby(game_client& backend, ui8 lobby_id);
// From IN_LOBBY, back to LOBBY_LIST.
ui32 game_client_signal_leave_lobby(game_client& backend);
// From IN_LOBBY, in a test lobby only. Values are clamped by the server.
ui32 game_client_lobby_admin_start_match(game_client& backend);
ui32 game_client_lobby_admin_set_player_count(game_client& backend, ui16 player_count);
ui32 game_client_lobby_admin_set_map_size(game_client& backend, ui16 width_regions, ui16 height_regions);
ui32 game_client_lobby_admin_set_seed(game_client& backend, ui32 random_seed);
ui32 game_client_lobby_admin_set_tick_rate(game_client& backend, ui8 tick_rate);

// Returns pointer to the request buffer filled by the functions above.
ui8* game_client_get_net_request_buffer(game_client& backend);

// MAIN LIFECYCLE FUNCTIONS.

// Initializes a client backend in the specified memory.
// From there the backend assumes it has full control of the passed memory arena.
// Returns a filled-in interface structure for use by the platform / front.
game_client* game_client_init(mem_arena& backend_memory);

// Ticks the backend logic by the provided real-time seconds.
void game_client_tick(game_client& backend, float delta_time);

#endif // GAME_CLIENT_PLATFORM_INCLUDED
