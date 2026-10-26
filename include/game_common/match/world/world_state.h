// Definition for match world state elements.

#ifndef WORLD_STATE_INCLUDED
#define WORLD_STATE_INCLUDED

#include "core.h"
#include "world.h"

#include "entities/world_entity_settlements.h"
#include "entities/world_entity_workers.h"
#include "entities/world_entity_caravans.h"
#include "entities/world_entity_armies.h"

// State of one player, indexed by match_player_id in the world state.
struct world_player_state
{
    ui32 treasury; // Wealth held by the player. Deposits: daily tax. Spending: worker pay, local soldier pay, army supplies, army raising.
    ui32 income; // Last day's deposits into the treasury.
    ui32 expenses; // Last day's spending from the treasury. Reset by the daily economy tick, so spenders must run after it.
    
    ui16 settlement_count;
    ui32 total_population;
    ui32 total_troops;
};

// Not an entity: the GDO covers the structure directly. Property names are the field names.
GAME_DATA_OBJECT_DEF_BEGIN(world_player_state)
    GAME_DATA_PROPERTY(treasury, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(income, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(expenses, GAME_DATA_PROP_TYPE::UI32)

    GAME_DATA_PROPERTY(settlement_count, GAME_DATA_PROP_TYPE::UI16)
    GAME_DATA_PROPERTY(total_population, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY(total_troops, GAME_DATA_PROP_TYPE::UI32)
GAME_DATA_OBJECT_DEF_END()

// Holds all entity data for a world state.
// Currently is all pre-allocated for pre-estimated max possible usage.
struct world_entities
{
    world_entity_settlements settlements;
    world_entity_caravans caravans;
    world_entity_armies armies;
    world_entity_workers workers;
    world_entity_battles battles;
};

// Contains the match's world state, namely its active entities, and static elements such as tiles.
// Overall this represents the "observable state" of the match that can be interacted with indirectly by sending input commands into the next tick.
struct match_world_state
{
    ui64 age; // Age of the world in ticks.

    rand_generator_32 rand_gen; // Main random generator used by this world's systems.

	world_terrain* terrain; // World Terrain data.

	world_path_store* path_store; // Paths calculated for entity movement.

    world_entities* entities; // Entities storage.

    ui16 player_count; // Number of players in the match. Size of player_states.
    world_player_state* player_states; // Indexed by player ID.
};


#endif // WORLD_STATE_INCLUDED
