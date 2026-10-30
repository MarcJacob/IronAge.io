// Military tactics: raising armies, defending against nearby foreign armies, and the campaign (gather at the rally settlement, then attack the target together).

#include "match_ai_internal.h"

constexpr ui32 AI_DEFEND_GATHER_RADIUS = 40; // Own idle armies this close to a threat help against it.

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

static bool ai_push_attack(ai_context& ctx, entity_guid attacker, entity_guid target)
{
	auto* attack = ai_push<command_data_set_entity_attack_target>(ctx, MATCH_COMMAND_TYPE::SET_ENTITY_ATTACK_TARGET);
	if (attack == nullptr) return false;
	attack->attacker_entity = attacker;
	attack->target_entity = target;
	return true;
}

// Idle armies near a foreign army that threatens us all attack it together, if together they clearly outnumber it (1.3x).
static void ai_tactics_defend(ai_context& ctx)
{
	ai_army_view threat;
	if (!ai_find_threat(ctx, threat)) return;

	ui32 strength = 0;
	for (ui16 index = 0; index < ctx.army_count; index++)
	{
		const ai_army_view& army = ctx.armies[index];
		if (army.idle && ai_dist_sq(army.location, threat.location) <= AI_DEFEND_GATHER_RADIUS * AI_DEFEND_GATHER_RADIUS) strength += army.troops;
	}
	if (strength * 10 < threat.troops * 13 + 50) return;

	ui32 orders = 0;
	for (ui16 index = 0; index < ctx.army_count && orders < AI_MAX_ORDERS; index++)
	{
		const ai_army_view& army = ctx.armies[index];
		if (!army.idle || ai_dist_sq(army.location, threat.location) > AI_DEFEND_GATHER_RADIUS * AI_DEFEND_GATHER_RADIUS) continue;
		if (!ai_push_attack(ctx, army.guid, threat.guid)) return;
		orders++;
	}
}

static bool ai_settlement_is_raising(const ai_context& ctx, entity_guid settlement)
{
	for (ui16 index = 0; index < ctx.army_count; index++)
	{
		if (ctx.armies[index].raising && ctx.armies[index].home == settlement) return true;
	}
	return false;
}

// Outside of EXPAND, until the armies reach the wanted strength (the target's attack need, at least AI_MIN_ARMY_STRENGTH), raises an army from
// up to two settlements of the window that have the peasants and no raise going on: a share of the peasants as levies, plus the whole men at arms pool.
static void ai_tactics_raise_armies(ai_context& ctx)
{
	if (ctx.state.mode == AI_MODE_EXPAND || ctx.army_count + 8 > AI_MAX_OWNED_ARMIES) return;

	ai_settlement_view target;
	ui32 wanted = ia_max(AI_MIN_ARMY_STRENGTH, ai_target_is_valid(ctx, target) ? ai_attack_need(target) : 0);
	ui32 raised = 0, raises = 0;
	ui32 treasury = ctx.treasury; // What the raises of this turn leave in the treasury.
	for (ui32 index = 0; index < ai_window_size(ctx) && raises < 2 && ctx.army_strength + raised < wanted; index++)
	{
		const ai_settlement_view& settlement = ai_window_settlement(ctx, index);
		if (settlement.peasants < AI_MIN_RAISE_PEASANTS || ai_settlement_is_raising(ctx, settlement.guid)) continue;

		army_troop_count targets = {};
		targets.levies = (ui16)ia_min((ui32)settlement.home_troops.levies, 400u); // Levies are a garrison pool now, not taken from the peasants.
		targets.men_at_arms = settlement.home_troops.men_at_arms;

		// Raising costs one day of the army's consumption, taken from the treasury.
		ui32 cost = ai_world_troops_daily_consumption(targets);
		if (treasury < cost) break;

		auto* raise = ai_push<command_data_raise_army>(ctx, MATCH_COMMAND_TYPE::RAISE_ARMY);
		if (raise == nullptr) return;
		raise->settlement = settlement.guid;
		raise->raise_location = settlement.location;
		raise->start_order = RAISE_START_ORDER_STATION; // The AI moves its armies itself: they stand where raised (P5 replaces this with orders).
		raise->target_levies = targets.levies;
		raise->target_men_at_arms = targets.men_at_arms;
		treasury -= cost;
		raised += raise->target_levies + raise->target_men_at_arms;
		raises++;
	}
}

// In ATTACK: idle armies walk to the rally settlement; once the idle armies around it add up to the target's attack need, they all attack the target.
static void ai_tactics_campaign(ai_context& ctx)
{
	if (ctx.state.mode != AI_MODE_ATTACK) return;

	ai_settlement_view target, rally;
	if (!ai_target_is_valid(ctx, target) || !ai_view_settlement_by_guid(ctx.world, ctx.state.rally_settlement, rally)) return;

	ui32 gathered = 0, orders = 0;
	for (ui16 index = 0; index < ctx.army_count && orders < AI_MAX_ORDERS; index++)
	{
		const ai_army_view& army = ctx.armies[index];
		if (!army.idle || army.troops == 0) continue;

		if (ai_dist_sq(army.location, rally.location) <= AI_RALLY_RADIUS * AI_RALLY_RADIUS) { gathered += army.troops; continue; }

		auto* move = ai_push<command_data_set_entity_move_target>(ctx, MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET);
		if (move == nullptr) return;
		move->entity = army.guid;
		move->move_target = rally.location;
		orders++;
	}
	if (gathered < ai_attack_need(target)) return;

	for (ui16 index = 0; index < ctx.army_count && orders < AI_MAX_ORDERS; index++)
	{
		const ai_army_view& army = ctx.armies[index];
		if (!army.idle || army.troops == 0 || ai_dist_sq(army.location, rally.location) > AI_RALLY_RADIUS * AI_RALLY_RADIUS) continue;
		if (!ai_push_attack(ctx, army.guid, target.guid)) return;
		orders++;
	}
}

constexpr ui32 AI_ASSAULT_OUTNUMBER = 3; // Begin the assault when the besiegers have this many times the defenders' men (total men)...
constexpr ui32 AI_ASSAULT_MIN_SUPPLY_DAYS = 2; // ...or when one of the besieging armies has fewer days of supplies than this (P5 replaces this rule).

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

static bool ai_is_next_to_own_settlement(const ai_context& ctx, const ai_army_view& army)
{
	for (ui16 index = 0; index < ctx.settlement_count; index++)
	{
		if (ctx.settlements[index].guid == army.home) return ai_dist_sq(army.location, ctx.settlements[index].location) <= 3 * 3;
	}
	return false;
}

// Idle remnants of engagements standing next to their home settlement are disbanded into it (a few per run). In EXPAND every idle army
// at home is: its levies go back to work the land and it stops costing supplies.
static void ai_tactics_disband_remnants(ai_context& ctx)
{
	ui32 disbands = 0;
	for (ui16 index = 0; index < ctx.army_count && disbands < 3; index++)
	{
		bool remnant = ctx.armies[index].troops < AI_TINY_ARMY || ctx.state.mode == AI_MODE_EXPAND;
		if (!ctx.armies[index].idle || !remnant || !ai_is_next_to_own_settlement(ctx, ctx.armies[index])) continue;

		auto* disband = ai_push<command_data_disband_army>(ctx, MATCH_COMMAND_TYPE::DISBAND_ARMY);
		if (disband == nullptr) return;
		disband->army = ctx.armies[index].guid;
		disbands++;
	}
}
