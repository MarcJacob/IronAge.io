// Expansion tactics: founding settlements on good land, and road spokes raising the land output of our settlements.

#include "game_common/match/game_parameters.h"
#include "match_ai_internal.h"

constexpr i32 AI_SITE_RANGE = 34; // Candidate sites are within this many tiles of the origin settlement (on each axis).

// Squared distance from a tile to the closest settlement owned by someone else.
static ui32 ai_nearest_foreign_dist_sq(const ai_context& ctx, world_location loc)
{
	ui32 nearest = 0xFFFFFFFF;
	ai_settlement_view settlement;
	for (ui16 slot = 0; slot < ai_world_settlement_slots(ctx.world); slot++)
	{
		if (ai_view_settlement(ctx.world, slot, settlement) && settlement.owner != ctx.player) nearest = ia_min(nearest, ai_dist_sq(loc, settlement.location));
	}
	return nearest;
}

// Random own settlement big enough to send colonists, or nullptr.
static const ai_settlement_view* ai_pick_colonist_origin(ai_context& ctx)
{
	ui32 start = ai_random(ctx, ctx.settlement_count);
	for (ui32 offset = 0; offset < ctx.settlement_count; offset++)
	{
		const ai_settlement_view& settlement = ctx.settlements[(start + offset) % ctx.settlement_count];
		if (settlement.population >= AI_COLONIST_MIN_POPULATION && settlement.peasants >= GAME_PARAMETERS.found_settlement_colonists + AI_COLONIST_RESERVE) return &settlement;
	}
	return nullptr;
}

// Samples AI_SITE_SAMPLES random tiles around the origin and returns the best valid one (good land around, near us, on free land, away from foreigners).
static bool ai_find_site(ai_context& ctx, const ai_settlement_view& origin, world_location& out_site)
{
	i32 bestScore = 0;
	for (ui32 sample = 0; sample < AI_SITE_SAMPLES; sample++)
	{
		i32 dx = (i32)ai_random(ctx, 2 * AI_SITE_RANGE + 1) - AI_SITE_RANGE;
		i32 dy = (i32)ai_random(ctx, 2 * AI_SITE_RANGE + 1) - AI_SITE_RANGE;
		i32 x = origin.location.x + dx, y = origin.location.y + dy;
		if (x < 0 || y < 0 || ia_max(ia_abs(dx), ia_abs(dy)) <= (i32)GAME_PARAMETERS.minimum_settlement_distance) continue;

		world_location site = { (ui16)x, (ui16)y };
		if (!ai_site_is_valid(ctx.world, site, ctx.player)) continue;

		i32 landScore = ai_site_land_score(ctx.world, site);
		if (landScore < AI_SITE_MIN_LAND_SCORE) continue;
		if (ai_nearest_foreign_dist_sq(ctx, site) < AI_MIN_SITE_DISTANCE_ENEMY * AI_MIN_SITE_DISTANCE_ENEMY) continue;

		i32 score = landScore * 2 - (ia_abs(dx) + ia_abs(dy)) / 2 + (ai_site_is_owned(ctx.world, site) ? 0 : 6);
		if (score > bestScore) { bestScore = score; out_site = site; }
	}
	return bestScore > 0;
}

// In EXPAND, while below the settlement goal, sends colonists to a new site. One expedition at a time. Counts failed searches for the state machine.
static void ai_tactics_found_settlement(ai_context& ctx)
{
	if (ctx.state.mode != AI_MODE_EXPAND || ctx.settlement_count >= ai_expand_goal(ctx) || ctx.state.expand_failures >= 3) return;
	if (!ai_world_has_room_to_found(ctx.world)) return;
	if (ai_count_projects(ctx.world, WORK_TYPE::FOUND_SETTLEMENT, ctx.player, INVALID_ENTITY_GUID) > 0) return;

	const ai_settlement_view* origin = ai_pick_colonist_origin(ctx);
	if (origin == nullptr) return; // No settlement big enough yet: not a failure.

	world_location site;
	if (!ai_find_site(ctx, *origin, site)) { ctx.state.expand_failures++; return; }

	auto* found = ai_push<command_data_found_settlement>(ctx, MATCH_COMMAND_TYPE::FOUND_SETTLEMENT);
	if (found == nullptr) return;
	found->settlement = origin->guid;
	found->target = site;
	ctx.state.expand_failures = 0;
}

// Road spokes: road tiles lower the travel distance of the land they cross, so it produces (the only way to raise a settlement's land output;
// output falls to 0 past about 5 road tiles). A settlement whose peasants work most of its land output gets a straight road to its first
// missing spoke tip: 8 directions, tip on buildable land in its own territory. One crew per settlement, 1 + income / AI_INCOME_PER_EXTRA_ROAD overall.
static void ai_tactics_roads(ai_context& ctx)
{
	static constexpr i32 DIRECTIONS[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } }; // Straight ones first.
	if (ctx.treasury < AI_MIN_ROAD_TREASURY) return;

	ui32 crews = ai_count_projects(ctx.world, WORK_TYPE::BUILD_ROAD, ctx.player, INVALID_ENTITY_GUID);
	ui32 maxCrews = 1 + ctx.income / AI_INCOME_PER_EXTRA_ROAD;
	for (ui32 index = 0; index < ai_window_size(ctx) && crews < maxCrews; index++)
	{
		const ai_settlement_view& settlement = ai_window_settlement(ctx, index);
		if (settlement.peasants * 100 < settlement.peasant_jobs * AI_ROAD_LAND_USE_PERCENT) continue;
		if (ai_count_projects(ctx.world, WORK_TYPE::BUILD_ROAD, ctx.player, settlement.guid) > 0) continue;

		for (ui32 direction = 0; direction < 8; direction++)
		{
			i32 length = direction < 4 ? AI_SPOKE_TILES_STRAIGHT : AI_SPOKE_TILES_DIAGONAL;
			i32 x = settlement.location.x + DIRECTIONS[direction][0] * length, y = settlement.location.y + DIRECTIONS[direction][1] * length;
			if (x < 0 || y < 0) continue;

			world_location tip = { (ui16)x, (ui16)y };
			if (ai_tile_is_road(ctx.world, tip) || !ai_tile_is_buildable(ctx.world, tip) || !ai_tile_owned_by(ctx.world, tip, settlement.guid)) continue;

			auto* road = ai_push<command_data_build_road>(ctx, MATCH_COMMAND_TYPE::BUILD_ROAD);
			if (road == nullptr) return;
			road->settlement = settlement.guid;
			road->origin = settlement.location;
			road->destination = tip;
			crews++;
			break;
		}
	}
}
