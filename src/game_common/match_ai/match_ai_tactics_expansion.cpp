// Expansion tactics: founding settlements on good land, and roads linking our settlements.

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

// Random own settlement with enough peasants to send colonists, or nullptr.
static const ai_settlement_view* ai_pick_colonist_origin(ai_context& ctx)
{
	ui32 start = ai_random(ctx, ctx.settlement_count);
	for (ui32 offset = 0; offset < ctx.settlement_count; offset++)
	{
		const ai_settlement_view& settlement = ctx.settlements[(start + offset) % ctx.settlement_count];
		if (settlement.peasants >= GAME_PARAMETERS.found_settlement_colonists + AI_COLONIST_RESERVE) return &settlement;
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
		if (x < 0 || y < 0 || ia_max(ia_abs(dx), ia_abs(dy)) <= (i32)SETTLEMENT_MIN_DISTANCE) continue;

		world_location site = { (ui16)x, (ui16)y };
		if (!ai_site_is_valid(ctx.world, site, ctx.player)) continue;

		i32 landScore = ai_site_land_score(ctx.world, site);
		if (landScore < 12) continue;
		if (ai_nearest_foreign_dist_sq(ctx, site) < AI_MIN_SITE_DISTANCE_ENEMY * AI_MIN_SITE_DISTANCE_ENEMY) continue;

		i32 score = landScore * 2 - (ia_abs(dx) + ia_abs(dy)) / 2 + (ai_site_is_owned(ctx.world, site) ? 0 : 6);
		if (score > bestScore) { bestScore = score; out_site = site; }
	}
	return bestScore > 0;
}

// While below the settlement goal, sends colonists to a new site. One expedition at a time. Counts failed searches for the state machine.
static void ai_tactics_found_settlement(ai_context& ctx)
{
	if (ctx.state.mode == AI_MODE_ATTACK || ctx.settlement_count >= ai_expand_goal(ctx) || ctx.state.expand_failures >= 3) return;
	if (!ai_world_has_room_to_found(ctx.world)) return;
	if (ai_count_projects(ctx.world, WORK_TYPE::FOUND_SETTLEMENT, ctx.player, INVALID_ENTITY_GUID) > 0) return;

	const ai_settlement_view* origin = ai_pick_colonist_origin(ctx);
	if (origin == nullptr) return; // Not enough peasants anywhere yet: not a failure.

	world_location site;
	if (!ai_find_site(ctx, *origin, site)) { ctx.state.expand_failures++; return; }

	auto* found = ai_push<command_data_found_settlement>(ctx, MATCH_COMMAND_TYPE::FOUND_SETTLEMENT);
	if (found == nullptr) return;
	found->settlement = origin->guid;
	found->target = site;
	ctx.state.expand_failures = 0;
}

// Links a settlement that has no road next to it to its closest own settlement (one road project at a time, when the treasury can pay the crew).
static void ai_tactics_roads(ai_context& ctx)
{
	if (ctx.treasury < AI_MIN_ROAD_TREASURY || ctx.settlement_count < 2) return;
	if (ai_count_projects(ctx.world, WORK_TYPE::BUILD_ROAD, ctx.player, INVALID_ENTITY_GUID) > 0) return;

	for (ui32 index = 0; index < ai_window_size(ctx); index++)
	{
		const ai_settlement_view& settlement = ai_window_settlement(ctx, index);
		if (ai_has_road_next_to(ctx.world, settlement.location)) continue;

		const ai_settlement_view* nearest = nullptr;
		ui32 nearestDist = AI_MAX_ROAD_LENGTH * AI_MAX_ROAD_LENGTH;
		for (ui16 other = 0; other < ctx.settlement_count; other++)
		{
			ui32 distance = ai_dist_sq(settlement.location, ctx.settlements[other].location);
			if (!(ctx.settlements[other].guid == settlement.guid) && distance < nearestDist) { nearestDist = distance; nearest = &ctx.settlements[other]; }
		}
		if (nearest == nullptr) continue;

		auto* road = ai_push<command_data_build_road>(ctx, MATCH_COMMAND_TYPE::BUILD_ROAD);
		if (road == nullptr) return;
		road->settlement = settlement.guid;
		road->origin = settlement.location;
		road->destination = nearest->location;
		return;
	}
}
