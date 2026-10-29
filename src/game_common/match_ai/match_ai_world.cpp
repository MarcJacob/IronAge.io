// World access of the match AI: the ONLY file reading the world state. Everything else works with the views and queries below.
// All of it is read-only. Keeping the reads here is what lets the AI later run parallel to the world tick (or on a copy).


#include "game_common/match/world/entities/world_entity_settlements.h"
#include "match_ai_internal.h"
#include "game_common/match/world/world_terrain.h"
#include "game_common/match/world/world_state.h"
#include "game_common/match/world/world_pathing.h"

static ui32 ai_world_player_count(const match_world_state& world) { return world.player_count; }
static ui32 ai_world_day(const match_world_state& world) { return (ui32)(world.age / GAME_PARAMETERS.ticks_per_day); }
static ui64 ai_world_age(const match_world_state& world) { return world.age; }
static ui16 ai_world_settlement_slots(const match_world_state& world) { return world.entities->settlements.max_count; }
static ui16 ai_world_army_slots(const match_world_state& world) { return world.entities->armies.max_count; }
static bool ai_world_has_room_to_found(const match_world_state& world)
{
	return world.entities->settlements.active_count < world.entities->settlements.max_count && world.entities->workers.active_count < world.entities->workers.max_count;
}
static world_dimensions ai_world_size(const match_world_state& world) { return world.terrain->size_tiles; }
static ui32 ai_world_troops_daily_consumption(const army_troop_count& troops) { return army_daily_consumption(troops); }

static bool ai_view_settlement(const match_world_state& world, ui16 slot, ai_settlement_view& out)
{
	if (slot >= world.entities->settlements.max_count || !world.entities->settlements.guids[slot].is_valid()) 
        return false;

	const world_entity_settlement_view settlement { world.entities->settlements, slot };

	out.guid = settlement.guid;             
	out.owner = settlement.owner;
	out.location = settlement.location;
	out.population = settlement.pop_total;
	out.peasants = settlement.pop_peasants;
	out.workers = settlement.pop_workers;
	out.local_wealth = settlement.trade.market_goods;
	out.peasant_jobs = settlement.pop_peasants_cap;
	out.growth_per_mille = settlement.pop_growth_per_mille;
	out.tax_rate = settlement.tax_rate;
	out.home_troops = settlement.garrison;
	out.urban_pop = settlement.urban_pop();
	out.urban_cap = settlement.pop_urban_cap;
	out.barracks = settlement.buildings.building_counts[AI_BUILDING_BARRACKS];
	out.caravanserai = settlement.buildings.building_counts[AI_BUILDING_CARAVANSERAI];
	out.trade_depots = settlement.buildings.building_counts[AI_BUILDING_TRADE_DEPOT];
	out.maa_ceiling = settlement.troop_ceilings.men_at_arms;
	return true;
}

static bool ai_view_settlement_by_guid(const match_world_state& world, entity_guid guid, ai_settlement_view& out)
{
	if (guid.get_type() != ENTITY_TYPE::SETTLEMENT || !world_entity_is_valid(world, guid)) return false;
	return ai_view_settlement(world, guid.get_index(), out);
}

static bool ai_view_army(const match_world_state& world, ui16 slot, ai_army_view& out)
{
	const world_entity_armies& armies = world.entities->armies;
	if (slot >= armies.max_count || !armies.guids[slot].is_valid()) return false;

	out.guid = armies.guids[slot];
	out.home = armies.home_settlements[slot];
	out.owner = armies.owners[slot];
	out.location = armies.locations[slot];
	out.troops = ai_troops_sum(armies.compositions[slot]);
	out.raising = armies.raising[slot];
	out.troops_target = out.raising ? ia_max(out.troops, ai_troops_sum(armies.raised_troops[slot])) : out.troops;
	out.supply_days = armies.supplies[slot] / ia_max(army_daily_consumption(armies.compositions[slot]), (ui32)1);
	out.idle = !out.raising && !armies.battles[slot].is_valid() && !armies.action_targets[slot].is_valid()
		&& armies.locations[slot] == armies.movements[slot].destination;
	return true;
}

static ui16 ai_world_battle_slots(const match_world_state& world) { return world.entities->battles.max_count; }

