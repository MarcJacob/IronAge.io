// Main declaration files for symbols related to Trade Caravan entities.

#ifndef WORLD_SIM_ENTITY_CARAVAN_INCLUDED
#define WORLD_SIM_ENTITY_CARAVAN_INCLUDED

#include "core.h"

#include "../world_sim.h"
#include "game_common/match/world/world_state.h"
#include "world_sim_entity_settlement.h"

// What caravans can cross: land only, straight segments. Other members zero-initialized.
static const path_params CARAVAN_PATH_PARAMS = { .passable_terrain = PATH_TERRAIN_LAND, .straight_segments = true };
// Ships (IS_SHIP): water only.
static const path_params SHIP_PATH_PARAMS = { .passable_terrain = PATH_TERRAIN_WATER, .straight_segments = true };

static inline bool caravan_is_ship(WORLD_ENTITY_CARAVAN_FLAGS flags)
{
    return ((ui8)flags & (ui8)WORLD_ENTITY_CARAVAN_FLAGS::IS_SHIP) != 0;
}

static inline const path_params& caravan_path_params(WORLD_ENTITY_CARAVAN_FLAGS flags)
{
    return caravan_is_ship(flags) ? SHIP_PATH_PARAMS : CARAVAN_PATH_PARAMS;
}

// Tile a caravan leaves from / travels to for a settlement: the settlement tile, or for ships its dock tile (settlement_dock_tile).
// A ship endpoint without a dock (should not happen: ship origins have a Trade Port, ship destinations are coastal) falls back to the settlement tile.
static inline world_location caravan_endpoint(const match_world_state& world, WORLD_ENTITY_CARAVAN_FLAGS flags, ui16 settlement_index)
{
    world_location tile = world.entities->settlements.locations[settlement_index];
    if (caravan_is_ship(flags)) settlement_dock_tile(world, settlement_index, tile);
    return tile;
}

// Tile a caravan travels to for a destination guid: a settlement (same as caravan_endpoint) or an army (its current tile). Ships never target armies.
static inline world_location caravan_destination_tile(const match_world_state& world, WORLD_ENTITY_CARAVAN_FLAGS flags, entity_guid destination)
{
    if (destination.get_type() == ENTITY_TYPE::ARMY)
    {
        ASSERT(!caravan_is_ship(flags));
        return world.entities->armies.locations[destination.get_index()];
    }
    return caravan_endpoint(world, flags, destination.get_index());
}

// Volume profit of delivering size goods to an army, from its current state (the selector's prediction and the delivery's actual profit).
ui64 caravan_army_trade_profit(const match_world_state& world, ui16 army_index, match_player_id origin_owner, ui32 origin_capacity, ui32 size, ui32 trade_dist);

// Picks a destination settlement for a caravan travelling from source (any location), excluding origin (may be INVALID_ENTITY_GUID; its capacity scales the profit).
// Candidates are the other active settlements (no reachability check) except destination_excludes (optional, e.g. destinations that just failed).
// For ships (caravan_flags IS_SHIP) only coastal ones (settlement_dock_tile). Distances stay between the settlement tiles.
// Armies are also candidates (land caravans with a valid origin only): army_is_trade_target, size min(value, supplies room, buy power), see caravan_army_candidate_weight.
// Caravan size per candidate: caravan_value (at most 0xFFFF), with size_to_room at most the candidate's room (capacity - local and export pools). Sizes below min_value are not candidates.
// Weight = ((size + exporter share of trade_volume_profit) x 1000 / tile distance) ^ 2 (see the weight helper), picked at random with world.rand_gen
// (not advanced without candidates). With size_to_room (a spawn, origin valid) it is instead the spawn score (profit x 100 / tile distance), only candidates
// reaching trade.min_profit_per_travel_dist_percent count (see caravan_spawn_score). Returns false if there is no destination, otherwise the destination and the caravan size for it.
bool world_caravan_select_destination(match_world_state& world, world_location source, entity_guid origin, ui32 caravan_value, ui32 min_value, bool size_to_room,
    WORLD_ENTITY_CARAVAN_FLAGS caravan_flags, entity_guid& out_destination, ui32& out_value,
    const entity_guid* destination_excludes = nullptr, ui32 destination_exclude_count = 0);

// Gives a caravan a new destination (its current one is invalid or unreachable), selected from its origin (or its own tile if the origin is gone).
// With none available it returns home (flag RETURNING_HOME) if the origin is valid, otherwise it is destroyed and its value is lost.
void world_caravan_retarget(match_world_state& world, entity_guid caravan);

entity_guid world_spawn_entity_caravan(match_world_state& world, const world_entity_caravan& start_state);
void world_destroy_entity_caravan(match_world_state& world, const entity_guid& caravan);

// Allocates the caravans' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_caravans(match_world_state& world, mem_arena& memory, ui32 count);

#endif // WORLD_SIM_ENTITY_CARAVAN_INCLUDED
