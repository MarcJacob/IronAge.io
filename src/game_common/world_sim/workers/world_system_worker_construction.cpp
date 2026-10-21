// Construction systems: advances the projects carried out by builder entities, and the tile modifications they perform.

#include "game_common/match/world/world.h"

#include "../world_sim.h"
#include "world_sim_entity_worker.h"

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
static bool worker_logic_build_road_tick(match_world_state& world, ui16 builder_index)
{
    world_entity_workers& workers = world.entities->workers;
    build_road_data& data = *(build_road_data*)workers.action_data[builder_index].bytes;

    if (data.head == data.destination) return true;

    path_params params = {};
    params.passable_terrain = PATH_TERRAIN_LAND;
    params.within_settlement_territory = workers.origin_entity[builder_index];

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
    world_entity_workers& workers = world.entities->workers;

    for (ui16 builderIndex = 0; builderIndex < workers.max_count; builderIndex++)
    {
        if (!workers.guids[builderIndex].is_valid()) continue;

        // A project can't go on without the entity it is tied to.
        bool finished = !world_entity_is_valid(world, workers.origin_entity[builderIndex]);

        if (!finished)
        {
            // @TODO(Marc): Similarly to input command, have an application logic function for each work type.
            switch ((WORK_TYPE)workers.work_types[builderIndex])
            {
            case WORK_TYPE::BUILD_ROAD:
                finished = worker_logic_build_road_tick(world, builderIndex);
                break;
            default:
                finished = true; // Unknown build type.
                break;
            }
        }

        if (finished)
        {
            world_destroy_worker(world, workers.guids[builderIndex]);
            continue;
        }

        // Increment Time spent working on the project, so it is 0 during the first tick.
        workers.work_times[builderIndex]++;
    }
}
