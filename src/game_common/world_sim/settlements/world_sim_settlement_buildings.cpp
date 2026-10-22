// Settlement building types: type-specific event functions (see SETTLEMENT_BUILDING_TABLE in world_entities.h).

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world.h"
#include "game_common/match/world/world_entities.h"

// BARRACKS

void settlement_building_barracks_on_added(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    ui16 settlementIndex = settlement.get_index();
    world.entities->settlements.populations[settlementIndex].troop_max.men_at_arms += GAME_PARAMETERS.barracks_maa_max_increase;
    // @TODO(Marc): Add enumeration for troop types.
    world.entities->settlements.populations[settlementIndex].troop_training[1].amount += 2;
}

void settlement_building_barracks_on_removed(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    ui16 settlementIndex = settlement.get_index();
    world.entities->settlements.populations[settlementIndex].troop_max.men_at_arms -= GAME_PARAMETERS.barracks_maa_max_increase;
    // @TODO(Marc): Add enumeration for troop types.
    world.entities->settlements.populations[settlementIndex].troop_training[1].amount -= 2;
}

// TRADE POSTS

// Trade pool cap added by each trade post. Placeholder, may become a building parameter later.
static constexpr ui32 TRADE_POST_POOL_CAPACITY = 200;

void settlement_building_tradepost_on_added(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    ui16 settlementIndex = settlement.get_index();

    world.entities->settlements.trade[settlementIndex].caravan_count_max++;
    world.entities->settlements.trade[settlementIndex].trade_pool_max += TRADE_POST_POOL_CAPACITY;
}

void settlement_building_tradepost_on_removed(match_world_state& world, entity_guid settlement)
{
    ASSERT(world_entity_is_valid(world, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);

    ui16 settlementIndex = settlement.get_index();

    world.entities->settlements.trade[settlementIndex].caravan_count_max--;
    world.entities->settlements.trade[settlementIndex].trade_pool_max -= TRADE_POST_POOL_CAPACITY; // The pool itself is kept.
}
// ...
