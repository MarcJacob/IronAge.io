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

// Returns false if the entity doesn't exist (anymore) or can't be inspected.
WASM_EXPORT bool client_query_entity(ui32 guid)
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return game_client_query_entity(*WEB_CLIENT.backend, entity_guid{ guid }, ENTITY_VIEW_BUFFER);
}
