// Settlement building types: type-specific event functions (see SETTLEMENT_BUILDING_TABLE in world_entities.h).

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world_entities.h"

// BARRACKS

void settlement_building_barracks_on_added(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    ui16 settlementIndex = settlement.get_index();
    world.entities->settlements.populations[settlementIndex].troop_max.men_at_arms += GAME_PARAMETERS.barracks_maa_max_increase;
    // @TODO(Marc): Add enumeration for troop types.
    world.entities->settlements.populations[settlementIndex].troop_training[1].amount += 1;
}

void settlement_building_barracks_on_removed(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    ui16 settlementIndex = settlement.get_index();
    world.entities->settlements.populations[settlementIndex].troop_max.men_at_arms -= GAME_PARAMETERS.barracks_maa_max_increase;
    // @TODO(Marc): Add enumeration for troop types.
    world.entities->settlements.populations[settlementIndex].troop_training[1].amount -= 1;
}

// ...