static bool ai_view_idle_siege(const match_world_state& world, ui16 slot, ai_siege_view& out)
{
	const world_entity_battles& battles = world.entities->battles;
	const world_entity_armies& armies = world.entities->armies;
	if (slot >= battles.max_count || !battles.guids[slot].is_valid()) return false;

	const world_entity_battles::state& battle = battles.states[slot];
	if (battle.kind != world_entity_battles::KIND_SIEGE || battle.phase != world_entity_battles::PHASE_BESIEGING) return false;

	out.guid = battles.guids[slot];
	out.attacker_commander = battle.attacker.commander;
	out.attacker_men = 0;
	out.defender_men = 0;
	out.min_supply_days = 0xFFFFFFFF;
	for (ui8 unitType = 0; unitType < world_entity_battles::UNIT_TYPES; unitType++)
	{
		out.attacker_men += battle.attacker.men[unitType];
		out.defender_men += battle.defender.men[unitType];
	}
	for (ui16 army = 0; army < armies.max_count; army++)
	{
		if (!armies.guids[army].is_valid() || armies.battles[army]._value != out.guid._value || armies.battle_sides[army] != world_entity_battles::SIDE_ATTACKER) continue;
		out.min_supply_days = ia_min(out.min_supply_days, armies.supplies[army] / ia_max(army_daily_consumption(armies.compositions[army]), (ui32)1));
	}
	return true;
}

// Counts the work projects of a type tied to the player's settlements (or to one settlement).
static ui16 ai_count_projects(const match_world_state& world, WORK_TYPE type, match_player_id player, entity_guid settlement)
{
	const world_entity_workers& workers = world.entities->workers;
	ui16 count = 0;
	for (ui16 slot = 0; slot < workers.max_count; slot++)
	{
		if (!workers.guids[slot].is_valid() || workers.work_types[slot] != type) continue;

		entity_guid origin = workers.origin_entity[slot];
		if (!world_entity_is_valid(world, origin)) continue;
		if (settlement.is_valid() ? origin == settlement : world_entity_get_owner(world, origin) == player) count++;
	}
	return count;
}

static bool ai_in_world(const match_world_state& world, i32 x, i32 y)
{
	return x >= 0 && y >= 0 && x < world.terrain->size_tiles.x && y < world.terrain->size_tiles.y;
}

// The validity rule of the FOUND_SETTLEMENT command: buildable terrain, not foreign land, far enough from every settlement.
static bool ai_site_is_valid(const match_world_state& world, world_location loc, match_player_id player)
{
	return ai_in_world(world, loc.x, loc.y) && world_is_valid_settlement_location(world, loc, player);
}

static i32 ai_tile_land_value(const match_world_state& world, i32 x, i32 y)
{
	if (!ai_in_world(world, x, y)) return 0;

	ui16 regionIndex, tileIndex;
	world_get_tile(*world.terrain, { (ui16)x, (ui16)y }, regionIndex, tileIndex);
	switch (world.terrain->regions[regionIndex].terrain_types[tileIndex])
	{
	case TERRAIN_TILE_TYPE::WATER_COAST: return 6;
	case TERRAIN_TILE_TYPE::LAND_PLAINS: return 5;
	case TERRAIN_TILE_TYPE::LAND_HILLS: return 4;
	default: return 1;
	}
}

// How good the land of a site is. Only the tile and its neighbours produce (land output falls to 0 two tiles out), unless road spokes
// are built (ai_tactics_roads): so the tile counts double, plus its 8 neighbours, plus 5 per buildable tile where its 8 spokes would go.
static i32 ai_site_land_score(const match_world_state& world, world_location loc)
{
	i32 score = 2 * ai_tile_land_value(world, loc.x, loc.y);
	for (i32 dy = -1; dy <= 1; dy++)
	{
		for (i32 dx = -1; dx <= 1; dx++)
		{
			if (dx == 0 && dy == 0) continue;

			score += ai_tile_land_value(world, loc.x + dx, loc.y + dy);
			i32 x = loc.x + dx * AI_SITE_SPOKE_PROBE, y = loc.y + dy * AI_SITE_SPOKE_PROBE;
			if (x >= 0 && y >= 0 && ai_tile_is_buildable(world, { (ui16)x, (ui16)y })) score += 5;
		}
	}
	return score;
}

static bool ai_site_is_owned(const match_world_state& world, world_location loc)
{
	ui16 regionIndex, tileIndex;
	world_get_tile(*world.terrain, loc, regionIndex, tileIndex);
	return world.terrain->regions[regionIndex].owners[tileIndex] != world_terrain::UNOWNED_TILE;
}

