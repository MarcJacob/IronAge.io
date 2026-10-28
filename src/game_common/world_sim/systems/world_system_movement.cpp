// Main implementation file for entity movement tick systems.

#include "game_common/match/world/world.h"
#include "../world_sim.h"
#include "../world_movement.h"

#include "../entities/world_sim_entity_battle.h"
#include "../entities/world_sim_entity_caravan.h"

// What entities can cross (caravans: CARAVAN_PATH_PARAMS, ships: SHIP_PATH_PARAMS). Entities move in straight segments. Other members zero-initialized.
// TEMP: armies and workers also cross water while GAME_PARAMETERS.armies_workers_cross_water is set, until ships exist.
static const path_params ARMY_WORKER_PATH_PARAMS = { .passable_terrain = (ui8)(PATH_TERRAIN_LAND | (GAME_PARAMETERS.armies_workers_cross_water != 0 ? PATH_TERRAIN_WATER : 0)), .straight_segments = true };

// Divides rounding to the nearest integer (halves away from zero). den must be above 0.
static i64 div_round_nearest(i64 num, i64 den)
{
    return num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den);
}

// Moves the entity one step in a straight line towards the center of its move target (its current waypoint).
// Positions are absolute fractional units: tile * 1000 + fractional_loc (a tile's center is its fractional 0).
// The step is travel_speed / cost of the tile the entity stands on (rounded to nearest, cost at least 1): travel_speed is the speed on a road.
// Returns false, without moving, if the step would enter a tile that is impassable for params.
static bool process_entity_movement(const match_world_state& world, world_entity_movement& movement, world_location& location, const path_params& params)
{
    ui16 regionIndex, tileIndex;
    world_get_tile(*world.terrain, location, regionIndex, tileIndex);
    ui8 cost = ia_max((ui8)1, world_tile_move_cost(*world.terrain, regionIndex, tileIndex, params)); // An entity left on an impassable tile still moves.
    i32 step = ((i32)movement.travel_speed + cost / 2) / cost;

    i32 posX = (i32)location.x * 1000 + movement.fractional_loc.x;
    i32 posY = (i32)location.y * 1000 + movement.fractional_loc.y;
    i32 toTargetX = (i32)movement.move_target.x * 1000 - posX;
    i32 toTargetY = (i32)movement.move_target.y * 1000 - posY;

    // Distance rounded down by the integer square root. A step reaching it lands exactly on the waypoint's center.
    ui32 dist = ia_isqrt((ui64)((i64)toTargetX * toTargetX + (i64)toTargetY * toTargetY));
    i32 newPosX = (i32)movement.move_target.x * 1000;
    i32 newPosY = (i32)movement.move_target.y * 1000;
    if ((ui32)step < dist)
    {
        newPosX = posX + (i32)div_round_nearest((i64)toTargetX * step, dist);
        newPosY = posY + (i32)div_round_nearest((i64)toTargetY * step, dist);
    }

    // Back to tile + fraction. Shifted by 500 so that the tile edges are multiples of 1000 and the fraction ends up in [-500, 499].
    // The position never goes below -500: it moves between points that are not below it.
    i32 shiftedX = newPosX + 500, shiftedY = newPosY + 500;
    world_location newLocation = { (ui16)(shiftedX / 1000), (ui16)(shiftedY / 1000) };

    if (newLocation != location)
    {
        ui16 newRegionIndex, newTileIndex;
        world_get_tile(*world.terrain, newLocation, newRegionIndex, newTileIndex);
        if (world_tile_move_cost(*world.terrain, newRegionIndex, newTileIndex, params) == 0) return false;
    }

    location = newLocation;
    movement.fractional_loc = { (i16)(shiftedX % 1000 - 500), (i16)(shiftedY % 1000 - 500) };
    return true;
}

// Asks for a path from the entity's tile to its destination and makes the path's first waypoint the move target.
// Returns false if there is nothing to follow (the destination is unreachable even partially), leaving the entity as it is.
static bool request_next_waypoint(match_world_state& world, world_entity_movement& movement, world_location location, const path_params& params, bool* out_partial)
{
    // The path handle is always invalid for now, so the path is recalculated from the current tile every time it is requested.
    path_result result;
    world_request_path(world, location, movement.destination, params, result);
    movement.path_handle = result.handle;
    if (out_partial != nullptr) *out_partial = result.partial;

    // TODO(Marc): Nothing to follow, which means the destination is unreachable (even partially). For now the entity stays and asks again next tick.
    if (result.count == 0) return false;

    movement.move_target = result.next_point;
    movement.path_destination = movement.destination;
    return true;
}

