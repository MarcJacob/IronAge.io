// Implementation for the query bridging code between JS frontend and Client backend: entity inspection and paths.

#include "game_client/game_client_backend.h"
#include "web_game_client.h"

// Entity GDO inspection: JS calls client_query_entity_gdo(guid): if it returns a size, the backend has written the entity as a Game Data Object
// at the offset given by client_get_entity_gdo_buffer_offset, which JS reads (and copies what it needs) before the next call.
// Separate from the command GDO buffer. Plain bytes on purpose, see the global constructor note in web_game_client.h.
static constexpr ui32 ENTITY_GDO_BUFFER_SIZE = 2048; // Must fit the largest entity GDO (the army's, with its 64 warband properties: about 1.1 KiB on wasm32).
static ui8 ENTITY_GDO_BUFFER[ENTITY_GDO_BUFFER_SIZE];

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
