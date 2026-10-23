// Construction systems: advances the projects carried out by worker entities, and the tile modifications they perform.

#include "game_common/match/world/world.h"

#include "../world_sim.h"
#include "../entities/world_sim_entity_worker.h"
#include "../entities/world_sim_entity_settlement.h"

// Makes the tile at the given location a road, and flags it for proximity recalculation if it is owned.
static void world_tile_set_road(match_world_state& world, world_location location)
{
    ui16 regionIndex, tileIndex;
    world_get_tile(*world.terrain, location, regionIndex, tileIndex);

    world_terrain::region& region = world.terrain->regions[regionIndex];
    region.status[tileIndex].is_road = true;
    if (region.owners[tileIndex] != world_terrain::UNOWNED_TILE) region.status[tileIndex].proximity_recalc = true;
}

// Road crew: starts at the first non-road tile of the path (road tiles are skipped for free), walks to the tile and builds it there, one tile at a time.
// Building a tile takes GAME_PARAMETERS.road_build_ticks_per_cost ticks per point of its travel cost without a road. The crew is paid for a day (GAME_PARAMETERS.road_crew_cost_per_day, treasury -> settlement's
// pending worker pay) at its first tick and then on each daily tick; it only works while the current day is paid. Returns true when the project is finished:
// the destination is reached, or no further progress is possible (unreachable).
static bool worker_logic_build_road_tick(match_world_state& world, ui16 worker_index)
{
    world_entity_workers& workers = world.entities->workers;
    build_road_data& data = *(build_road_data*)workers.action_data[worker_index].bytes;

    if (world_is_daily_tick(world) || workers.work_times[worker_index] == 0)
    {
        ui16 originIndex = workers.origin_entity[worker_index].get_index();
        match_player_id owner = world.entities->settlements.owners[originIndex];
        data.funded = owner < world.player_count && world.player_states[owner].treasury >= GAME_PARAMETERS.road_crew_cost_per_day;
        if (data.funded) settlement_wealth_fund_from_treasury(world, originIndex, GAME_PARAMETERS.road_crew_cost_per_day, &settlement_pending_income::worker_pay);
    }
    if (!data.funded) return false; // Stalled until a day can be paid.

    path_params params = {};
    params.passable_terrain = PATH_TERRAIN_LAND;

    // Find the next tile to build, skipping the ones that are already roads.
    world_location next;
    ui16 regionIndex, tileIndex;
    while (true)
    {
        if (data.head == data.destination) return true;

        // Recalculated every tick, like movement. To be replaced by the path cache.
        path_result result;
        world_request_path(world, data.head, data.destination, params, result);
        if (result.count == 0) return true;

        next = world_path_next_tile(data.head, result.next_point);
        world_get_tile(*world.terrain, next, regionIndex, tileIndex);
        if (!world.terrain->regions[regionIndex].status[tileIndex].is_road) break;

        data.head = next;
        data.ticks_left = 0;
    }

    // Walk to the tile.
    world_entity_movement& movement = workers.movements[worker_index];
    world_location& location = workers.locations[worker_index];
    movement.destination = next;
    if (location != next) return false;

    // On the tile: the first tick sets the build time (the tile is not a road, so this is its terrain cost), then the tile is built when it runs out.
    // Tiles not owned by the settlement's player (unowned, or another player's) take road_build_foreign_multiplier times longer.
    if (data.ticks_left == 0)
    {
        data.ticks_left = (ui16)(ia_max((ui8)1, world_tile_move_cost(*world.terrain, regionIndex, tileIndex, params)) * GAME_PARAMETERS.road_build_ticks_per_cost);
        ui16 tileOwner = world.terrain->regions[regionIndex].owners[tileIndex];
        match_player_id crewOwner = world.entities->settlements.owners[workers.origin_entity[worker_index].get_index()];
        if (tileOwner == world_terrain::UNOWNED_TILE || world.entities->settlements.owners[tileOwner] != crewOwner)
            data.ticks_left *= GAME_PARAMETERS.road_build_foreign_multiplier;
    }
    data.ticks_left--;
    if (data.ticks_left == 0)
    {
        world_tile_set_road(world, next);
        data.head = next;
    }
    return false;
}

