// Military tactics: the AI uses the campaign system only (settlements join a campaign and raise its armies, the campaign gets an objective and is begun), and begins assaults of its sieges.
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

// The AI's military is its campaign (one at a time). In EXPAND the campaign is disbanded (its armies go home and disband). Otherwise, until the armies reach the wanted strength
// (the target's attack need, at least AI_MIN_ARMY_STRENGTH), up to two settlements of the window that are in no campaign and have the peasants join it: the first one founds it
// (the others join it on a later decision, once it exists), and each raises its campaign army (or sends the one it kept).
static void ai_tactics_campaign_join(ai_context& ctx)
{
	ai_campaign_view campaign;
	bool hasCampaign = ai_view_own_campaign(ctx.world, ctx.player, campaign);

	if (ctx.state.mode == AI_MODE_EXPAND)
	{
		if (!hasCampaign) return;

		auto* disband = ai_push<command_data_campaign_disband>(ctx, MATCH_COMMAND_TYPE::CAMPAIGN_DISBAND);
		if (disband != nullptr) disband->campaign = campaign.guid;
		return;
	}
	if (ctx.army_count + 8 > AI_MAX_OWNED_ARMIES) return;

	ai_settlement_view target;
	ui32 wanted = ia_max(AI_MIN_ARMY_STRENGTH, ai_target_is_valid(ctx, target) ? ai_attack_need(target) : 0);
	ui32 joined = 0, joins = 0;
	ui32 treasury = ctx.treasury; // What the joins of this turn leave in the treasury.
	for (ui32 index = 0; index < ai_window_size(ctx) && joins < 2 && ctx.army_strength + joined < wanted; index++)
	{
		const ai_settlement_view& settlement = ai_window_settlement(ctx, index);
		if (settlement.peasants < AI_MIN_RAISE_PEASANTS || settlement.campaign.is_valid()) continue;

		army_troop_count targets = ai_campaign_army_targets(settlement);
		if (ai_troops_sum(targets) == 0) continue;

		// A new campaign army costs gold taken from the treasury (the army starts with it as gold).
		ui32 cost = ai_world_raising_cost(targets);
		if (treasury < cost) break;

		auto* join = ai_push<command_data_campaign_join>(ctx, MATCH_COMMAND_TYPE::CAMPAIGN_JOIN);
		if (join == nullptr) return;
		join->entity = settlement.guid;
		join->campaign = hasCampaign ? campaign.guid : INVALID_ENTITY_GUID; // Invalid: founds a campaign.
		treasury -= cost;
		joined += ai_troops_sum(targets);
		joins++;
		if (!hasCampaign) break;
	}
}

// In ATTACK: once the campaign's armies add up to the target's attack need, it gets its objective (conquer the target) and is begun. Out of ATTACK a campaign that
// had an objective is abandoned (disbanded).
static void ai_tactics_campaign(ai_context& ctx)
{
	ai_campaign_view campaign;
	if (!ai_view_own_campaign(ctx.world, ctx.player, campaign)) return;

	if (ctx.state.mode != AI_MODE_ATTACK)
	{
		if (campaign.objective_type == CAMPAIGN_OBJECTIVE_NONE) return;

		auto* disband = ai_push<command_data_campaign_disband>(ctx, MATCH_COMMAND_TYPE::CAMPAIGN_DISBAND);
		if (disband != nullptr) disband->campaign = campaign.guid;
		return;
	}

	ai_settlement_view target;
	if (!ai_target_is_valid(ctx, target)) return;

	if (campaign.objective_type == CAMPAIGN_OBJECTIVE_NONE)
	{
		if (campaign.troops < ai_attack_need(target)) return;

		auto* objective = ai_push<command_data_set_campaign_objective>(ctx, MATCH_COMMAND_TYPE::SET_CAMPAIGN_OBJECTIVE);
		if (objective == nullptr) return;
		objective->campaign = campaign.guid;
		objective->objective_type = CAMPAIGN_OBJECTIVE_CONQUER_SETTLEMENT;
		objective->target = target.guid;
		return; // Begun on the next decision, once the objective is set.
	}

	if (campaign.state != CAMPAIGN_STATE_GATHERING) return;

	auto* begin = ai_push<command_data_campaign_begin>(ctx, MATCH_COMMAND_TYPE::CAMPAIGN_BEGIN);
	if (begin != nullptr) begin->campaign = campaign.guid;
}

// Sieges this player commands that are only besieging: begin the assault if the besiegers outnumber the defenders enough, or run low on supplies.
static void ai_tactics_assault(ai_context& ctx)
{
	ai_siege_view siege;
	for (ui16 slot = 0; slot < ai_world_engagement_slots(ctx.world); slot++)
	{
		if (!ai_view_idle_siege(ctx.world, slot, siege) || siege.attacker_commander != ctx.player) continue;
		if (siege.attacker_men < siege.defender_men * AI_ASSAULT_OUTNUMBER && siege.min_supply_days >= AI_ASSAULT_MIN_SUPPLY_DAYS) continue;

		auto* assault = ai_push<command_data_begin_assault>(ctx, MATCH_COMMAND_TYPE::BEGIN_ASSAULT);
		if (assault == nullptr) return;
		assault->engagement = siege.guid;
	}
}
