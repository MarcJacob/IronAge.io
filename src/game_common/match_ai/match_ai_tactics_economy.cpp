// Economy tactics, per settlement, on the round-robin window of the owned settlements: tax rates, buildings, troop ceilings.

#include "match_ai_internal.h"

static ui32 ai_window_size(const ai_context& ctx)
{
	return ia_min((ui32)ctx.settlement_count, AI_SETTLEMENTS_PER_TACTICS);
}

static const ai_settlement_view& ai_window_settlement(const ai_context& ctx, ui32 index)
{
	return ctx.settlements[(ctx.state.settlement_cursor + index) % ctx.settlement_count];
}

// Taxes feed the treasury (buildings, roads) but starve growth: base rate by mode (1 when the treasury is not being spent), 0 while the settlement shrinks.
static void ai_tactics_taxes(ai_context& ctx)
{
	for (ui32 index = 0; index < ai_window_size(ctx); index++)
	{
		const ai_settlement_view& settlement = ai_window_settlement(ctx, index);

		// Hysteresis on the growth: 0 while clearly shrinking, back to the base rate once it grows well, otherwise left alone.
		i32 base = ctx.treasury > AI_RICH_TREASURY ? 1 : AI_TAX_BY_MODE[ctx.state.mode];
		i32 desired = settlement.tax_rate;
		if (settlement.growth_per_mille < -10) desired = 0;
		else if (settlement.growth_per_mille >= 20 || settlement.tax_rate > base) desired = base;
		desired = ia_clamped(desired, GAME_PARAMETERS.tax_rate_min, GAME_PARAMETERS.tax_rate_max);
		if (ia_abs(settlement.tax_rate - desired) < 2) continue; // Hysteresis: no command for a one point change.

		auto* tax = ai_push<command_data_set_settlement_tax_rate>(ctx, MATCH_COMMAND_TYPE::SET_SETTLEMENT_TAX_RATE);
		if (tax == nullptr) return;
		tax->settlement = settlement.guid;
		tax->tax_rate = (i8)desired;
	}
}

// Row of SETTLEMENT_BUILDING_TABLE to build in the settlement, or -1.
static i32 ai_pick_building(const ai_context& ctx, const ai_settlement_view& settlement)
{
	if (ctx.state.mode != AI_MODE_EXPAND && settlement.barracks < 1 + settlement.population / 500) return 0;
	if (settlement.population >= 400 && settlement.caravanserai == 0 && ctx.treasury >= 300) return 1;
	return -1;
}

// One new building project at a time per settlement and AI_MAX_BUILD_PROJECTS overall. Barracks also get their men at arms ceiling opened (it is 0 by default, so nothing is trained).
static void ai_tactics_buildings(ai_context& ctx)
{
	ui32 projects = ai_count_projects(ctx.world, WORK_TYPE::BUILD_SETTLEMENT_BUILDING, ctx.player, INVALID_ENTITY_GUID);

	for (ui32 index = 0; index < ai_window_size(ctx); index++)
	{
		const ai_settlement_view& settlement = ai_window_settlement(ctx, index);

		if (settlement.barracks > 0 && settlement.maa_ceiling == 0)
		{
			auto* ceiling = ai_push<command_data_set_settlement_troop_ceiling>(ctx, MATCH_COMMAND_TYPE::SET_SETTLEMENT_TROOP_CEILING);
			if (ceiling == nullptr) return;
			ceiling->settlement = settlement.guid;
			ceiling->troop_type = 0;
			ceiling->ceiling = 0xFFFF;
		}

		i32 building = ai_pick_building(ctx, settlement);
		if (building < 0 || projects >= AI_MAX_BUILD_PROJECTS + ctx.treasury / AI_TREASURY_PER_EXTRA_PROJECT || ctx.treasury < AI_MIN_BUILDING_TREASURY) continue;
		if (settlement.peasants < SETTLEMENT_BUILDING_TABLE[building].workers + 20u) continue;
		if (ai_count_projects(ctx.world, WORK_TYPE::BUILD_SETTLEMENT_BUILDING, ctx.player, settlement.guid) > 0) continue;

		auto* build = ai_push<command_data_build_settlement_building>(ctx, MATCH_COMMAND_TYPE::BUILD_SETTLEMENT_BUILDING);
		if (build == nullptr) return;
		build->settlement = settlement.guid;
		build->building_type = (ui8)building;
		projects++;
	}
}
