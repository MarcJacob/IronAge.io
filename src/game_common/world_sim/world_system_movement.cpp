// Main implementation file for entity pathfinding & movement code.

#include "game_common/match/world.h"
#include "world_sim.h"

void process_entity_movement (world_entity_movement& movement, world_location& location)
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

void world_system_entity_movement_decision(match_world_state& world)
{
    const world_entity_settlements settlements = world.entities.settlements;

    world_entity_caravans& caravans = world.entities.caravans;

	// Make sure caravans have their movement target set to their destination settlement.
	for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
	{
		if (!caravans.guids[caravanIndex].is_valid()) continue;

		// Despawn caravans whose destination settlement was destroyed.
		if (!world_entity_is_valid(world, caravans.dest_settlements[caravanIndex]))
		{
			caravans.guids[caravanIndex] = INVALID_ENTITY_GUID;
			caravans.active_count--;
			continue;
		}

		const world_location& destLoc = settlements.locations[caravans.dest_settlements[caravanIndex].get_index()];
		caravans.movements[caravanIndex].move_target = destLoc;
	}

    world_entity_armies& armies = world.entities.armies;

	// Armies committed to an attack chase their target's current location. They stop if the target no longer exists.
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || !armies.action_targets[armyIndex].is_valid()) continue;

		if (world_entity_is_valid(world, armies.action_targets[armyIndex]))
		{
			armies.movements[armyIndex].move_target = world_get_entity_location(world, armies.action_targets[armyIndex]);
		}
		else
		{
			armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
			armies.movements[armyIndex].move_target = armies.locations[armyIndex];
		}
	}
}

void world_system_entity_movement(match_world_state& world)
{
    world_entity_caravans& caravans = world.entities.caravans;
    world_entity_armies& armies = world.entities.armies;

	for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
	{
		if (!caravans.guids[caravanIndex].is_valid()) continue;

		process_entity_movement(caravans.movements[caravanIndex], caravans.locations[caravanIndex]);
	}

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid()) continue;

		process_entity_movement(armies.movements[armyIndex], armies.locations[armyIndex]);
	}
}
