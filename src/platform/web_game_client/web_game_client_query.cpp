// Implementation for the entity inspection bridging code between JS frontend and Client backend.
// JS calls client_query_entity(guid): on success the backend has filled ENTITY_VIEW_BUFFER, which JS then reads
// at the offset / size given by the two getters below. On failure the buffer content is stale and must not be read.

#include "game_client/game_client_backend.h"
#include "web_game_client.h"

static entity_full_view ENTITY_VIEW_BUFFER;

WASM_EXPORT ui8* client_get_entity_view_buffer_offset()
{
	return (ui8*)&ENTITY_VIEW_BUFFER;
}

WASM_EXPORT ui32 client_get_entity_view_buffer_size()
{
	return sizeof(entity_full_view);
}

// Path query: JS calls client_query_path, which returns how many waypoints it wrote, then reads that many (x, y) ui16 pairs
// at the offset given by client_get_path_buffer_offset. The start tile is not part of the waypoints, see struct path.
// A plain ui16 array rather than world_location on purpose: see the global constructor note in web_game_client.h.
static constexpr ui16 PATH_BUFFER_CAPACITY = 64; // In waypoints.
static ui16 PATH_BUFFER[PATH_BUFFER_CAPACITY * 2];
static_assert(sizeof(world_location) == 4, "PATH_BUFFER is read as (x, y) ui16 pairs, matching world_location.");

WASM_EXPORT ui8* client_get_path_buffer_offset()
{
	return (ui8*)PATH_BUFFER;
}

// Returns the number of waypoints written to the path buffer (0 if there is no path to follow).
WASM_EXPORT ui32 client_query_path(ui32 start_x, ui32 start_y, ui32 end_x, ui32 end_y)
{
	ASSERT(WEB_CLIENT.backend != nullptr);

	path result = {};
	result.points = (world_location*)PATH_BUFFER;
	result.points_capacity = PATH_BUFFER_CAPACITY;

	game_client_query_path(*WEB_CLIENT.backend, { (ui16)start_x, (ui16)start_y }, { (ui16)end_x, (ui16)end_y }, result);
	return result.count;
}

// Returns false if the entity doesn't exist (anymore) or can't be inspected.
WASM_EXPORT bool client_query_entity(ui32 guid)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_query_entity(*WEB_CLIENT.backend, entity_guid{ guid }, ENTITY_VIEW_BUFFER);
}
