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

// Taxes feed the treasury (roads, buildings, armies) and slow growth a little. The wanted rate follows the treasury: high while it is low
// (roads and buildings always need more), normal above AI_TREASURY_COMFORT, 1 when rich. A settlement steps down while it shrinks,
// and back up towards the wanted rate while it does not.
static void ai_tactics_taxes(ai_context& ctx)
{
	i32 wanted = ctx.treasury > AI_RICH_TREASURY ? 1 : (ctx.treasury < AI_TREASURY_COMFORT ? AI_TAX_HIGH : AI_TAX_NORMAL);
	for (ui32 index = 0; index < ai_window_size(ctx); index++)
	{
		const ai_settlement_view& settlement = ai_window_settlement(ctx, index);

		i32 desired = settlement.tax_rate;
		if (settlement.growth_per_mille < -10) desired -= AI_TAX_STEP;
		else if (settlement.growth_per_mille >= 0 && desired < wanted) desired = ia_min(desired + AI_TAX_STEP, wanted);
		desired = ia_clamped(ia_min(desired, wanted), GAME_PARAMETERS.tax_rate_min, GAME_PARAMETERS.tax_rate_max);
		if (desired == settlement.tax_rate) continue;

		auto* tax = ai_push<command_data_set_settlement_tax_rate>(ctx, MATCH_COMMAND_TYPE::SET_SETTLEMENT_TAX_RATE);
		if (tax == nullptr) return;
		tax->settlement = settlement.guid;
		tax->tax_rate = (i8)desired;
	}
}

// Row of SETTLEMENT_BUILDING_TABLE to build in the settlement, or -1. Trade first: caravanserais (caravans bring goods beyond the land output),
// then a trade depot in big trading settlements (more capacity: caravans pay more there), then barracks outside of EXPAND.
static i32 ai_pick_building(const ai_context& ctx, const ai_settlement_view& settlement)
{
	if (settlement.caravanserai < ia_min(AI_MAX_CARAVANSERAI, settlement.population / AI_POPULATION_PER_CARAVANSERAI)) return AI_BUILDING_CARAVANSERAI;
	if (settlement.population >= AI_TRADE_DEPOT_MIN_POPULATION && settlement.caravanserai > 0 && settlement.trade_depots == 0) return AI_BUILDING_TRADE_DEPOT;
	if (ctx.state.mode != AI_MODE_EXPAND && settlement.barracks < settlement.population / AI_POPULATION_PER_BARRACKS) return AI_BUILDING_BARRACKS;
	return -1;
}

// One new building project at a time per settlement and AI_MAX_BUILD_PROJECTS overall, once the treasury holds a few days of its cost.
// Barracks also get their men at arms ceiling opened (it is 0 by default, so nothing is trained).
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
		if (building < 0 || projects >= AI_MAX_BUILD_PROJECTS + ctx.treasury / AI_TREASURY_PER_EXTRA_PROJECT) continue;
		if (ctx.treasury < SETTLEMENT_BUILDING_TABLE[building].cost_per_day * AI_BUILD_RESERVE_DAYS) continue;
		if (settlement.peasants < SETTLEMENT_BUILDING_TABLE[building].workers + 20u) continue;
		if (ai_count_projects(ctx.world, WORK_TYPE::BUILD_SETTLEMENT_BUILDING, ctx.player, settlement.guid) > 0) continue;

		auto* build = ai_push<command_data_build_settlement_building>(ctx, MATCH_COMMAND_TYPE::BUILD_SETTLEMENT_BUILDING);
		if (build == nullptr) return;
		build->settlement = settlement.guid;
		build->building_type = (ui8)building;
		projects++;
	}
}
