// Expansion tactics: founding settlements on good land, roads joining our settlements into one network, and road spokes raising their land output.

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

// Own settlement with this guid, or nullptr (gone, or no longer ours).
static const ai_settlement_view* ai_own_settlement(const ai_context& ctx, entity_guid guid)
{
	for (ui16 index = 0; index < ctx.settlement_count; index++)
	{
		if (ctx.settlements[index].guid == guid) return &ctx.settlements[index];
	}
	return nullptr;
}

// One step of the road network sweep over the pairs of own settlements, a few pairs per run. Two settlements joined by a road-only path are in the
// same network (roads of any owner count: everybody can use them). For the other pairs the route a crew would build is searched, existing roads
// counting at their low cost, and the cheapest pair is kept. Once every pair was looked at, it becomes the link to build and a new sweep starts.
// Pairs without a land route (water, or too far for a search) are skipped: they stay separate networks.
static void ai_road_link_sweep(ai_context& ctx)
{
	ai_player_state& state = ctx.state;
	ui32 searches = 0;
	for (ui32 checked = 0; checked < AI_ROAD_LINK_PAIRS_PER_TACTICS && searches < AI_ROAD_LINK_SEARCHES_PER_TACTICS; checked++)
	{
		if (state.link_sweep_second <= state.link_sweep_first) state.link_sweep_second = (ui16)(state.link_sweep_first + 1);
		if (state.link_sweep_second >= ctx.settlement_count)
		{
			state.link_sweep_first++;
			state.link_sweep_second = (ui16)(state.link_sweep_first + 1);
		}
		if (state.link_sweep_second >= ctx.settlement_count)
		{
			// Sweep done.
			if (state.link_best_cost != 0)
			{
				state.link_a = state.link_best_a;
				state.link_b = state.link_best_b;
				state.link_started = 0;
			}
			state.link_sweep_first = 0;
			state.link_sweep_second = 1;
			state.link_best_cost = 0;
			return;
		}

		const ai_settlement_view& a = ctx.settlements[state.link_sweep_first];
		const ai_settlement_view& b = ctx.settlements[state.link_sweep_second];
		state.link_sweep_second++;
		if (ai_road_connected(ctx.world, a.location, b.location)) continue;

		searches++;
		ui32 cost = ai_road_route_cost(ctx.world, a.location, b.location);
		if (cost == 0 || (state.link_best_cost != 0 && cost >= state.link_best_cost)) continue;

		state.link_best_cost = cost;
		state.link_best_a = a.guid;
		state.link_best_b = b.guid;
	}
}

// Road network: joins all own settlements by roads, one link at a time (cheapest pair first, see ai_road_link_sweep). A link gets a crew from each
// end heading for the other: both follow the same route (crews search it the same way from either end), so they meet halfway. A link is done when
// its crews are gone; the next sweep then checks whether it joined the networks.
static void ai_road_link(ai_context& ctx, ui32& crews, ui32 max_crews)
{
	ai_player_state& state = ctx.state;
	if (!state.link_a.is_valid()) ai_road_link_sweep(ctx);
	if (!state.link_a.is_valid()) return;

	const ai_settlement_view* a = ai_own_settlement(ctx, state.link_a);
	const ai_settlement_view* b = ai_own_settlement(ctx, state.link_b);
	bool done = a == nullptr || b == nullptr;
	if (!done && state.link_started)
	{
		done = ai_count_projects(ctx.world, WORK_TYPE::BUILD_ROAD, ctx.player, a->guid) == 0
			&& ai_count_projects(ctx.world, WORK_TYPE::BUILD_ROAD, ctx.player, b->guid) == 0;
	}
	if (!done && !state.link_started) done = ai_road_connected(ctx.world, a->location, b->location); // Joined since it was picked.
	if (done)
	{
		state.link_a = INVALID_ENTITY_GUID;
		state.link_b = INVALID_ENTITY_GUID;
		state.link_started = 0;
		return;
	}
	if (state.link_started) return;

	// Not started: a crew from each end that has no road crew of its own, while the crew budget allows.
	const ai_settlement_view* ends[2][2] = { { a, b }, { b, a } };
	for (ui32 end = 0; end < 2 && crews < max_crews; end++)
	{
		const ai_settlement_view& from = *ends[end][0];
		if (ai_count_projects(ctx.world, WORK_TYPE::BUILD_ROAD, ctx.player, from.guid) > 0) continue;

		auto* road = ai_push<command_data_build_road>(ctx, MATCH_COMMAND_TYPE::BUILD_ROAD);
		if (road == nullptr) return;
		road->settlement = from.guid;
		road->origin = from.location;
		road->destination = ends[end][1]->location;
		crews++;
		state.link_started = 1;
	}
}

// Roads: the network link first (ai_road_link), then road spokes with the crews left.
// Road spokes: road tiles lower the travel distance of the land they cross, so it produces (the only way to raise a settlement's land output;
// output falls to 0 past about 5 road tiles). A settlement whose peasants work most of its land output gets a straight road to its first
// missing spoke tip: 8 directions, tip on buildable land in its own territory. One crew per settlement, 1 + income / AI_INCOME_PER_EXTRA_ROAD overall,
// minus one kept for a link waiting for its crews.
static void ai_tactics_roads(ai_context& ctx)
{
	static constexpr i32 DIRECTIONS[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } }; // Straight ones first.
	if (ctx.treasury < AI_MIN_ROAD_TREASURY) return;

	ui32 crews = ai_count_projects(ctx.world, WORK_TYPE::BUILD_ROAD, ctx.player, INVALID_ENTITY_GUID);
	ui32 maxCrews = 1 + ctx.income / AI_INCOME_PER_EXTRA_ROAD;
	if (ctx.settlement_count >= 2) ai_road_link(ctx, crews, maxCrews);
	if (ctx.state.link_a.is_valid() && !ctx.state.link_started) maxCrews--;

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
