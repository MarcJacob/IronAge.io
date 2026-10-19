// Main collection of symbols for accessing / running the world simulation systems.

#ifndef WORLD_SIM_INCLUDED
#define WORLD_SIM_INCLUDED

#include "game_common/match/start_params.h"
#include "game_common/match/world.h"
#include "game_common/match/pathing.h"

// TERRAIN TRAVEL

// Travel difficulty over each terrain type. Used for tile proximity and pathfinding.
static const ui8 TILE_TERRAIN_TRAVEL_COST[] = {
    3, // LAND_PLAINS
    5, // LAND_HILLS
    10, // LAND_MOUNTAINS

    1, // WATER_COAST
    4, // WATER_SEA
};

// Travel difficulty of a road tile, whatever its terrain.
static constexpr ui8 ROAD_TRAVEL_COST = 1;

// Distances are measured with chamfer weights (integer approximation of euclidean distance, ratio 7 / 5 = 1.4 ~ sqrt(2)).
// Stepping to a tile adds its travel cost times the weight.
static constexpr ui16 PROXIMITY_WEIGHT_ORTHOGONAL = 5;
static constexpr ui16 PROXIMITY_WEIGHT_DIAGONAL = 7;

// Neighbour offsets, non-diagonal only or all 8.
static const vec2<i8> NEIGHBOUR_OFFSETS_ORTHOGONAL[] = { {-1, 0}, {0, -1}, {0, 1}, {1, 0} };
static const vec2<i8> NEIGHBOUR_OFFSETS_ALL[] = { {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1} };

// Returns the travel cost of entering a tile for the given path parameters. 0 means impassable / invalid for the given params.
static inline ui8 world_tile_move_cost(const world_terrain& terrain, ui16 region_index, ui16 tile_index, const path_params& params)
{
    const world_terrain::region& region = terrain.regions[region_index];
    TERRAIN_TILE_TYPE type = region.terrain_types[tile_index];

    // Check terrain flags. If all flags are at 0, then all terrains are allowed.
    if (params.passable_terrain != 0 && (params.passable_terrain & (1 << (ui8)type)) == 0) return 0;

    // Roads return the road travel cost no matter what.
    if (region.status[tile_index].is_road) return ROAD_TRAVEL_COST;

    // Stop here if we can only path on roads.
    if (params.roads_only) return 0;

    // If a valid "within settlement territory" guid was given, prevent pathing through any tile that isn't in the settlement.
    if (params.within_settlement_territory.is_valid() 
            && params.within_settlement_territory.get_index() != terrain.regions[region_index].owners[tile_index]) return 0;

    // Default case: "native" terrain travel cost.
    return TILE_TERRAIN_TRAVEL_COST[(ui8)type];
}

// PATH STORE

// What a path request to the world's path store gives back: enough to follow the path without holding the path itself.
struct path_result
{
    world_location next_point; // First waypoint after the start: the point to move straight towards.
    ui16 count; // Number of waypoints. 0 = nothing to do (start == end, invalid end) or no progress possible.
    ui16 length; // Total length in tiles.
    bool partial; // The path ends at the closest reachable tile instead of at the requested target.
    ui32 handle; // Identifies the stored path for later re-use. INVALID_PATH_HANDLE = none (the store doesn't keep paths yet).
};

// Size of the store's search memory. Fixed, as it limits how far a search goes and so must be the same on every machine.
static constexpr ui64 WORLD_PATH_STORE_SCRATCH_SIZE = KiB(256);
static constexpr ui16 WORLD_PATH_STORE_MAX_WAYPOINTS = 256;

// The world's path store: for now only holds the last path calculated, which each request overwrites.
struct world_path_store
{
    mem_arena scratch; // Search memory, cleared by every request.
    world_location points[WORLD_PATH_STORE_MAX_WAYPOINTS]; // Waypoints of the last path.
    path last; // The last path, its points being the array above.
};

// Allocates and initializes a path store from the given memory. Returns nullptr if there isn't enough.
world_path_store* world_path_store_init(mem_arena& memory);

// Requests a path from the world's path store, which may compute it or re-use a stored one. Simulation code only.
// out_result.count is 0 if there is nothing to follow.
void world_request_path(match_world_state& world, world_location start, world_location end, const path_params& params, path_result& out_result);

