// Contains the core world state elements / types of a match.

#ifndef MATCH_WORLD_INCLUDED
#define MATCH_WORLD_INCLUDED

#include "core.h"
#include "game_common/game_data_object.h"

using match_player_id = ui16; // Index / unique ID of a player in the game, a single atomic participant in the match able to own entities and emit commands.
using match_player_count = match_player_id; // Alias type for places where we need to count player.

static constexpr match_player_id INVALID_MATCH_PLAYER_ID = ~0; // TODO(Marc): Make this 0 so things have an invalid player ID by default.

using world_location = vec2<ui16>;
using world_dimensions = vec2<ui16>;

static constexpr world_location INVALID_WORLD_LOCATION = { ~0, ~0 };
//
// @TEMP(Marc): Static memory size / layout properties.
constexpr ui32 SETTLEMENTS_PER_REGION = 10;
constexpr ui32 MICRO_ENTITIES_PER_TYPE_PER_REGION = 32;

struct world_terrain;
struct world_entities;
struct world_path_store; // Opaque outside the world simulation, see world_sim.h.

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

#endif // MATCH_WORLD_INCLUDED
