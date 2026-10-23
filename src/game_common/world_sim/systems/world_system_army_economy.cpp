// Army economy systems: soldiers consume wealth every day, and the owner's treasury pays for it.
// Armies carry their own supplies. Their soldiers consume them daily (per-type upkeep rates), so consumption is a sink of the economy.
// Inside territory its owner owns, an army refills its supplies from the treasury, up to a number of days of its own consumption.
// Consumption that supplies cannot cover makes troops desert, in which case they go back home.
// Outside of the owner's territory armies are not supplied: they must be back regularly, or find *alternative* means of financing themselves (later).
// Soldiers waiting in a settlement (local pool) are paid wages from the treasury instead, which become the settlement's income.

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world.h"
#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_settlement.h"

#include "../world_sim.h"

// True if the army stands on a tile claimed by a settlement of its owner.
static bool army_in_owned_territory(const match_world_state& world, ui16 army_index)
{
	const world_entity_armies& armies = world.entities->armies;

	ui16 regionIndex, tileIndex;
	world_get_tile(*world.terrain, armies.locations[army_index], regionIndex, tileIndex);
	ui16 tileOwner = world.terrain->regions[regionIndex].owners[tileIndex];

	return tileOwner != world_terrain::UNOWNED_TILE && world.entities->settlements.owners[tileOwner] == armies.owners[army_index];
}

// Runs daily. Refills the supplies of the armies inside their owner's territory (the treasury pays as much as it can: never below 0, no debt),
// then every army consumes. What supplies could not cover is the share of the army that deserts.
void world_system_armies_upkeep(match_world_state& world)
{
	if (!world_is_daily_tick(world)) return;

	world_entity_armies& armies = world.entities->armies;

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid()) continue;

		match_player_id owner = armies.owners[armyIndex];
		if (owner >= world.player_count) continue;

		world_player_state& player = world.player_states[owner];
		ui32& supplies = armies.supplies[armyIndex];

		// Refill. Supplies above the capacity are kept, just not refilled.
		ui32 capacity = army_supply_capacity(world, (ui16)armyIndex);
		if (supplies < capacity && army_in_owned_territory(world, (ui16)armyIndex))
		{
			ui32 refill = ia_min(capacity - supplies, player.treasury);
			player.treasury -= refill;
			player.expenses += refill;
			supplies += refill;
		}

		// Consumption.
		ui32 consumption = army_daily_consumption(armies.compositions[armyIndex]);
		ui32 consumed = ia_min(consumption, supplies);
		supplies -= consumed;

		// Desertion: each troop type loses the unfunded fraction of the day's consumption (rounded down).
		ui32 unfunded = consumption - consumed;
		if (unfunded == 0) continue;

		// The army is destroyed when its last man leaves: stop at once, its slot is free.
		entity_guid army = armies.guids[armyIndex];
		army_troop_count troops = armies.compositions[armyIndex];
		for (ui8 troopType = 0; troopType < TROOP_TYPE_COUNT; troopType++)
		{
			ui16 deserters = (ui16)((ui64)troop_count_field(troops, troopType) * unfunded / consumption);
			world_army_desertion(world, army, troopType, deserters);
			if (!world_entity_is_valid(world, army)) break;
		}
	}
}

// Runs daily. Soldiers of the local pools get their wages from the owner's treasury (down to 0, no debt) into the settlement's pending wages.
void world_system_local_troops_upkeep(match_world_state& world)
{
	if (!world_is_daily_tick(world)) return;

	world_entity_settlements& settlements = world.entities->settlements;

	for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
	{
		if (!settlements.guids[settlementIndex].is_valid()) continue;

		settlement_wealth_fund_from_treasury(world, settlementIndex, army_daily_consumption(settlements.populations[settlementIndex].home));
	}
}