// ENTITIES

// Spawns a new settlement in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_settlement(match_world_state& world, const world_entity_settlements::single& start_state);

// Spawns a new caravan in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_caravan(match_world_state& world, const world_entity_caravans::single& start_state);

// Spawns a new army in the world with the provided start state and returns its GUID. It has no warbands yet: add at least one right away (world_spawn_warband).
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_army(match_world_state& world, const world_entity_armies::single& start_state);

// Spawns a new warband in the given army (valid, with room for it) with the provided start state and returns its GUID. Its owner is the army's.
entity_guid world_spawn_warband(match_world_state& world, const world_entity_warbands::single& start_state, entity_guid army);

// Spawns a new battle in the world with the provided start state and returns its GUID.
entity_guid world_spawn_battle(match_world_state& world, const world_entity_battles::state& start_state);

// Spawns a new siege in the world with the provided start state and returns its GUID. Add armies with world_siege_add_army, not through the start state.
entity_guid world_spawn_siege(match_world_state& world, const world_entity_sieges::state& start_state);

// Destroys a siege: its armies are no longer in it (free to move again).
void world_destroy_siege(match_world_state& world, entity_guid siege);

// Spawns the army a settlement defends itself with when attacked (siege assault or sally): one warband with no home, of settlement_defender_men.
// Its men are taken out of the settlement (levies from the population, the whole local troop pool), world_disband_army gives them back.
entity_guid world_spawn_settlement_defenders(match_world_state& world, entity_guid settlement);

// Siege of a settlement, INVALID_ENTITY_GUID if there is none.
entity_guid world_settlement_siege(const match_world_state& world, entity_guid settlement);

void world_destroy_settlement(match_world_state& world, entity_guid settlement);
void world_destroy_caravan(match_world_state& world, entity_guid caravan);
void world_destroy_army(match_world_state& world, entity_guid army); // Destroys its warbands too.
void world_destroy_battle(match_world_state& world, entity_guid battle);

// Destroys a warband, taking it out of its army. Destroys the army too if it was its last warband.
void world_destroy_warband(match_world_state& world, entity_guid warband);

// Destroys an army and returns the men of all its warbands to the settlement: levies to the population, the other types to the local troop pool.
void world_disband_army(match_world_state& world, entity_guid army, entity_guid settlement);

void world_settlement_decrease_population(match_world_state& world, entity_guid settlement, ui32 amount);

// Counts the warbands raised from a settlement (home settlement with the same owner) and their total men.
// Those men still count in the settlement's population (levies) and local troops (other types).
void world_settlement_raised(const match_world_state& world, entity_guid settlement, ui32& out_warbands, ui32& out_men);

// Removes men a warband lost (or took away to settle) from its home settlement: levies from the population (never below 1), other types from the local pool.
// Does nothing if the home settlement is gone or changed hands.
void world_warband_remove_men_from_home(match_world_state& world, ui16 warband_index, const world_entity_warbands::comp& men);

// Moves every warband of an army into another army of the same owner, then destroys the emptied army.
// Returns false (nothing done) if the warbands don't all fit in the other army.
bool world_army_merge(match_world_state& world, entity_guid army, entity_guid into_army);

// Moves the warbands at the given slots of an army (bit N = slot N of its warband list) into a new army of the same owner, on a walkable tile
// next to it (its own tile if there is none). The selection must be valid: slots within the list, not all of them, at least one.
// Returns the new army.
entity_guid world_army_detach(match_world_state& world, entity_guid army, ui64 slots);

bool world_alloc_settlements(match_world_state& world, mem_arena& memory, ui32 count);

bool world_alloc_caravans(match_world_state& world, mem_arena& memory, ui32 count);

bool world_alloc_armies(match_world_state& world, mem_arena& memory, ui32 count);

bool world_alloc_battles(match_world_state& world, mem_arena& memory, ui32 count);

bool world_alloc_warbands(match_world_state& world, mem_arena& memory, ui32 count);

bool world_alloc_sieges(match_world_state& world, mem_arena& memory, ui32 count);

// Spawns a new builder in the world with the provided start state and returns its GUID.
// Returns INVALID_ENTITY_GUID if the origin is not a valid entity. Asserts if its type is not the one the build type requires.
entity_guid world_spawn_builder(match_world_state& world, const world_entity_builders::single& start_state);

