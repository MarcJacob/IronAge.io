// Implementation for client backend entity inspection: match entity state -> packed full view for the frontend.

#include "core.h"

#include "game_client.h"

static void fill_movement_view(entity_full_view& view, const world_entity_movement& movement)
{
	view.travel_speed = movement.travel_speed;
	view.has_target_location = movement.move_target.x != INVALID_WORLD_LOCATION.x || movement.move_target.y != INVALID_WORLD_LOCATION.y;
	view.target_location_x = movement.move_target.x;
	view.target_location_y = movement.move_target.y;
}

bool game_client_query_entity(game_client& backend, entity_guid entity, entity_full_view& out_view)
{
	if (backend.local_match == nullptr) return false;
	const game_match& match = *backend.local_match;

	// Dispatch on the GUID's type to the matching match query, which also validates the GUID.
	switch (entity.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
	{
		world_entity_settlements::single settlement;
		if (!query_entity_state_settlement(match, entity, settlement)) return false;

		out_view = {};
		out_view.location_x = settlement.location.x;
		out_view.location_y = settlement.location.y;
		out_view.owner = settlement.owner;
		out_view.extra.settlement.population = settlement.population;
		out_view.extra.settlement.local_wealth = settlement.local_wealth;
		out_view.extra.settlement.tier = settlement.tier;
		out_view.extra.settlement.trade_attractivity = settlement.trade_attractivity;
		out_view.extra.settlement.area_influence = settlement.area_influence;
		break;
	}
	case ENTITY_TYPE::CARAVAN:
	{
		world_entity_caravans::single caravan;
		if (!query_entity_state_caravan(match, entity, caravan)) return false;

		out_view = {};
		out_view.location_x = caravan.location.x;
		out_view.location_y = caravan.location.y;

		// Owner is that of the origin settlement, as in the render state.
		world_entity_settlements::single origin;
		out_view.owner = query_entity_state_settlement(match, caravan.origin_settlement, origin) ? origin.owner : INVALID_MATCH_PLAYER_ID;

		out_view.has_target_entity = caravan.dest_settlement.is_valid();
		out_view.target_entity = caravan.dest_settlement;
		fill_movement_view(out_view, caravan.movement);
		out_view.extra.caravan.origin_settlement = caravan.origin_settlement;
		break;
	}
	case ENTITY_TYPE::ARMY:
	{
		world_entity_armies::single army;
		if (!query_entity_state_army(match, entity, army)) return false;

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
	default:
		return false;
	}

	out_view.entity_type = entity.get_type();
	out_view.guid = entity;
	return true;
}