bool world_entity_follow_path(match_world_state& world, world_entity_movement& movement, world_location& location, const path_params& params, bool* out_partial)
{
    if (location == movement.destination) return true; // Arrived.

    // A new path is needed when the entity stands on its waypoint (also true for an entity that never had one) or when its destination changed.
    bool onWaypoint = location == movement.move_target && movement.fractional_loc.x == 0 && movement.fractional_loc.y == 0;
    if (onWaypoint || movement.destination != movement.path_destination)
    {
        if (!request_next_waypoint(world, movement, location, params, out_partial)) return false;
    }

    // The step would enter a tile that became impassable: path again from here instead.
    if (!process_entity_movement(world, movement, location, params))
    {
        if (!request_next_waypoint(world, movement, location, params, out_partial)) return false;
    }
    return true;
}

void world_system_entity_movement_decision(match_world_state& world)
{
    const world_entity_settlements settlements = world.entities->settlements;

    world_entity_armies& armies = world.entities->armies;

	// Armies committed to an attack chase their target's current location. They stop if the target no longer exists.
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid()) continue;

		// Armies in a battle walk to the battle location, and are pinned once close enough.
		if (armies.battles[armyIndex].is_valid())
		{
			world_location battleLoc = world.entities->battles.states[armies.battles[armyIndex].get_index()].location;
			if (vec2_dist_squared(armies.locations[armyIndex], battleLoc) <= BATTLE_ARMY_DISTANCE * BATTLE_ARMY_DISTANCE) continue;
			armies.movements[armyIndex].destination = battleLoc;
            continue;
		}

        // Otherwise move to action target entity.
        // @TODO(Marc): Merge battle guid into action target.
		if (world_entity_is_valid(world, armies.action_targets[armyIndex]))
		{
			armies.movements[armyIndex].destination = world_entity_get_location(world, armies.action_targets[armyIndex]);
		}
		else
		{
            if (armies.action_targets[armyIndex]._value != INVALID_ENTITY_GUID._value)
            {
                // Target existed but was invalidated. Stop any movement.
                armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
                world_movement_stop(armies.movements[armyIndex], armies.locations[armyIndex]);
            }
            // ... Otherwise do nothing as we don't want to overwrite a destination set by itself.
		}
	}


    world_entity_caravans& caravans = world.entities->caravans;
    
    // Caravans move towards their destination settlement (ships: its dock tile, see caravan_endpoint). If it is gone, they pick a new one.
    for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
    {
        entity_guid caravan = caravans.guids[caravanIndex];
        if (!caravan.is_valid()) continue;

        // Returning caravans only walk back to their origin (the trade system refunds them there). Without an origin they are lost.
        if (((ui8)caravans.flags[caravanIndex] & (ui8)WORLD_ENTITY_CARAVAN_FLAGS::RETURNING_HOME) != 0)
        {
            entity_guid origin = caravans.origin_settlements[caravanIndex];
            if (origin.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, origin))
                caravans.movements[caravanIndex].destination = caravan_endpoint(world, caravans.flags[caravanIndex], origin.get_index());
            else
                world_destroy_entity_caravan(world, caravan);
            continue;
        }

        entity_guid destination = caravans.destination_settlements[caravanIndex];
        if (destination.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, destination))
        {
            caravans.movements[caravanIndex].destination = caravan_endpoint(world, caravans.flags[caravanIndex], destination.get_index());
        }
        else
        {
            world_caravan_retarget(world, caravan);
        }
    }
}

void world_system_entity_movement(match_world_state& world)
{
    world_entity_armies& armies = world.entities->armies;
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid()) continue;
		world_entity_follow_path(world, armies.movements[armyIndex], armies.locations[armyIndex], ARMY_WORKER_PATH_PARAMS);
	}

    world_entity_workers& workers = world.entities->workers;
    for (ui32 workerIndex = 0; workerIndex < workers.max_count; workerIndex++)
    {
        if (!workers.guids[workerIndex].is_valid()) continue;
        world_entity_follow_path(world, workers.movements[workerIndex], workers.locations[workerIndex], ARMY_WORKER_PATH_PARAMS);
    }

    world_entity_caravans& caravans = world.entities->caravans;
    for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
    {
        if (!caravans.guids[caravanIndex].is_valid()) continue;

        bool partial = false;
        bool hasPath = world_entity_follow_path(world, caravans.movements[caravanIndex], caravans.locations[caravanIndex], caravan_path_params(caravans.flags[caravanIndex]), &partial);

        // No (or only a partial) path means the destination is unreachable: pick another one.
        // Returning caravans are left alone: they keep asking for a path home.
        bool returning = ((ui8)caravans.flags[caravanIndex] & (ui8)WORLD_ENTITY_CARAVAN_FLAGS::RETURNING_HOME) != 0;
        if ((!hasPath || partial) && !returning)
        {
            world_caravan_retarget(world, caravans.guids[caravanIndex]);
        }
    }
}
