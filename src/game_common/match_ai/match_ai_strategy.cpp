// Strategy layer: picks the goal (target player, target settlement, rally point) and runs the mode state machine.
// Runs on a slow sub-cadence. Tactics read its results from ai_player_state and the mode.
//
// EXPAND   -> BUILD_UP : enough settlements for the goal, or no site found 3 times, or a threat showed up.
// BUILD_UP -> ATTACK   : a target exists and our armies reach its attack need.
// BUILD_UP -> EXPAND   : well below the settlement goal, no threat, sites still found.
// ATTACK   -> BUILD_UP : target gone, army fell under a third of the need, or the campaign took too long.

#include "match_ai_internal.h"

static void ai_set_mode(ai_context& ctx, AI_MODE mode)
{
	ctx.state.mode = mode;
	ctx.state.mode_start_day = ctx.day;
}

static ui32 ai_expand_goal(const ai_context& ctx)
{
	return ia_min(AI_GOAL_SETTLEMENTS_MAX, 3 + ctx.day / AI_GOAL_DAYS_PER_SETTLEMENT);
}

// Target player: the hostile-est (lowest opinion) one with a settlement within campaign distance of ours.
static void ai_pick_target_player(ai_context& ctx, const ai_player_scan& scan)
{
	ctx.state.has_target = 0;
	i32 bestOpinion = 0x7FFFFFFF;
	for (ui8 player = 0; player < AI_MAX_TRACKED_PLAYERS; player++)
	{
		if (scan.settlements[player] == 0 || scan.nearest_dist_sq[player] > AI_CAMPAIGN_MAX_DISTANCE * AI_CAMPAIGN_MAX_DISTANCE) continue;
		if (ctx.state.opinions[player] >= bestOpinion) continue;

		bestOpinion = ctx.state.opinions[player];
		ctx.state.target_player = player;
		ctx.state.has_target = 1;
	}
}

// Target settlement: the weakest one of the target player near our land (score = 4 x defense + distance), and our closest settlement to it as rally point.
static void ai_pick_target_settlement(ai_context& ctx)
{
	ctx.state.target_settlement = INVALID_ENTITY_GUID;
	ctx.state.rally_settlement = INVALID_ENTITY_GUID;
	if (!ctx.state.has_target || ctx.settlement_count == 0) return;

	ui32 bestScore = 0xFFFFFFFF;
	ai_settlement_view settlement;
	for (ui16 slot = 0; slot < ai_world_settlement_slots(ctx.world); slot++)
	{
		if (!ai_view_settlement(ctx.world, slot, settlement) || settlement.owner != ctx.state.target_player) continue;

		const ai_settlement_view* rally = &ctx.settlements[0];
		ui32 rallyDist = 0xFFFFFFFF;
		for (ui16 own = 0; own < ctx.settlement_count; own++)
		{
			ui32 distance = ai_dist_sq(settlement.location, ctx.settlements[own].location);
			if (distance < rallyDist) { rallyDist = distance; rally = &ctx.settlements[own]; }
		}
		if (rallyDist > AI_CAMPAIGN_MAX_DISTANCE * AI_CAMPAIGN_MAX_DISTANCE) continue;

		ui32 manhattan = (ui32)(ia_abs((i32)settlement.location.x - rally->location.x) + ia_abs((i32)settlement.location.y - rally->location.y));
		ui32 score = ai_estimate_defense(settlement) * 4 + manhattan;
		if (score >= bestScore) continue;

		bestScore = score;
		ctx.state.target_settlement = settlement.guid;
		ctx.state.rally_settlement = rally->guid;
	}
}

static bool ai_target_is_valid(const ai_context& ctx, ai_settlement_view& out_target)
{
	return ai_view_settlement_by_guid(ctx.world, ctx.state.target_settlement, out_target) && out_target.owner != ctx.player;
}

static void ai_strategy_update(ai_context& ctx)
{
	ai_player_scan scan = {};
	ai_scan_players(ctx, scan);

	ai_army_view threat;
	bool threatened = ai_find_threat(ctx, threat);

	ui32 ownTroops = ctx.army_strength;
	for (ui16 own = 0; own < ctx.settlement_count; own++) ownTroops += ai_troops_sum(ctx.settlements[own].home_troops);
	ai_update_opinions(ctx, scan, ownTroops, threatened ? threat.owner : INVALID_MATCH_PLAYER_ID);

	// A campaign keeps its target until it is over; otherwise the target is re-picked.
	ai_settlement_view target;
	if (ctx.state.mode != AI_MODE_ATTACK || !ai_target_is_valid(ctx, target))
	{
		ai_pick_target_player(ctx, scan);
		ai_pick_target_settlement(ctx);
	}
	bool hasTarget = ai_target_is_valid(ctx, target);
	ui32 need = hasTarget ? ai_attack_need(target) : 0xFFFFFFFF;

	ui32 daysInMode = ctx.day - ctx.state.mode_start_day;
	bool canExpand = ctx.state.expand_failures < 3;
	switch (ctx.state.mode)
	{
	case AI_MODE_EXPAND:
		if (threatened || ctx.settlement_count >= ai_expand_goal(ctx) || !canExpand) ai_set_mode(ctx, AI_MODE_BUILD_UP);
		break;
	case AI_MODE_BUILD_UP:
		if (daysInMode >= 20) ctx.state.expand_failures = 0; // Try colonizing again now and then.
		if (hasTarget && ctx.army_strength >= need && daysInMode >= AI_BUILD_UP_MIN_DAYS) ai_set_mode(ctx, AI_MODE_ATTACK);
		else if (!threatened && canExpand && ctx.settlement_count + 2 < ai_expand_goal(ctx)) ai_set_mode(ctx, AI_MODE_EXPAND);
		break;
	case AI_MODE_ATTACK:
		if (!hasTarget || ctx.army_strength < need / 3 || daysInMode > AI_ATTACK_MAX_DAYS) ai_set_mode(ctx, AI_MODE_BUILD_UP);
		break;
	}
}
