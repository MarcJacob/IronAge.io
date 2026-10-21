// Implementation for the query bridging code between JS frontend and Client backend: entity inspection, game parameters, world state values and paths.

#include "game_client/game_client_backend.h"
#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world_entities.h"
#include "web_game_client.h"

// Entity GDO inspection: JS calls client_query_entity_gdo(guid): if it returns a size, the backend has written the entity as a Game Data Object
// at the offset given by client_get_entity_gdo_buffer_offset, which JS reads (and copies what it needs) before the next call.
// Separate from the command GDO buffer. Plain bytes on purpose, see the global constructor note in web_game_client.h.
static constexpr ui32 ENTITY_GDO_BUFFER_SIZE = 1024; // Must fit the largest entity GDO (checked below: one static_assert per entity GDO).
static ui8 ENTITY_GDO_BUFFER[ENTITY_GDO_BUFFER_SIZE];
static_assert(sizeof(gdo_world_entity_settlement) <= ENTITY_GDO_BUFFER_SIZE, "Settlement GDO does not fit ENTITY_GDO_BUFFER_SIZE.");
static_assert(sizeof(gdo_world_entity_army) <= ENTITY_GDO_BUFFER_SIZE, "Army GDO does not fit ENTITY_GDO_BUFFER_SIZE.");
static_assert(sizeof(gdo_world_entity_worker) <= ENTITY_GDO_BUFFER_SIZE, "Builder GDO does not fit ENTITY_GDO_BUFFER_SIZE.");
static_assert(sizeof(gdo_world_entity_battle) <= ENTITY_GDO_BUFFER_SIZE, "Battle GDO does not fit ENTITY_GDO_BUFFER_SIZE.");

WASM_EXPORT ui8* client_get_entity_gdo_buffer_offset()
{
	return ENTITY_GDO_BUFFER;
}

// Returns the byte size of the GDO written, or 0 if the entity doesn't exist (anymore), its type has no GDO yet, or its GDO doesn't fit the buffer.
WASM_EXPORT ui32 client_query_entity_gdo(ui32 guid)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_query_entity_gdo(*WEB_CLIENT.backend, entity_guid{ guid }, ENTITY_GDO_BUFFER, ENTITY_GDO_BUFFER_SIZE);
}

// Global game parameters: the constant GAME_PARAMETERS as a Game Data Object, in its own buffer. JS calls client_get_game_parameters_gdo_offset (which fills it)
// and reads the properties by name. Plain bytes on purpose, see the global constructor note in web_game_client.h.
static ui8 GAME_PARAMETERS_GDO_BUFFER[sizeof(gdo_game_parameters)];

WASM_EXPORT ui8* client_get_game_parameters_gdo_offset()
{
	gdo_game_parameters gdo; // Built on the stack then copied, as for the command GDO.
	ia_memcpy(&gdo._data, &GAME_PARAMETERS, sizeof(game_parameters));
	ia_memcpy(GAME_PARAMETERS_GDO_BUFFER, &gdo, sizeof(gdo_game_parameters));
	return GAME_PARAMETERS_GDO_BUFFER;
}

// World age in ticks as a double (a wasm i64 would reach JS as a BigInt): exact up to 2^53 ticks.
WASM_EXPORT double client_query_world_age()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return (double)game_client_query_world_age(*WEB_CLIENT.backend);
}

// State of the controlled player (world_player_state) as a Game Data Object, in its own buffer. JS calls client_get_player_state_gdo_offset (which fills it:
// the values change, so read it again every time) and reads the properties by name. Zeroed without a match. Plain bytes on purpose, see the global constructor note in web_game_client.h.
static ui8 PLAYER_STATE_GDO_BUFFER[sizeof(gdo_world_player_state)];

WASM_EXPORT ui8* client_get_player_state_gdo_offset()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	world_player_state state = game_client_query_controlled_player_state(*WEB_CLIENT.backend);
	gdo_world_player_state gdo; // Built on the stack then copied, as for the game parameters.
	ia_memcpy(&gdo._data, &state, sizeof(world_player_state));
	ia_memcpy(PLAYER_STATE_GDO_BUFFER, &gdo, sizeof(gdo_world_player_state));
	return PLAYER_STATE_GDO_BUFFER;
}

// Path query: JS calls client_query_path, which returns how many waypoints it wrote, then reads that many (x, y) ui16 pairs
// at the offset given by client_get_path_buffer_offset. The start tile is not part of the waypoints, see struct path.
// A plain ui16 array rather than world_location on purpose: see the global constructor note in web_game_client.h.
static constexpr ui16 PATH_BUFFER_CAPACITY = 256; // In waypoints.
static ui16 PATH_BUFFER[PATH_BUFFER_CAPACITY * 2];
static_assert(sizeof(world_location) == 4, "PATH_BUFFER is read as (x, y) ui16 pairs, matching world_location.");

WASM_EXPORT ui8* client_get_path_buffer_offset()
{
	return (ui8*)PATH_BUFFER;
}

// Returns the number of waypoints written to the path buffer (0 if there is no path to follow).
// territory_guid: a settlement's guid to limit the path to its territory, or 0 for no limit.
WASM_EXPORT ui32 client_query_path(ui32 start_x, ui32 start_y, ui32 end_x, ui32 end_y, ui32 territory_guid)
{
	ASSERT(WEB_CLIENT.backend != nullptr);

	path result = {};
	result.points = (world_location*)PATH_BUFFER;
	result.points_capacity = PATH_BUFFER_CAPACITY;

	game_client_query_path(*WEB_CLIENT.backend, { (ui16)start_x, (ui16)start_y }, { (ui16)end_x, (ui16)end_y }, result, entity_guid{ territory_guid });
	return result.count;
}
