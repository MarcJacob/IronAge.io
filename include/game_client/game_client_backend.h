// Main bridge between Game Client platform code and the platform-independent Game Client Backend.
// The backend is used as a service by the platform / frontend, by sending it input events, network messages to process...
// In return the backend can output abstract presentation data structures allowing the frontend to do its work.

#ifndef GAME_CLIENT_PLATFORM_INCLUDED
#define GAME_CLIENT_PLATFORM_INCLUDED

#include "core.h"

#include "game_common/match/match.h"

#include "game_common/game_messages.h"

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
	ui8 size_viewport; // Square size of the entity in viewport space.
};
static_assert(sizeof(render_entity) == 16, "render_entity layout is read by backend.ts at fixed offsets (stride 16).");

// Render state exposed to the frontend.
struct client_render_state
{
	// Viewport space definition: bottom left corner location + width & height in world tiles.
	// Used to have an idea of the size of world elements compared to viewport.
	struct viewport_state
	{
		// Kept as floats (not rounded to whole tiles): rounding here caused visible per-frame jitter in
		// scaleX/scaleY (canvas pixels per tile) on the frontend as the eased zoom crossed tile boundaries.
		vec2f viewport_bottom_left; // World-space coordinates of the viewport.
		float viewport_width; // Width of the viewport in world tiles.
		float viewport_height; // Height of the viewport in world tiles.
	} viewport;

	match_player_id controlled_player_id; // ID of the player this client is in control of.
	vec2<ui16> world_size; // Dimensions of the match world, in tiles. May move elsewhere once match info grows.

	ui16 entity_count; // Entities currently visible in the viewport only.
	render_entity* entity_states;
};

#pragma pack(pop)

// ENTITY INSPECTION

// Full view of a single entity's current state, for the frontend to inspect it (filled by game_client_query_entity).
// "Fat struct": a core section common to all entity types, then an extra block read according to entity_type.
// Fields that don't apply to a type are zeroed.
// Targets: has_target_entity / has_target_location say whether the corresponding value is meaningful.
// Army: target entity = what it is engaging / following. Caravan: target entity = destination settlement.
// Army / caravan: target location = where it is moving towards. Settlements have neither.
// Read by backend.ts at fixed offsets: keep both in sync (the static_assert below catches size changes only).
#pragma pack(push, 1)

struct entity_full_view
{
	// Core
	ENTITY_TYPE entity_type;
	entity_guid guid;

	ui16 location_x;
	ui16 location_y;
	match_player_id owner; // For caravans, resolved through their origin settlement.

	ui8 has_target_entity;
	entity_guid target_entity;
	ui8 has_target_location;
	ui16 target_location_x;
	ui16 target_location_y;

	ui8 travel_speed; // Tiles per second, 0 for settlements.

	// Type-specific, sized by the largest member (army).
	union
	{
		struct
		{
			ui32 population;
			ui32 local_wealth;
			ui8 tier;
			ui8 trade_attractivity;
			ui8 area_influence;
		} settlement;

		struct
		{
			entity_guid origin_settlement;
		} caravan;

		struct
		{
			ui16 levies;
			ui16 archers;
			ui32 men_at_arms;
			ui16 horsemen;
			ui16 knights;
			entity_guid arrows_target; // 0 if none.
		} army;
	} extra;
};
static_assert(sizeof(entity_full_view) == 38, "entity_full_view layout is read by backend.ts at fixed offsets (size 38).");

#pragma pack(pop)

// INPUT SYSTEM

// Identifies which payload struct below a generalized input event's bytes should be read as.
enum class INPUT_EVENT_TYPE : ui16
{
	VIEWPORT_CONTROL,
	SET_TARGET_LOC,
	FOUND_SETTLEMENT,
	SPAWN_ARMY,
	ATTACK_TARGET,
	SPAWN_CARAVAN,
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

// Orders an entity to move to the location under the cursor.
struct input_event_payload_set_target_loc
{
	entity_guid entity; // Entity to order.
	i32 x, y; // Cursor position in viewport space: whole world tiles from the viewport's bottom-left corner.
};
INPUT_EVENT_PAYLOAD_SIZE_GUARD(input_event_payload_set_target_loc);

// Turns an army into a settlement at its location.
struct input_event_payload_found_settlement
{
	entity_guid army; // Army to found the settlement with.
};
INPUT_EVENT_PAYLOAD_SIZE_GUARD(input_event_payload_found_settlement);

// Has a settlement spawn an army from its population.
struct input_event_payload_spawn_army
{
	entity_guid settlement; // Settlement to spawn the army from.
};
INPUT_EVENT_PAYLOAD_SIZE_GUARD(input_event_payload_spawn_army);

// Has a settlement spawn a caravan from its local wealth.
struct input_event_payload_spawn_caravan
{
	entity_guid settlement; // Settlement to spawn the caravan from.
};
INPUT_EVENT_PAYLOAD_SIZE_GUARD(input_event_payload_spawn_caravan);

// Orders an army to chase and attack another entity.
struct input_event_payload_attack_target
{
	entity_guid attacker_entity; // Army to order.
	entity_guid target_entity; // Entity to attack.
};
INPUT_EVENT_PAYLOAD_SIZE_GUARD(input_event_payload_attack_target);

#pragma pack(pop)

#undef INPUT_EVENT_PAYLOAD_SIZE_GUARD

// BACKEND INTERFACE DEFINITION.

struct game_client;
struct game_match_start_params;

// INPUT

// Interprets payload_bytes (at most INPUT_EVENT_MAX_PAYLOAD_SIZE bytes, owned by the caller) as the payload struct
// matching code, and applies it. Returns false if code isn't recognized (frontend out of sync with this header).
bool game_client_process_input_event(game_client& backend, INPUT_EVENT_TYPE code, ui8 payload_bytes[INPUT_EVENT_MAX_PAYLOAD_SIZE]);

// RENDERING 

// Gets current render state for presentation.
client_render_state* game_client_get_render_state(game_client& backend);

// ENTITY INSPECTION

// Fills out_view with the current state of the entity if it exists in the local match and is of a supported type.
// Returns false (leaving out_view untouched) if there is no local match, the GUID is invalid / stale, or its type is unsupported.
bool game_client_query_entity(game_client& backend, entity_guid entity, entity_full_view& out_view);

// MATCH

// If the backend has an ongoing local match, retrieves its start parameters.
game_match_start_params* game_client_get_local_match_params(game_client& backend);

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

// MAIN LIFECYCLE FUNCTIONS.

// Initializes a client backend in the specified memory.
// From there the backend assumes it has full control of the passed memory arena.
// Returns a filled-in interface structure for use by the platform / front.
game_client* game_client_init(mem_arena& backend_memory);

// Ticks the backend logic by the provided real-time seconds.
void game_client_tick(game_client& backend, float delta_time);

#endif // GAME_CLIENT_PLATFORM_INCLUDED