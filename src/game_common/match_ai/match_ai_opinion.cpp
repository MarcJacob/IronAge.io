// Opinions: a primitive i16 per other player (negative = hostile, the preferred target; positive = friendly or feared; 0 = neutral).
// Fed by what can be seen in the world each strategy run.

#include "match_ai_internal.h"

// Gathers per other player figures: settlement count, troops (armies and home pools), distance to our closest settlement.
// Cost: foreign settlements x owned settlements, plus the foreign armies.
static void ai_scan_players(const ai_context& ctx, ai_player_scan& out)
{
	for (ui8 player = 0; player < AI_MAX_TRACKED_PLAYERS; player++) out.nearest_dist_sq[player] = 0xFFFFFFFF;

	ai_settlement_view settlement;
	for (ui16 slot = 0; slot < ai_world_settlement_slots(ctx.world); slot++)
	{
		if (!ai_view_settlement(ctx.world, slot, settlement) || settlement.owner == ctx.player || settlement.owner >= AI_MAX_TRACKED_PLAYERS) continue;

		out.settlements[settlement.owner]++;
		out.troops[settlement.owner] += ai_troops_sum(settlement.home_troops);
		for (ui16 own = 0; own < ctx.settlement_count; own++)
		{
			out.nearest_dist_sq[settlement.owner] = ia_min(out.nearest_dist_sq[settlement.owner], ai_dist_sq(settlement.location, ctx.settlements[own].location));
		}
	}

	ai_army_view army;
	for (ui16 slot = 0; slot < ai_world_army_slots(ctx.world); slot++)
	{
		if (ai_view_army(ctx.world, slot, army) && army.owner != ctx.player && army.owner < AI_MAX_TRACKED_PLAYERS) out.troops[army.owner] += army.troops;
	}
}

// The whole opinion update rule. Each run, per other player still alive:
// - drifts back 1 towards neutral;
// - weaker than us (less than half our troops): -3, much stronger (more than double): +3;
// - close to us: -3 within 40 tiles, -1 within 90;
// - had an army near one of our settlements (threat_owner): -15.
static void ai_update_opinions(ai_context& ctx, const ai_player_scan& scan, ui32 own_troops, match_player_id threat_owner)
{
	for (ui8 player = 0; player < AI_MAX_TRACKED_PLAYERS; player++)
	{
		if (player == ctx.player) continue;
		if (scan.settlements[player] == 0) { ctx.state.opinions[player] = 0; continue; }

		i32 opinion = ctx.state.opinions[player];
		opinion += opinion < 0 ? 1 : (opinion > 0 ? -1 : 0);
		if (scan.troops[player] * 2 < own_troops) opinion -= 3;
		else if (scan.troops[player] > own_troops * 2) opinion += 3;

		ui32 nearest = scan.nearest_dist_sq[player];
		if (nearest < 40 * 40) opinion -= 3;
		else if (nearest < 90 * 90) opinion -= 1;

		if (player == threat_owner) opinion -= 15;

		ctx.state.opinions[player] = (i16)ia_clamped(opinion, -AI_OPINION_LIMIT, AI_OPINION_LIMIT);
	}
}
