// Army economy systems: soldiers consume wealth every day, and the owner's treasury pays for it.
// Armies carry their own supplies. Their soldiers consume them daily (per-type upkeep rates), so consumption is a sink of the economy.
// Inside territory its owner owns, an army refills its supplies from the treasury (a couple of days' worth per day), up to its carry capacity.
// Consumption that supplies cannot cover makes troops desert, in which case they go back home. A raising army stops raising instead.
// Outside of the owner's territory armies are not supplied: they must be back regularly, or find *alternative* means of financing themselves (later).
// Soldiers waiting in a settlement (local pool) are paid from the treasury instead.

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/entities/world_entity_engagements.h"
#include "game_common/match/world/world.h"
#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_settlement.h"

#include "../world_sim.h"

// True if the army stands on a tile claimed by a settlement of its owner.
static bool army_is_in_owner_territory(const match_world_state& world, ui16 army_index)
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

		armies.desertions[armyIndex] = 0; // Counts the last day only.

		// Men of an army whose home settlement now belongs to someone else desert: army_wrong_origin_owner_desertion_percent of each type per day, rounded up.
		// They go back to the home settlement. The army is destroyed when its last man leaves: stop at once, its slot is free.
		entity_guid originSettlement = armies.home_settlements[armyIndex];
		if (world_entity_is_valid(world, originSettlement) && originSettlement.get_type() == ENTITY_TYPE::SETTLEMENT
			&& world.entities->settlements.owners[originSettlement.get_index()] != owner)
		{
			entity_guid originArmy = armies.guids[armyIndex];
			army_troop_count troops = armies.compositions[armyIndex];
			for (ui8 troopType = 0; troopType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT && world_entity_is_valid(world, originArmy); troopType++)
			{
				ui16 deserters = (ui16)(((ui64)troops.count[troopType] * ia_min(GAME_PARAMETERS.army_wrong_origin_owner_desertion_percent, (ui32)100) + 99) / 100);
				ui16 left = world_army_desertion(world, originArmy, (TROOP_TYPE)troopType, deserters);
				if (world_entity_is_valid(world, originArmy)) armies.desertions[armyIndex] += left; // Destroyed: its stats are gone with it.
			}
			if (!world_entity_is_valid(world, originArmy)) continue;
		}

        // If army is engaged in a field battle, do not run its supply consumption / refill logic.
        entity_guid armyCurrentEngagement = armies.engagements[armyIndex];
        if (armyCurrentEngagement.is_valid() && world.entities->engagements.states[armyCurrentEngagement.get_index()].kind == ENGAGEMENT_KIND::FIELD_BATTLE) continue;

		// Refill, before consuming so the day's consumption is covered. It adds army_supply_refill_days of the supply basis's consumption at most,
		// and leaves room for the day's consumption above the capacity, so the army ends the day at the capacity (not below it) if it can.
		// Supplies above the capacity are kept, just not refilled.
		ui32 consumption = army_daily_consumption(armies.compositions[armyIndex]);
		ui32 capacity = army_supply_capacity(world, (ui16)armyIndex);
		ui32 headroom = (ui32)ia_min((ui64)capacity + consumption, (ui64)0xFFFFFFFF);
		if (supplies < headroom && army_is_in_owner_territory(world, (ui16)armyIndex))
		{
			ui64 refillRate = (ui64)army_daily_consumption(army_supply_basis(world, (ui16)armyIndex)) * GAME_PARAMETERS.army_supply_refill_days;
			ui32 refill = (ui32)ia_min(ia_min((ui64)headroom - supplies, refillRate), (ui64)player.treasury);
			player.treasury -= refill;
			player.expenses += refill;
			supplies += refill;
		}

		// Consumption.
		ui32 consumed = ia_min(consumption, supplies);
		supplies -= consumed;

		// Not enough supplies: a raising army stops raising (keeping its men, no desertion today), any other army loses men.
		ui32 unfunded = consumption - consumed;
		if (unfunded == 0) continue;
		if (armies.raising[armyIndex])
		{
			armies.raising[armyIndex] = 0;
			continue;
		}

		// Desertion: each troop type loses the unfunded fraction of the day's consumption (rounded down).

		// The army is destroyed when its last man leaves: stop at once, its slot is free.
		entity_guid army = armies.guids[armyIndex];
		army_troop_count troops = armies.compositions[armyIndex];
		for (ui8 troopType = 0; troopType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT; troopType++)
		{
			ui16 deserters = (ui16)((ui64)troops.count[troopType] * unfunded / consumption);
			ui16 left = world_army_desertion(world, army, (TROOP_TYPE)troopType, deserters);

			if (!world_entity_is_valid(world, army)) 
                break; // Destroyed: its stats are gone with it.
                       
			armies.desertions[armyIndex] += left;
		}
	}
}
