// AI behaviors: tiny independent functions, each reading the observed state and pushing commands for one player.
// Same for every AI player. To add one: write it here, call it from ai_decide.

#include "game_common/match/ai/match_ai.h"
#include "game_common/match/game_parameters.h"

// What a behavior works with.
struct ai_context
{
	const ai_observed_world_state& observed;
	match_player_id player;
	command_sequence_builder& commands; // Unvalidated sequence of this decision.
};

// Placeholder tuning.
constexpr ui32 AI_RAISE_MIN_TREASURY = 100;
constexpr ui32 AI_RAISE_MIN_PEASANTS = 100;
constexpr ui32 AI_ARMIES_PER_SETTLEMENT = 2;
constexpr ui32 AI_ATTACK_MIN_TROOPS = 20;
constexpr ui8 AI_BARRACKS_BUILDING_TYPE = 0; // Row of SETTLEMENT_BUILDING_TABLE.

static ui32 ai_distance_sq(world_location a, world_location b)
{
	i32 dx = (i32)a.x - b.x, dy = (i32)a.y - b.y;
	return (ui32)(dx * dx + dy * dy);
}

static ui32 ai_treasury(const ai_context& ctx)
{
	return ctx.player < ctx.observed.player_count ? ctx.observed.treasuries[ctx.player] : 0;
}

// Raises one army from a settlement with enough peasants, as long as the player has fewer than a few armies per settlement.
static void ai_behavior_raise_army(ai_context& ctx)
{
	if (ai_treasury(ctx) < AI_RAISE_MIN_TREASURY) return;

	ui32 settlementCount = 0, armyCount = 0;
	for (ui16 i = 0; i < ctx.observed.army_count; i++) armyCount += ctx.observed.armies[i].owner == ctx.player;
	for (ui16 i = 0; i < ctx.observed.settlement_count; i++) settlementCount += ctx.observed.settlements[i].owner == ctx.player;
	if (armyCount >= settlementCount * AI_ARMIES_PER_SETTLEMENT) return;

	for (ui16 i = 0; i < ctx.observed.settlement_count; i++)
	{
		const ai_observed_settlement& settlement = ctx.observed.settlements[i];
		if (settlement.owner != ctx.player || settlement.peasants < AI_RAISE_MIN_PEASANTS) continue;

		auto* raise = ctx.commands.push_command<command_data_raise_army>(MATCH_COMMAND_TYPE::RAISE_ARMY);
		if (raise == nullptr) return;
		raise->settlement = settlement.guid;
		raise->raise_location = settlement.location;
		raise->target_levies = (ui16)ia_min(settlement.peasants / 4, 150u);
		raise->target_men_at_arms = settlement.home_troops.men_at_arms;
		return; // One per decision.
	}
}

// Sends each strong enough idle army at the nearest settlement it does not own.
static void ai_behavior_attack_with_idle_armies(ai_context& ctx)
{
	for (ui16 i = 0; i < ctx.observed.army_count; i++)
	{
		const ai_observed_army& army = ctx.observed.armies[i];
		if (army.owner != ctx.player || !army.idle || army.troops < AI_ATTACK_MIN_TROOPS) continue;

		const ai_observed_settlement* nearest = nullptr;
		ui32 nearestDistance = 0;
		for (ui16 s = 0; s < ctx.observed.settlement_count; s++)
		{
			const ai_observed_settlement& settlement = ctx.observed.settlements[s];
			if (settlement.owner == ctx.player) continue;

			ui32 distance = ai_distance_sq(army.location, settlement.location);
			if (nearest == nullptr || distance < nearestDistance) { nearest = &settlement; nearestDistance = distance; }
		}
		if (nearest == nullptr) return;

		auto* attack = ctx.commands.push_command<command_data_set_entity_attack_target>(MATCH_COMMAND_TYPE::SET_ENTITY_ATTACK_TARGET);
		if (attack == nullptr) return;
		attack->attacker_entity = army.guid;
		attack->target_entity = nearest->guid;
	}
}

// Starts a barracks in the first settlement without one and with no construction going on, if the treasury can pay all of it.
static void ai_behavior_build_barracks(ai_context& ctx)
{
	const settlement_building_def& barracks = SETTLEMENT_BUILDING_TABLE[AI_BARRACKS_BUILDING_TYPE];
	if (ai_treasury(ctx) < barracks.cost_per_day * barracks.build_days) return;

	for (ui16 i = 0; i < ctx.observed.settlement_count; i++)
	{
		const ai_observed_settlement& settlement = ctx.observed.settlements[i];
		if (settlement.owner != ctx.player || settlement.workers > 0 || settlement.buildings.building_counts[AI_BARRACKS_BUILDING_TYPE] > 0) continue;

		auto* build = ctx.commands.push_command<command_data_build_settlement_building>(MATCH_COMMAND_TYPE::BUILD_SETTLEMENT_BUILDING);
		if (build == nullptr) return;
		build->settlement = settlement.guid;
		build->building_type = AI_BARRACKS_BUILDING_TYPE;
		return;
	}
}

// Sets the tax rate of every owned settlement to the maximum, so the treasury (which pays upkeep and construction) fills up.
static void ai_behavior_max_taxes(ai_context& ctx)
{
	for (ui16 i = 0; i < ctx.observed.settlement_count; i++)
	{
		const ai_observed_settlement& settlement = ctx.observed.settlements[i];
		if (settlement.owner != ctx.player || settlement.tax_rate == GAME_PARAMETERS.tax_rate_max) continue;

		auto* tax = ctx.commands.push_command<command_data_set_settlement_tax_rate>(MATCH_COMMAND_TYPE::SET_SETTLEMENT_TAX_RATE);
		if (tax == nullptr) return;
		tax->settlement = settlement.guid;
		tax->tax_rate = (i8)GAME_PARAMETERS.tax_rate_max;
	}
}

// Runs all behaviors for one player. Remove or add lines to change what AI players do.
static void ai_decide(ai_context& ctx)
{
	ai_behavior_max_taxes(ctx);
	ai_behavior_raise_army(ctx);
	ai_behavior_attack_with_idle_armies(ctx);
	ai_behavior_build_barracks(ctx);
}
