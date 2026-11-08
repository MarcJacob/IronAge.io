// Strategy layer: picks the goal (target settlement, rally point) and runs the mode state machine.
// Runs on a slow sub-cadence. Tactics read its results from ai_player_state and the mode.
//
// EXPAND is the economy phase (founding, no armies). Armies wait for a big enough population, attacks for supplied armies.
// EXPAND   -> BUILD_UP : a threat showed up, or the population reached AI_BUILD_UP_MIN_POPULATION with enough settlements for the goal (or no site found 3 times).
// BUILD_UP -> ATTACK   : a target exists and our supplied armies reach its attack need.
// BUILD_UP -> EXPAND   : no threat, and the population fell under AI_BUILD_UP_MIN_POPULATION or we are well below the settlement goal with sites still found.
// ATTACK   -> BUILD_UP : target gone, army fell under a third of the need, or the campaign took too long.

#include "match_ai_internal.h"

static void ai_set_mode(ai_context& ctx, AI_MODE mode)
{
	ctx.state.mode = mode;
	ctx.state.mode_start_day = ctx.day;
}

static ui32 ai_expand_goal(const ai_context& ctx)
{
	return ia_min(AI_GOAL_SETTLEMENTS_MAX, AI_GOAL_SETTLEMENTS_START + ctx.day / AI_GOAL_DAYS_PER_SETTLEMENT);
}

// Strength of a player: troops in its settlements' garrisons and armies. Cheap measure, a weaker owner is a better target.
static ui32 ai_player_strength(const ai_context& ctx, match_player_id owner)
{
	ui32 strength = 0;
	ai_settlement_view settlement;
	for (ui16 slot = 0; slot < ai_world_settlement_slots(ctx.world); slot++)
	{
		if (ai_view_settlement(ctx.world, slot, settlement) && settlement.owner == owner) strength += ai_troops_sum(settlement.home_troops);
	}
	ai_army_view army;
	for (ui16 slot = 0; slot < ai_world_army_slots(ctx.world); slot++)
	{
		if (ai_view_army(ctx.world, slot, army) && army.owner == owner) strength += army.troops;
	}
	return strength;
}

struct ai_target_candidate
{
	ai_settlement_view settlement;
	ui32 distance_sq; // To the closest of our settlements, which is the rally point.
	const ai_settlement_view* rally;
};

// Target settlement: any settlement of another player. The AI_TARGET_SHORTLIST closest ones in a straight line (to any of our settlements) are
// scored, the best one wins and our settlement closest to it is the rally point. Score, higher is better:
//   population x AI_TARGET_WEIGHT_POPULATION - owner strength x AI_TARGET_WEIGHT_STRENGTH - route cost x AI_TARGET_WEIGHT_ROUTE_COST
// The route cost comes from a path search from the rally point; no route (or one too long for the search) = not a target.
static void ai_pick_target(ai_context& ctx)
{
	ctx.state.target_settlement = INVALID_ENTITY_GUID;
	ctx.state.rally_settlement = INVALID_ENTITY_GUID;
	if (ctx.settlement_count == 0) return;

	// Shortlist, sorted by straight line distance.
	ai_target_candidate shortlist[AI_TARGET_SHORTLIST] = {};
	ui32 count = 0;
	ai_settlement_view settlement;
	for (ui16 slot = 0; slot < ai_world_settlement_slots(ctx.world); slot++)
	{
		if (!ai_view_settlement(ctx.world, slot, settlement) || settlement.owner == ctx.player) continue;

		const ai_settlement_view* rally = &ctx.settlements[0];
		ui32 distanceSq = 0xFFFFFFFF;
		for (ui16 own = 0; own < ctx.settlement_count; own++)
		{
			ui32 distance = ai_dist_sq(settlement.location, ctx.settlements[own].location);
			if (distance < distanceSq) { distanceSq = distance; rally = &ctx.settlements[own]; }
		}
		if (count == AI_TARGET_SHORTLIST && distanceSq >= shortlist[count - 1].distance_sq) continue;

		ui32 at = count < AI_TARGET_SHORTLIST ? count++ : count - 1;
		for (; at > 0 && shortlist[at - 1].distance_sq > distanceSq; at--) shortlist[at] = shortlist[at - 1];
		shortlist[at].settlement = settlement;
		shortlist[at].distance_sq = distanceSq;
		shortlist[at].rally = rally;
	}

	bool found = false;
	i32 bestScore = 0;
	for (ui32 index = 0; index < count; index++)
	{
		const ai_target_candidate& candidate = shortlist[index];
		ui32 routeCost = ai_road_route_cost(ctx.world, candidate.rally->location, candidate.settlement.location);
		if (routeCost == 0) continue;

		i32 score = (i32)candidate.settlement.population * AI_TARGET_WEIGHT_POPULATION
			- (i32)ai_player_strength(ctx, candidate.settlement.owner) * AI_TARGET_WEIGHT_STRENGTH
			- (i32)routeCost * AI_TARGET_WEIGHT_ROUTE_COST;
		if (found && score <= bestScore) continue;

		found = true;
		bestScore = score;
		ctx.state.target_settlement = candidate.settlement.guid;
		ctx.state.rally_settlement = candidate.rally->guid;
	}
}

static bool ai_target_is_valid(const ai_context& ctx, ai_settlement_view& out_target)
{
	return ai_view_settlement_by_guid(ctx.world, ctx.state.target_settlement, out_target) && out_target.owner != ctx.player;
}

static void ai_strategy_update(ai_context& ctx)
{
	ai_army_view threat;
	bool threatened = ai_find_threat(ctx, threat);

	// The target is kept until it is no longer valid (gone or ours), then re-picked.
	ai_settlement_view target;
	if (!ai_target_is_valid(ctx, target)) ai_pick_target(ctx);
	bool hasTarget = ai_target_is_valid(ctx, target);
	ui32 need = hasTarget ? ai_attack_need(target) : 0xFFFFFFFF;

	ui32 daysInMode = ctx.day - ctx.state.mode_start_day;
	if (ctx.day % AI_EXPAND_RETRY_DAYS == 0) ctx.state.expand_failures = 0; // Try colonizing again now and then.
	bool canExpand = ctx.state.expand_failures < 3;
	bool bigEnough = ctx.population >= AI_BUILD_UP_MIN_POPULATION;
	switch (ctx.state.mode)
	{
	case AI_MODE_EXPAND:
		if (threatened || (bigEnough && (ctx.settlement_count >= ai_expand_goal(ctx) || !canExpand))) ai_set_mode(ctx, AI_MODE_BUILD_UP);
		break;
	case AI_MODE_BUILD_UP:
		if (hasTarget && ctx.army_ready_strength >= need && daysInMode >= AI_BUILD_UP_MIN_DAYS) ai_set_mode(ctx, AI_MODE_ATTACK);
		else if (!threatened && (!bigEnough || (canExpand && ctx.settlement_count + 2 < ai_expand_goal(ctx)))) ai_set_mode(ctx, AI_MODE_EXPAND);
		break;
	case AI_MODE_ATTACK:
		if (!hasTarget || ctx.army_strength < need / 3 || daysInMode > AI_ATTACK_MAX_DAYS) ai_set_mode(ctx, AI_MODE_BUILD_UP);
		break;
	}
}
