// Military tactics: the AI uses the campaign system only (settlements join a campaign and raise its armies, the campaign gets an objective and is begun). Assaults are begun by the sim's campaign logic.
// Defense is left to the garrisons and the campaign armies the sim already runs.

#include "match_ai_internal.h"


// The biggest foreign army within AI_THREAT_RADIUS of one of our settlements.
static bool ai_find_threat(const ai_context& ctx, ai_army_view& out_threat)
{
	bool found = false;
	ai_army_view army;
	for (ui16 slot = 0; slot < ai_world_army_slots(ctx.world); slot++)
	{
		if (!ai_view_army(ctx.world, slot, army) || army.owner == ctx.player || (found && army.troops <= out_threat.troops)) continue;

		for (ui16 own = 0; own < ctx.settlement_count; own++)
		{
			if (ai_dist_sq(army.location, ctx.settlements[own].location) > AI_THREAT_RADIUS * AI_THREAT_RADIUS) continue;

			out_threat = army;
			found = true;
			break;
		}
	}
	return found;
}

// A campaign army's troops: campaign_army_garrison_percent of each type of the settlement's garrison (what the sim raises for a settlement joining a campaign).
static army_troop_count ai_campaign_army_targets(const ai_settlement_view& settlement)
{
	army_troop_count targets = {};
	for (ui8 troopType = 0; troopType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; troopType++)
	{
		targets.count[troopType] = (ui16)((ui32)settlement.home_troops.count[troopType] * GAME_PARAMETERS.campaign_army_garrison_percent / 100);
	}
	return targets;
}

// Whether the settlement can join the campaign (invalid = found one) now with an army worth sending: in no campaign, enough peasants, a garrison to take men from,
// the treasury covers the raise cost, and the CAMPAIGN_JOIN command would really be accepted (besieged, kept army of another owner, no army slot... see ai_world_join_is_valid).
// Returns the men it would add in out_men and charges the cost to the treasury figure the caller keeps.
static bool ai_settlement_can_join(const ai_context& ctx, const ai_settlement_view& settlement, entity_guid campaign, ui32& treasury, ui32& out_men)
{
	if (settlement.peasants < AI_MIN_RAISE_PEASANTS || settlement.campaign.is_valid()) return false;

	army_troop_count targets = ai_campaign_army_targets(settlement);
	out_men = ai_troops_sum(targets);
	ui32 cost = ai_world_raising_cost(targets); // A new campaign army costs gold taken from the treasury (the army starts with it as gold).
	if (out_men == 0 || treasury < cost) return false;
	if (!ai_world_join_is_valid(ctx.world, ctx.player, settlement.guid, campaign)) return false;

	treasury -= cost;
	return true;
}

// Gold the cheapest settlement that could join a campaign would pay for its army (AI rules only: ignores besieged settlements and the like). 0 = none could.
// The economy keeps this much in the treasury when it starts buildings, so a campaign is never starved by construction.
static ui32 ai_next_campaign_army_cost(const ai_context& ctx)
{
	ui32 best = 0;
	for (ui16 index = 0; index < ctx.settlement_count; index++)
	{
		const ai_settlement_view& settlement = ctx.settlements[index];
		if (settlement.peasants < AI_MIN_RAISE_PEASANTS || settlement.campaign.is_valid()) continue;

		army_troop_count targets = ai_campaign_army_targets(settlement);
		if (ai_troops_sum(targets) == 0) continue;

		ui32 cost = ai_world_raising_cost(targets);
		if (best == 0 || cost < best) best = cost;
	}
	return best;
}

// Disband condition of a campaign that has an objective: it is older than AI_ATTACK_MAX_DAYS (as seen by the AI), or, once past GATHERING (raising armies
// count few men), its members fell under a third of the objective target's attack need.
static bool ai_campaign_is_stuck(const ai_context& ctx, const ai_campaign_view& campaign)
{
	if (ctx.day - ctx.state.campaign_seen_day > AI_ATTACK_MAX_DAYS) return true;

	ai_settlement_view objectiveTarget;
	if (campaign.state < CAMPAIGN_STATE_MARCHING || !ai_view_settlement_by_guid(ctx.world, campaign.objective_target, objectiveTarget)) return false;
	return campaign.troops < ai_attack_need(objectiveTarget) / 3;
}

