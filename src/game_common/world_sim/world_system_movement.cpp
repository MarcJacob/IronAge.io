// Main implementation file for entity movement tick systems.

#include "game_common/match/world/world.h"
#include "world_sim.h"

#include "battles/world_sim_entity_battle.h"
#include "caravans/world_sim_entity_caravan.h"

// What entities can cross for now: land only.
static const path_params ENTITY_PATH_PARAMS = { PATH_TERRAIN_LAND }; // Other members zero-initialized.

// Moves the entity towards its move target (always an adjacent tile, or its own tile) using its speed and in-tile location.
static void process_entity_movement (world_entity_movement& movement, world_location& location)
{
    if (movement.move_target == location) return;

    vec2<i32> travelVec = (vec2<i32>)(movement.move_target) - location;

    // Figure out a distribution over the 0, 256 range of both axis representing an absolute travel direction.
    // The relative values of the axis will determine which will get more of the movement.
    vec2<ui8> travelDir;
    if (travelVec.y == 0) travelDir = { 255, 0 };
    else if (travelVec.x == 0) travelDir = { 0, 255 };
    else
    {
        ui16 travelAbsX = ia_abs(travelVec.x), travelAbsY = ia_abs(travelVec.y);
        if (travelAbsX == travelAbsY) travelDir = { 127, 127 };
        // Get a reasonably accurate measure of how much larger one is compared to the other and use it to determine distribution.
        if (travelAbsX > travelAbsY)
        {
            ui32 xMult = travelAbsX * 1000 / travelAbsY;
            ui8 parts = 255 * 1000 / (xMult + 1000);
            travelDir.x = 255 - parts;
            travelDir.y = parts;
        }
        else
        {
            ui32 yMult = travelAbsY * 1000 / travelAbsX;
            ui8 parts = 255 * 1000 / (yMult + 1000);
            travelDir.y = 255 - parts;
            travelDir.x = parts;
        }
    }

    // Turn the travel vector into a signs vector.
    travelVec = { (travelVec.x >= 0) ? 1 : -1, (travelVec.y >= 0) ? 1 : -1 };

    // With absolute travel vector, we can determine where to apply the movement value of the entity.
    i16 xTravel = travelDir.x * 1000 / 255 * movement.travel_speed / 100;
    i16 yTravel = travelDir.y * 1000 / 255 * movement.travel_speed / 100;

    i16 newFracX = movement.fractional_loc.x + (i16)xTravel * travelVec.x;
    i16 newFracY = movement.fractional_loc.y + (i16)yTravel * travelVec.y;

    while (newFracX > 100 && location.x < movement.move_target.x) { location.x++; newFracX -= 200; }
    while (newFracX < -100 && location.x > movement.move_target.x) { location.x--; newFracX += 200; }

    while (newFracY > 100 && location.y < movement.move_target.y) { location.y++; newFracY -= 200; }
    while (newFracY < -100 && location.y > movement.move_target.y) { location.y--; newFracY += 200; }

    movement.fractional_loc.x = newFracX * (location.x != movement.move_target.x);
    movement.fractional_loc.y = newFracY * (location.y != movement.move_target.y);

}

bool world_entity_follow_path(match_world_state& world, world_entity_movement& movement, world_location& location, bool* out_partial)
{
    if (location == movement.move_target)
    {
        if (location == movement.destination) return true; // Arrived.

        // The path handle is always invalid for now, so the path is recalculated from the current tile every time.
        path_result result;
        world_request_path(world, location, movement.destination, ENTITY_PATH_PARAMS, result);
        movement.path_handle = result.handle;
        if (out_partial != nullptr) *out_partial = result.partial;

        // TODO(Marc): Nothing to follow, which means the destination is unreachable (even partially). For now the entity stays and asks again next tick.
        if (result.count == 0) return false;

        movement.move_target = world_path_next_tile(location, result.next_point);
    }

    process_entity_movement(movement, location);
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
    
    // Caravans move towards their destination settlement. If it is gone, they pick a new one.
    for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
    {
        entity_guid caravan = caravans.guids[caravanIndex];
        if (!caravan.is_valid()) continue;

        // Returning caravans only walk back to their origin (the trade system refunds them there). Without an origin they are lost.
        if (((ui8)caravans.flags[caravanIndex] & (ui8)WORLD_ENTITY_CARAVAN_FLAGS::RETURNING_HOME) != 0)
        {
            entity_guid origin = caravans.origin_settlements[caravanIndex];
            if (origin.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, origin))
                caravans.movements[caravanIndex].destination = world_entity_get_location(world, origin);
            else
                world_destroy_entity_caravan(world, caravan);
            continue;
        }

        entity_guid destination = caravans.destination_settlements[caravanIndex];
        if (destination.get_type() == ENTITY_TYPE::SETTLEMENT && world_entity_is_valid(world, destination))
        {
            caravans.movements[caravanIndex].destination = world_entity_get_location(world, destination);
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
		world_entity_follow_path(world, armies.movements[armyIndex], armies.locations[armyIndex]);
	}

    world_entity_workers& workers = world.entities->workers;
    for (ui32 workerIndex = 0; workerIndex < workers.max_count; workerIndex++)
    {
        if (!workers.guids[workerIndex].is_valid()) continue;
        world_entity_follow_path(world, workers.movements[workerIndex], workers.locations[workerIndex]);
    }

    world_entity_caravans& caravans = world.entities->caravans;
    for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
    {
        if (!caravans.guids[caravanIndex].is_valid()) continue;

        bool partial = false;
        bool hasPath = world_entity_follow_path(world, caravans.movements[caravanIndex], caravans.locations[caravanIndex], &partial);

        // No (or only a partial) path means the destination is unreachable: pick another one.
        // Returning caravans are left alone: they keep asking for a path home.
        bool returning = ((ui8)caravans.flags[caravanIndex] & (ui8)WORLD_ENTITY_CARAVAN_FLAGS::RETURNING_HOME) != 0;
        if ((!hasPath || partial) && !returning)
        {
            world_caravan_retarget(world, caravans.guids[caravanIndex]);
        }
    }
}
