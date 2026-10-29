// Main implementation file for entity movement tick systems.

#include "game_common/match/world/world.h"
#include "../world_sim.h"
#include "../world_movement.h"

#include "../entities/world_sim_entity_battle.h"
#include "../entities/world_sim_entity_caravan.h"

// Divides rounding to the nearest integer (halves away from zero). den must be above 0.
static i64 div_round_nearest(i64 num, i64 den)
{
    return num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den);
}

static bool tile_passable(const match_world_state& world, world_location tile, const path_params& params)
{
    ui16 regionIndex, tileIndex;
    world_get_tile(*world.terrain, tile, regionIndex, tileIndex);
    return world_tile_move_cost(*world.terrain, regionIndex, tileIndex, params) != 0;
}

// Moves the entity one step in a straight line towards the center of the target tile.
// Positions are absolute fractional units: tile * 1000 + fractional_loc (a tile's center is its fractional 0).
// The step is travel_speed / cost of the tile the entity stands on (rounded to nearest, cost at least 1): travel_speed is the speed on a road.
// Returns false, without moving, if the step would enter a tile that is impassable for params.
bool world_entity_step_towards(const match_world_state& world, world_entity_movement& movement, world_location& location, world_location target, const path_params& params)
{
    ui16 regionIndex, tileIndex;
    world_get_tile(*world.terrain, location, regionIndex, tileIndex);
    ui8 cost = ia_max((ui8)1, world_tile_move_cost(*world.terrain, regionIndex, tileIndex, params)); // An entity left on an impassable tile still moves.
    i32 step = ((i32)movement.travel_speed + cost / 2) / cost;

    i32 posX = (i32)location.x * 1000 + movement.fractional_loc.x;
    i32 posY = (i32)location.y * 1000 + movement.fractional_loc.y;
    i32 toTargetX = (i32)target.x * 1000 - posX;
    i32 toTargetY = (i32)target.y * 1000 - posY;

    // Distance rounded down by the integer square root. A step reaching it lands exactly on the waypoint's center.
    ui32 dist = ia_isqrt((ui64)((i64)toTargetX * toTargetX + (i64)toTargetY * toTargetY));
    i32 newPosX = (i32)target.x * 1000;
    i32 newPosY = (i32)target.y * 1000;
    if ((ui32)step < dist)
    {
        newPosX = posX + (i32)div_round_nearest((i64)toTargetX * step, dist);
        newPosY = posY + (i32)div_round_nearest((i64)toTargetY * step, dist);
    }

    // Back to tile + fraction. Shifted by 500 so that the tile edges are multiples of 1000 and the fraction ends up in [-500, 499].
    // The position never goes below -500: it moves between points that are not below it.
    i32 shiftedX = newPosX + 500, shiftedY = newPosY + 500;
    world_location newLocation = { (ui16)(shiftedX / 1000), (ui16)(shiftedY / 1000) };

    if (newLocation != location && !tile_passable(world, newLocation, params))
    {
        // Slide along the blocked edge: keep the move on one axis and hold the other at the current tile's border.
        // This happens when a straight line cuts a tile corner (the side tiles were never checked): the next tick can then cross the corner.
        // Only tiles that are passable (or the current tile) are ever entered. The axis with the longer way to go is preferred.
        world_location keepX = { newLocation.x, location.y };
        world_location keepY = { location.x, newLocation.y };
        bool keepXOk = keepX == location || tile_passable(world, keepX, params);
        bool keepYOk = keepY == location || tile_passable(world, keepY, params);
        bool xFirst = (toTargetX < 0 ? -toTargetX : toTargetX) >= (toTargetY < 0 ? -toTargetY : toTargetY);
        if (keepXOk && (xFirst || !keepYOk))
        {
            if (newLocation.y != location.y) newPosY = (i32)location.y * 1000 + (newLocation.y > location.y ? 499 : -500);
        }
        else if (keepYOk)
        {
            if (newLocation.x != location.x) newPosX = (i32)location.x * 1000 + (newLocation.x > location.x ? 499 : -500);
        }
        else return false;

        if (newPosX == posX && newPosY == posY) return false; // Nothing left to move.
        shiftedX = newPosX + 500;
        shiftedY = newPosY + 500;
        newLocation = { (ui16)(shiftedX / 1000), (ui16)(shiftedY / 1000) };
    }

    location = newLocation;
    movement.fractional_loc = { (i16)(shiftedX % 1000 - 500), (i16)(shiftedY % 1000 - 500) };
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
		world_path_store_move_entity(world, armies.movements[armyIndex], armies.locations[armyIndex]);
	}

    world_entity_workers& workers = world.entities->workers;
    for (ui32 workerIndex = 0; workerIndex < workers.max_count; workerIndex++)
    {
        if (!workers.guids[workerIndex].is_valid()) continue;
        world_path_store_move_entity(world, workers.movements[workerIndex], workers.locations[workerIndex]);
    }

    world_entity_caravans& caravans = world.entities->caravans;
    for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
    {
        if (!caravans.guids[caravanIndex].is_valid()) continue;

        // The result (false = arrived or impossible) is not used yet: no unreachable-destination handling for now.
        world_path_store_move_entity(world, caravans.movements[caravanIndex], caravans.locations[caravanIndex]);
    }
}