// The AI's military is its campaign (one at a time), driven like the human client does it: a settlement founds it (CAMPAIGN_JOIN without campaign) once there is a target,
// then, as soon as it exists, one batch gives it its objective and sends the settlements that bring the wanted strength (the client's CampaignStart.finish).
// Armies then gather, and the campaign marches by itself (CAMPAIGN_BEGIN only skips the wait for stragglers once the members are strong enough).
// EXPAND only stops new campaigns from being founded (and disbands one without objective). A campaign with no objective and no target is disbanded (it would sit WAITING for ever),
// and so is one with an objective that got stuck (ai_campaign_is_stuck).
static void ai_tactics_campaign(ai_context& ctx)
{
	ai_campaign_view campaign;
	bool hasCampaign = ai_view_own_campaign(ctx.world, ctx.player, campaign);
	ai_settlement_view target;
	bool hasTarget = ai_target_is_valid(ctx, target);
	bool expanding = ctx.state.mode == AI_MODE_EXPAND;

	if (hasCampaign && !(campaign.guid == ctx.state.campaign_seen))
	{
		ctx.state.campaign_seen = campaign.guid;
		ctx.state.campaign_seen_day = ctx.day;
	}

	if (hasCampaign)
	{
		bool hasObjective = campaign.objective_type != CAMPAIGN_GOAL_NONE;
		if ((!hasObjective && (expanding || !hasTarget)) || (hasObjective && ai_campaign_is_stuck(ctx, campaign)))
		{
			auto* disband = ai_push<command_data_campaign_disband>(ctx, MATCH_COMMAND_TYPE::CAMPAIGN_DISBAND);
			if (disband != nullptr) disband->campaign = campaign.guid;
			return;
		}
	}
	else if (expanding) return;

	if (!hasTarget || ctx.army_count + 8 > AI_MAX_OWNED_ARMIES) return;

	ui32 treasury = ctx.treasury; // What the joins of this decision leave in the treasury.
	ui32 men = 0;
	if (!hasCampaign)
	{
		// Founder: the rally settlement (our closest to the target) if it can, else the first one that can.
		const ai_settlement_view* founder = nullptr;
		for (ui16 index = 0; index < ctx.settlement_count && founder == nullptr; index++)
		{
			ui32 scratchTreasury = treasury;
			if (ctx.settlements[index].guid == ctx.state.rally_settlement && ai_settlement_can_join(ctx, ctx.settlements[index], INVALID_ENTITY_GUID, scratchTreasury, men)) founder = &ctx.settlements[index];
		}
		for (ui16 index = 0; index < ctx.settlement_count && founder == nullptr; index++)
		{
			ui32 scratchTreasury = treasury;
			if (ai_settlement_can_join(ctx, ctx.settlements[index], INVALID_ENTITY_GUID, scratchTreasury, men)) founder = &ctx.settlements[index];
		}
		if (founder == nullptr) return;

		auto* found = ai_push<command_data_campaign_join>(ctx, MATCH_COMMAND_TYPE::CAMPAIGN_JOIN);
		if (found != nullptr) found->entity = founder->guid; // campaign stays invalid: founds one. It exists on the next decision.
		return;
	}

	ui32 need = ai_attack_need(target);
	if (campaign.objective_type == CAMPAIGN_GOAL_NONE)
	{
		auto* objective = ai_push<command_data_set_campaign_objective>(ctx, MATCH_COMMAND_TYPE::SET_CAMPAIGN_OBJECTIVE);
		if (objective == nullptr) return;
		objective->campaign = campaign.guid;
		objective->objective_type = CAMPAIGN_GOAL_CONQUER_SETTLEMENT;
		objective->target = target.guid;
	}
	else if (campaign.state == CAMPAIGN_STATE_GATHERING && campaign.troops >= need)
	{
		auto* begin = ai_push<command_data_campaign_begin>(ctx, MATCH_COMMAND_TYPE::CAMPAIGN_BEGIN);
		if (begin != nullptr) begin->campaign = campaign.guid;
	}

	// Settlements join until the armies (the raising ones at their target) reach the wanted strength: the target's attack need, at least AI_MIN_ARMY_STRENGTH.
	ui32 wanted = ia_max(AI_MIN_ARMY_STRENGTH, need);
	ui32 joined = 0;
	ui32 freeSlots = ai_world_free_army_slots(ctx.world); // Each join may need a new army: the check above sees the slots one join at a time.
	for (ui16 index = 0; index < ctx.settlement_count && ctx.army_strength + joined < wanted && freeSlots > 0; index++)
	{
		if (!ai_settlement_can_join(ctx, ctx.settlements[index], campaign.guid, treasury, men)) continue;

		auto* join = ai_push<command_data_campaign_join>(ctx, MATCH_COMMAND_TYPE::CAMPAIGN_JOIN);
		if (join == nullptr) return;
		join->entity = ctx.settlements[index].guid;
		join->campaign = campaign.guid;
		joined += men;
		freeSlots--;
	}
}