// Walks to the target with the colonists, then founds a settlement there; if that is not possible, walks back and gives the colonists back.
// Returns true when the project is finished.
static bool worker_logic_found_settlement_tick(match_world_state& world, ui16 worker_index)
{
    world_entity_workers& workers = world.entities->workers;
    found_settlement_data& data = *(found_settlement_data*)workers.action_data[worker_index].bytes;
    world_entity_movement& movement = workers.movements[worker_index];
    world_location& location = workers.locations[worker_index];

    const world_entity_settlements& settlements = world.entities->settlements;
    ui16 originIndex = workers.origin_entity[worker_index].get_index();

    movement.destination = data.returning ? settlements.locations[originIndex] : data.target;
    if (location != movement.destination)
    {
        return false;
    }

    // Back home: the colonists rejoin the peasants.
    if (data.returning)
    {
        settlement_add_peasants(world, originIndex, data.colonists);
        return true;
    }

    // At the target. Checked again: the situation may have changed since the worker left (location taken, slots filled, owner changed).
    match_player_id owner = settlements.owners[originIndex];
    if (!world_is_valid_settlement_location(world, data.target, owner) || settlements.active_count >= settlements.max_count)
    {
        data.returning = 1;
        return false;
    }

    world_entity_settlements::single newSettlement = {};
    newSettlement.owner = owner;
    newSettlement.location = data.target;
    newSettlement.population.peasants = data.colonists;
    world_spawn_settlement(world, newSettlement);
    return true;
}

// Daily financing of a settlement building project. A day is financed when the project holds its workers (taken from the free peasants) AND the owner's
// treasury can pay the row's cost_per_day (moved to the settlement's pending worker pay); then progress +1. Otherwise the day stalls: held workers return to the
// peasants and nothing is paid. When the build time is reached the workers are released and the building is added. Returns true when the project is finished.
static bool worker_logic_build_settlement_building_tick(match_world_state& world, ui16 worker_index)
{
    if (!world_is_daily_tick(world)) return false;

    world_entity_workers& workers = world.entities->workers;
    build_settlement_building_data& data = *(build_settlement_building_data*)workers.action_data[worker_index].bytes;
    if (data.building_type >= SETTLEMENT_BUILDING_TYPE_COUNT) return true; // Unknown building type.

    const settlement_building_def& def = SETTLEMENT_BUILDING_TABLE[data.building_type];
    const world_entity_settlements& settlements = world.entities->settlements;
    ui16 settlementIndex = workers.origin_entity[worker_index].get_index();
    match_player_id owner = settlements.owners[settlementIndex];

    // Take the missing workers, then pay: no payment happens unless the whole day can be financed.
    data.held_workers += (ui8)settlement_assign_workers(world, settlementIndex, def.workers - data.held_workers);
    bool hasWorkers = data.held_workers == def.workers;
    bool canPay = owner < world.player_count && world.player_states[owner].treasury >= def.cost_per_day;

    if (!hasWorkers || !canPay)
    {
        settlement_release_workers(world, settlementIndex, data.held_workers);
        data.held_workers = 0;
        return false;
    }

    settlement_wealth_fund_from_treasury(world, settlementIndex, def.cost_per_day, &settlement_pending_income::worker_pay);
    data.progress_days++;
    if (data.progress_days < def.build_days) return false;

    settlement_release_workers(world, settlementIndex, data.held_workers);
    data.held_workers = 0;
    settlement_building_add(world, settlementIndex, data.building_type, 1);
    return true;
}

void world_system_worker_construction(match_world_state& world)
{
    world_entity_workers& workers = world.entities->workers;

    for (ui16 workerIndex = 0; workerIndex < workers.max_count; workerIndex++)
    {
        if (!workers.guids[workerIndex].is_valid()) continue;

        // A project can't go on without the entity it is tied to.
        bool finished = !world_entity_is_valid(world, workers.origin_entity[workerIndex]);

        if (!finished)
        {
            switch ((WORK_TYPE)workers.work_types[workerIndex])
            {
            case WORK_TYPE::BUILD_ROAD:
                finished = worker_logic_build_road_tick(world, workerIndex);
                break;
            case WORK_TYPE::BUILD_SETTLEMENT_BUILDING:
                finished = worker_logic_build_settlement_building_tick(world, workerIndex);
                break;
            case WORK_TYPE::FOUND_SETTLEMENT:
                finished = worker_logic_found_settlement_tick(world, workerIndex);
                break;
            default:
                finished = true; // Unknown work type.
                break;
            }
        }

        if (finished)
        {
            world_destroy_worker(world, workers.guids[workerIndex]);
            continue;
        }

        // Increment Time spent working on the project, so it is 0 during the first tick.
        workers.work_times[workerIndex]++;
    }
}
