// Construction systems: advances the projects carried out by builder entities, and the tile modifications they perform.

#include "game_common/match/world/world.h"

#include "../world_sim.h"
#include "../builders/world_sim_builder.h"

// Makes the tile at the given location a road, and flags it for proximity recalculation if it is owned.
static void world_tile_set_road(match_world_state& world, world_location location)
{
    ui16 regionIndex, tileIndex;
    world_get_tile(*world.terrain, location, regionIndex, tileIndex);

    world_terrain::region& region = world.terrain->regions[regionIndex];
    region.status[tileIndex].is_road = true;
    if (region.owners[tileIndex] != world_terrain::UNOWNED_TILE) region.status[tileIndex].proximity_recalc = true;
}

// Builds one more tile of the road, within the origin settlement's territory. Returns true when the project is finished:
// the destination is reached, or no further progress is possible (unreachable, or the territory ends).
static bool builder_tick_build_road(match_world_state& world, ui16 builder_index)
{
    world_entity_builders& builders = world.entities->builders;
    build_road_data& data = *(build_road_data*)builders.action_data[builder_index].bytes;

    if (data.head == data.destination) return true;

    path_params params = {};
    params.passable_terrain = PATH_TERRAIN_LAND;
    params.within_settlement_territory = builders.origin_entity[builder_index];

    // Recalculated every tick, like movement. To be replaced by the path cache.
    path_result result;
    world_request_path(world, data.head, data.destination, params, result);
    if (result.count == 0) return true;

    data.head = world_path_next_tile(data.head, result.next_point);
    world_tile_set_road(world, data.head);
    return false;
}

void world_system_construction(match_world_state& world)
{
    world_entity_builders& builders = world.entities->builders;

    for (ui16 builderIndex = 0; builderIndex < builders.max_count; builderIndex++)
    {
        if (!builders.guids[builderIndex].is_valid()) continue;

        // A project can't go on without the entity it is tied to.
        bool finished = !world_entity_is_valid(world, builders.origin_entity[builderIndex]);

        if (!finished)
        {
            switch ((BUILD_TYPE)builders.build_types[builderIndex])
            {
            case BUILD_TYPE_ROAD:
                finished = builder_tick_build_road(world, builderIndex);
                break;
            default:
                finished = true; // Unknown build type.
                break;
            }
        }

        if (finished)
        {
            world_destroy_builder(world, builders.guids[builderIndex]);
            continue;
        }

        // Increment Time spent working on the project, so it is 0 during the first tick.
        builders.build_times[builderIndex]++;
    }
}
