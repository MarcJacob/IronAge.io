// World access of the match AI: the ONLY file reading the world state. Everything else works with the views and queries below.
// All of it is read-only. Keeping the reads here is what lets the AI later run parallel to the world tick (or on a copy).

#include "match_ai_internal.h"
#include "game_common/match/world/world_terrain.h"

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
	const world_entity_settlements& settlements = world.entities->settlements;
	if (slot >= settlements.max_count || !settlements.guids[slot].is_valid()) return false;

	const settlement_population& population = settlements.populations[slot];
	out.guid = settlements.guids[slot];
	out.owner = settlements.owners[slot];
	out.location = settlements.locations[slot];
	out.population = population.population_total;
	out.peasants = population.peasants;
	out.workers = population.workers;
	out.local_wealth = settlements.economies[slot].income;
	out.growth_per_mille = population.growth_per_mille;
	out.tax_rate = settlements.tax_rates[slot];
	out.home_troops = population.home;
	out.barracks = settlements.buildings[slot].building_counts[0];
	out.caravanserai = settlements.buildings[slot].building_counts[1];
	out.maa_ceiling = settlements.troop_ceilings[slot].men_at_arms;
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
	out.idle = !out.raising && !armies.battles[slot].is_valid() && !armies.action_targets[slot].is_valid()
		&& armies.locations[slot] == armies.movements[slot].destination;
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
		if (settlement.is_valid() ? origin == settlement : world.entities->settlements.owners[origin.get_index()] == player) count++;
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
    case TERRAIN_TILE_TYPE::WATER_COAST: return 5;
	case TERRAIN_TILE_TYPE::LAND_PLAINS: return 3;
	case TERRAIN_TILE_TYPE::LAND_HILLS: return 2;
	default: return 0;
	}
}

// How good the land around a tile is: the tile counts double, plus 8 probes around it.
static i32 ai_site_land_score(const match_world_state& world, world_location loc)
{
	i32 score = 2 * ai_tile_land_value(world, loc.x, loc.y);
	for (i32 dy = -1; dy <= 1; dy++)
	{
		for (i32 dx = -1; dx <= 1; dx++)
		{
			if (dx != 0 || dy != 0) score += ai_tile_land_value(world, loc.x + dx * 7, loc.y + dy * 7);
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

static bool ai_has_road_next_to(const match_world_state& world, world_location loc)
{
	for (i32 dy = -1; dy <= 1; dy++)
	{
		for (i32 dx = -1; dx <= 1; dx++)
		{
			if ((dx == 0 && dy == 0) || !ai_in_world(world, loc.x + dx, loc.y + dy)) continue;

			ui16 regionIndex, tileIndex;
			world_get_tile(*world.terrain, { (ui16)(loc.x + dx), (ui16)(loc.y + dy) }, regionIndex, tileIndex);
			if (world.terrain->regions[regionIndex].status[tileIndex].is_road) return true;
		}
	}
	return false;
}

// Fills the context's owned settlements and armies (up to the caps), treasury and army strength.
static void ai_gather_own(ai_context& ctx)
{
	const match_world_state& world = ctx.world;
	ctx.day = ai_world_day(world);
	ctx.treasury = ctx.player < world.player_count ? world.player_states[ctx.player].treasury : 0;

	ai_settlement_view settlement;
	for (ui16 slot = 0; slot < ai_world_settlement_slots(world) && ctx.settlement_count < AI_MAX_OWNED_SETTLEMENTS; slot++)
	{
		if (ai_view_settlement(world, slot, settlement) && settlement.owner == ctx.player) ctx.settlements[ctx.settlement_count++] = settlement;
	}

	ai_army_view army;
	for (ui16 slot = 0; slot < ai_world_army_slots(world) && ctx.army_count < AI_MAX_OWNED_ARMIES; slot++)
	{
		if (!ai_view_army(world, slot, army) || army.owner != ctx.player) continue;

		ctx.armies[ctx.army_count++] = army;
		ctx.army_strength += army.troops_target;
	}
}
