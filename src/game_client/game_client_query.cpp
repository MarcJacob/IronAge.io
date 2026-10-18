// Implementation for client backend entity inspection: match entity state -> packed full view for the frontend.

#include "core.h"

#include "game_client.h"

static void fill_movement_view(entity_full_view& view, const world_entity_movement& movement)
{
	view.travel_speed = movement.travel_speed;
	view.has_target_location = movement.destination.x != INVALID_WORLD_LOCATION.x || movement.destination.y != INVALID_WORLD_LOCATION.y;
	view.target_location_x = movement.destination.x;
	view.target_location_y = movement.destination.y;
}

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

void game_client_query_path(game_client& backend, world_location start, world_location end, path& out_path, entity_guid within_settlement_territory)
{
	out_path.count = 0;
	out_path.length = 0;
	out_path.partial = true;

	if (backend.local_match == nullptr) return;
	const world_terrain& terrain = backend.local_match->world->terrain;
	if (start.x >= terrain.size_tiles.x || start.y >= terrain.size_tiles.y || end.x >= terrain.size_tiles.x || end.y >= terrain.size_tiles.y) return;

	path_params params = {};
	params.passable_terrain = PATH_TERRAIN_LAND;
	params.within_settlement_territory = within_settlement_territory;
	backend.path_scratch.clear();
	world_path_find(terrain, start, end, params, backend.path_scratch, out_path);
}

bool game_client_query_entity(game_client& backend, entity_guid entity, entity_full_view& out_view)
{
	if (backend.local_match == nullptr) return false;
	const match_world_state& world = *backend.local_match->world;

	// Dispatch on the GUID's type to the matching match query, which also validates the GUID.
	switch (entity.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
	{
		world_entity_settlements::single settlement;
		if (!query_entity_state_settlement(world, entity, settlement)) return false;

		out_view = {};
		out_view.location_x = settlement.location.x;
		out_view.location_y = settlement.location.y;
		out_view.owner = settlement.owner;
		out_view.extra.settlement.population = settlement.population;
		out_view.extra.settlement.local_wealth = settlement.local_wealth;
		out_view.extra.settlement.tier = settlement.tier;
		out_view.extra.settlement.trade_attractivity = settlement.trade_attaction_index;
		break;
	}
	case ENTITY_TYPE::CARAVAN:
	{
		world_entity_caravans::single caravan;
		if (!query_entity_state_caravan(world, entity, caravan)) return false;

		out_view = {};
		out_view.location_x = caravan.location.x;
		out_view.location_y = caravan.location.y;

		// Owner is that of the origin settlement, as in the render state.
		world_entity_settlements::single origin;
		out_view.owner = query_entity_state_settlement(world, caravan.origin_settlement, origin) ? origin.owner : INVALID_MATCH_PLAYER_ID;

		out_view.has_target_entity = caravan.dest_settlement.is_valid();
		out_view.target_entity = caravan.dest_settlement;
		fill_movement_view(out_view, caravan.movement);
		out_view.extra.caravan.origin_settlement = caravan.origin_settlement;
		break;
	}
	case ENTITY_TYPE::ARMY:
	{
		world_entity_armies::single army;
		if (!query_entity_state_army(world, entity, army)) return false;

		out_view = {};
		out_view.location_x = army.location.x;
		out_view.location_y = army.location.y;
		out_view.owner = army.owner;
		out_view.has_target_entity = army.action_target.is_valid();
		out_view.target_entity = army.action_target;
		fill_movement_view(out_view, army.movement);
		out_view.extra.army.levies = army.composition.levies;
		out_view.extra.army.archers = army.composition.archers;
		out_view.extra.army.men_at_arms = army.composition.men_at_arms;
		out_view.extra.army.horsemen = army.composition.horsemen;
		out_view.extra.army.knights = army.composition.knights;
		out_view.extra.army.arrows_target = army.arrows_target;
		break;
	}
	case ENTITY_TYPE::BATTLE:
	{
		if (!world_entity_is_valid(world, entity)) return false;
		const world_entity_battles::state& battle = world.entities.battles.states[entity.get_index()];

		out_view = {};
		out_view.location_x = battle.location.x;
		out_view.location_y = battle.location.y;
		out_view.owner = INVALID_MATCH_PLAYER_ID;
		for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++)
		{
			out_view.extra.battle.attacker_men += battle.men[0][unitType];
			out_view.extra.battle.defender_men += battle.men[1][unitType];
		}
		break;
	}
	default:
		return false;
	}

	out_view.entity_type = entity.get_type();
	out_view.guid = entity;
	return true;
}
