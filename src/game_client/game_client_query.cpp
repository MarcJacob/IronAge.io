// Implementation for client backend queries: command validity, paths, and entity inspection (match entity state -> Game Data Object).

#include "core.h"

#include "game_client.h"
#include "game_common/match/world/world_state.h"

bool game_client_query_command_valid(game_client& backend, ui8 command_type, const ui8* payload_bytes, match_command_validity_feedback* out_feedback)
{
	// Invalid with no detail until the check says more.
	if (out_feedback != nullptr)
	{
		*out_feedback = {};
		out_feedback->error_flags = COMMAND_ERROR_UNSPECIFIED;
	}

	if (backend.local_match == nullptr) return false;
	if (command_type >= (ui8)MATCH_COMMAND_TYPE::TYPE_COUNT) return false;

	MATCH_COMMAND_TYPE type = (MATCH_COMMAND_TYPE)command_type;
	ui8 payloadSize = get_command_data_size(type);
	if (payloadSize > INPUT_EVENT_MAX_PAYLOAD_SIZE) return false;

	// Rebuild the command as it would sit in a command buffer: header, then payload.
	ui8 commandBuffer[sizeof(match_command_header) + INPUT_EVENT_MAX_PAYLOAD_SIZE];
	match_command_header& header = *(match_command_header*)commandBuffer;
	header.type = type;
#ifdef MATCH_COMMAND_DEBUG
	header._data_size = payloadSize;
#endif
	ia_memcpy(header.command_data, payload_bytes, payloadSize);

	return match_command_validity_check(*backend.local_match->world, backend.controlled_player_id, header, out_feedback);
}

void game_client_query_path(game_client& backend, world_location start, world_location end, path& out_path, entity_guid within_settlement_territory, PATH_MODE mode)
{
	out_path.count = 0;
	out_path.partial = true;

	if (backend.local_match == nullptr) return;
	const world_terrain& terrain = *backend.local_match->world->terrain;
	if (start.x >= terrain.size_tiles.x || start.y >= terrain.size_tiles.y || end.x >= terrain.size_tiles.x || end.y >= terrain.size_tiles.y) return;

	path_params params = {};
	params.start = start;
	params.end = end;
	params.mode = mode;
	params.within_settlement_territory = within_settlement_territory;
	backend.path_scratch.clear();
	world_path_find(terrain, params, backend.path_scratch, out_path);
}

ia_string_view game_client_get_player_name(game_client& backend, match_player_id player_id)
{
	if (player_id >= backend.state.match_player_name_count) return ia_string_view();
	return (ia_string_view)backend.state.match_player_names[player_id];
}

ui64 game_client_query_world_age(game_client& backend)
{
	if (backend.local_match == nullptr) return 0;
	return backend.local_match->world->age;
}

world_player_state game_client_query_controlled_player_state(game_client& backend)
{
	if (backend.local_match == nullptr) return {};
	const match_world_state& world = *backend.local_match->world;
	if (backend.controlled_player_id >= world.player_count) return {};
	return world.player_states[backend.controlled_player_id];
}

// Fills a GDO of the given type from the entity's single state (read by the given query function) and copies it into the buffer.
// Returns the size written, or 0 if the query failed or the GDO does not fit.
template<typename GdoType>
static ui32 fill_entity_gdo(const match_world_state& world, entity_guid entity,
	bool (*query_state)(const match_world_state&, entity_guid, decltype(GdoType::_data)&), ui8* out_buffer, ui32 buffer_size)
{
	if (sizeof(GdoType) > buffer_size) return 0;

	// Queried into a local (naturally aligned) then copied: the GDO is packed, so its copy of the struct can't be bound to a reference.
	decltype(GdoType::_data) state;
	if (!query_state(world, entity, state)) return 0;

	GdoType gdo;
	ia_memcpy(&gdo._data, &state, sizeof(state));
	ia_memcpy(out_buffer, &gdo, sizeof(GdoType));
	return sizeof(GdoType);
}

ui32 game_client_query_entity_gdo(game_client& backend, entity_guid entity, ui8* out_buffer, ui32 buffer_size)
{
	if (backend.local_match == nullptr) return 0;
	const match_world_state& world = *backend.local_match->world;

	// One case per entity type with a GDO: its GDO definition (world.h) and single state query are all it needs.
	switch (entity.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT: return fill_entity_gdo<gdo_world_entity_settlement>(world, entity, world_entity_query_settlement, out_buffer, buffer_size);
	case ENTITY_TYPE::ARMY: return fill_entity_gdo<gdo_world_entity_army>(world, entity, world_entity_query_army, out_buffer, buffer_size);
	case ENTITY_TYPE::WORKER: return fill_entity_gdo<gdo_world_entity_worker>(world, entity, world_entity_query_worker, out_buffer, buffer_size);
	case ENTITY_TYPE::CARAVAN: return fill_entity_gdo<gdo_world_entity_caravan>(world, entity, world_entity_query_caravan, out_buffer, buffer_size);
	case ENTITY_TYPE::BATTLE: return fill_entity_gdo<gdo_world_entity_battle>(world, entity, world_entity_query_battle, out_buffer, buffer_size);
	default: return 0;
	}
}