void world_destroy_builder(match_world_state& world, entity_guid builder);

bool world_alloc_builders(match_world_state& world, mem_arena& memory, ui32 count);

// BUILD TYPES

// Everything that can be built.
enum BUILD_TYPE : ui16
{
    BUILD_TYPE_ROAD,
    BUILD_TYPE_COUNT,
};

// Common properties of a build type. Its progression logic is selected by the BUILD_TYPE in the construction system.
struct build_type_definition
{
    ENTITY_TYPE origin_entity_type; // Type of entity a builder of this type must be tied to.
};

static const build_type_definition BUILD_TYPE_DEFINITIONS[BUILD_TYPE_COUNT] = {
    { ENTITY_TYPE::SETTLEMENT }, // BUILD_TYPE_ROAD
};

// Layout of the action data of a road project.
struct build_road_data
{
    world_location start; // Tile the road starts from.
    world_location head; // Last tile built, which is where the road continues from.
    world_location destination; // Tile the road is going towards.
};
static_assert(sizeof(build_road_data) <= WORLD_BUILD_ACTION_DATA_SIZE, "Road action data doesn't fit the builder's action data.");

// SYSTEMS & HELPERS

// Makes the tile at the given location a road, and flags it for proximity recalculation if it is owned.
void world_tile_set_road(match_world_state& world, world_location location);

// Advances all construction projects (builders) by one tick, destroying the finished ones.
void world_system_construction(match_world_state& world);

// Armies in a battle move towards the battle location until they are this many tiles from it, then stay pinned.
constexpr i32 BATTLE_ARMY_DISTANCE = 2;

// Makes an entity stop where it is, dropping its path and the step it was making.
static inline void world_movement_stop(world_entity_movement& movement, world_location location)
{
    movement.destination = location;
    movement.move_target = location;
    movement.path_handle = INVALID_PATH_HANDLE;
}

// Claims a tile in the world for the given settlement using the origin proximity value as proximity base.
// Ensure settlement's influence bounds rectangle is kept up to date !
ui16 world_influence_claim_tile(const match_world_state& world, ui16 region_index, ui16 tile_index, ui16 settlement_index);
void world_system_economy_proximity_recalc(match_world_state& world);
void world_system_economy_influence_spread(match_world_state& world);

void world_system_economy_contestation(match_world_state& world);

void world_system_economy_update_pressure(match_world_state& world);

void world_system_economy_pop_growth(match_world_state& world);

void world_system_economy_troop_growth(match_world_state& world);

void world_system_settlement_spawn_caravans(match_world_state& world);

void world_system_entity_movement_decision(match_world_state& world);

void world_system_entity_movement(match_world_state& world);

void world_system_caravan_arrivals(match_world_state& world);

void world_system_armies_target_reached(match_world_state& world);

// Raising warbands (whose army is not in a battle) draw men from their home settlement. Their army waits until they are all raised (see world_system_entity_movement).
void world_system_armies_raising(match_world_state& world);

void world_system_battles(match_world_state& world);

// Adds an army to a siege (pinning it there) and has it join the siege's battle if one is in progress. Returns false (nothing done) if the siege is full.
bool world_siege_add_army(match_world_state& world, entity_guid siege, entity_guid army);

// Takes an army out of its siege. The siege ends if it was its last army.
void world_siege_remove_army(match_world_state& world, entity_guid army);

// Starts the battle of a siege: every besieger not already fighting elsewhere on side 0, against the settlement's defenders (world_spawn_settlement_defenders) on side 1.
// defense_bonus doubles the defenders' defense (assault). The siege ends once the battle is over (see world_system_sieges).
void world_siege_start_battle(match_world_state& world, entity_guid siege, bool defense_bonus);

// Sieges: their armies leaving or dying, their end, and the attrition of the besieged settlements (every growth tick).
void world_system_sieges(match_world_state& world);

// LIFECYCLE

// Generates the terrain of a world whose terrain size and regions are already set up.
void world_terrain_generate(world_terrain& terrain);

// Initializes a new world state into the given memory from match start parameters.
match_world_state* world_state_init(mem_arena& memory, const game_match_start_params& start_params);

// Advances the world state by one tick.
void world_state_tick(match_world_state& world);

#endif // WORLD_SIM_INCLUDED