static bool ai_tile_is_road(const match_world_state& world, world_location loc)
{
	if (!ai_in_world(world, loc.x, loc.y)) return false;

	ui16 regionIndex, tileIndex;
	world_get_tile(*world.terrain, loc, regionIndex, tileIndex);
	return world.terrain->regions[regionIndex].status[tileIndex].is_road;
}

static bool ai_tile_is_buildable(const match_world_state& world, world_location loc)
{
	if (!ai_in_world(world, loc.x, loc.y)) return false;

	ui16 regionIndex, tileIndex;
	world_get_tile(*world.terrain, loc, regionIndex, tileIndex);
	TERRAIN_TILE_TYPE type = world.terrain->regions[regionIndex].terrain_types[tileIndex];
	return type == TERRAIN_TILE_TYPE::LAND_PLAINS || type == TERRAIN_TILE_TYPE::LAND_HILLS;
}

static bool ai_tile_owned_by(const match_world_state& world, world_location loc, entity_guid settlement)
{
	if (!ai_in_world(world, loc.x, loc.y)) return false;

	ui16 regionIndex, tileIndex;
	world_get_tile(*world.terrain, loc, regionIndex, tileIndex);
	return world.terrain->regions[regionIndex].owners[tileIndex] == settlement.get_index();
}

// Path search memory, shared by all AIs: the server runs ai_update for one AI at a time on its one thread. NOT thread safe (one buffer per thread if AIs ever run in parallel).
alignas(8) static ui8 AI_PATH_SCRATCH[AI_PATH_SCRATCH_SIZE];
static world_location AI_PATH_POINTS[AI_PATH_MAX_WAYPOINTS];

static path ai_path_find(const match_world_state& world, world_location start, world_location end, const path_params& params)
{
	mem_arena scratch = mem_arena_create(AI_PATH_SCRATCH, sizeof(AI_PATH_SCRATCH));
	path found = {};
	found.points = AI_PATH_POINTS;
	found.points_capacity = AI_PATH_MAX_WAYPOINTS;
	world_path_find(*world.terrain, start, end, params, scratch, found);
	return found;
}

static bool ai_road_connected(const match_world_state& world, world_location a, world_location b)
{
	if (a == b) return true;

	path_params params = {};
	params.passable_terrain = PATH_TERRAIN_LAND;
	params.roads_only = true;
	path found = ai_path_find(world, a, b, params);
	return found.count > 0 && !found.partial;
}

static ui32 ai_road_route_cost(const match_world_state& world, world_location a, world_location b)
{
	// The road crews' params (world_system_worker_construction.cpp): same route, and a route the crews can't find fails here too.
	path_params params = {};
	params.passable_terrain = PATH_TERRAIN_LAND;
	params.require_complete = true;
	path found = ai_path_find(world, a, b, params);
	return found.count > 0 ? found.cost : 0;
}

// Fills the context's owned settlements and armies (up to the caps), treasury, income, population and army strengths.
static void ai_gather_own(ai_context& ctx)
{
	const match_world_state& world = ctx.world;
	ctx.day = ai_world_day(world);
	ctx.treasury = ctx.player < world.player_count ? world.player_states[ctx.player].treasury : 0;
	ctx.income = ctx.player < world.player_count ? world.player_states[ctx.player].income : 0;

	ai_settlement_view settlement;
	for (ui16 slot = 0; slot < ai_world_settlement_slots(world) && ctx.settlement_count < AI_MAX_OWNED_SETTLEMENTS; slot++)
	{
		if (!ai_view_settlement(world, slot, settlement) || settlement.owner != ctx.player) continue;

		ctx.settlements[ctx.settlement_count++] = settlement;
		ctx.population += settlement.population;
	}

	ai_army_view army;
	for (ui16 slot = 0; slot < ai_world_army_slots(world) && ctx.army_count < AI_MAX_OWNED_ARMIES; slot++)
	{
		if (!ai_view_army(world, slot, army) || army.owner != ctx.player) continue;

		ctx.armies[ctx.army_count++] = army;
		ctx.army_strength += army.troops_target;
		if (!army.raising && army.supply_days >= AI_ATTACK_MIN_SUPPLY_DAYS) ctx.army_ready_strength += army.troops;
	}
}
