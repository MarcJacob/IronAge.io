// Army economy systems: soldiers consume goods every day, and armies buy them with gold.
// Armies carry gold and supplies (goods). Their soldiers consume the supplies daily (per-type upkeep rates).
// Inside territory its owner owns (pay only; purchases need the settlement itself), an army is paid gold by the treasury (a day of consumption at the goods price per day), up to its carry capacity.
// An army standing in a settlement itself (at its location) buys goods from its market goods with its gold, up to its carry capacity. The owner of the settlement gets the gold.
// Consumption that supplies cannot cover makes troops desert, in which case they go back home. A raising army stops raising instead.
// Outside of the owner's territory armies are not paid: they must be back regularly, or find *alternative* means of financing themselves (later).
// Soldiers waiting in a settlement (local pool) are paid from the treasury instead.

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/entities/world_entity_engagements.h"
#include "game_common/match/world/world.h"
#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_settlement.h"

#include "../world_sim.h"

// Index of the settlement whose territory the army stands in, world_terrain::UNOWNED_TILE if none.
ui16 army_tile_owner(const match_world_state& world, ui16 army_index)
{
	ui16 regionIndex, tileIndex;
	world_get_tile(*world.terrain, world.entities->armies.location[army_index], regionIndex, tileIndex);
	return world.terrain->regions[regionIndex].owners[tileIndex];
}

// True if the army stands on a tile claimed by a settlement of its owner.
static bool army_is_in_owner_territory(const match_world_state& world, ui16 army_index)
{
	ui16 tileOwner = army_tile_owner(world, army_index);
	return tileOwner != world_terrain::UNOWNED_TILE && world.entities->settlements.owner[tileOwner] == world.entities->armies.owner[army_index];
}

// Runs daily. Pays the armies inside their owner's territory (the treasury pays as much as it can: never below 0, no debt), then they buy goods with their gold
// from the settlement they stand in, then every army consumes. What supplies could not cover is the share of the army that deserts.
void world_system_armies_upkeep(match_world_state& world)
{
	if (!world_is_daily_tick(world)) return;

	world_entity_armies& armies = world.entities->armies;

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid()) continue;

		match_player_id owner = armies.owner[armyIndex];
		if (owner >= world.player_count) continue;

		world_player_state& player = world.player_states[owner];
		ui32& supplies = armies.supplies[armyIndex];

		armies.desertions[armyIndex] = 0; // Counts the last day only.

		// Men of an army whose home settlement now belongs to someone else desert: army_wrong_origin_owner_desertion_percent of each type per day, rounded up.
		// They go back to the home settlement. The army is destroyed when its last man leaves: stop at once, its slot is free.
		entity_guid originSettlement = armies.home_settlement[armyIndex];
		if (world_entity_is_valid(world, originSettlement) && originSettlement.get_type() == ENTITY_TYPE::SETTLEMENT
			&& world.entities->settlements.owner[originSettlement.get_index()] != owner)
		{
			entity_guid originArmy = armies.guids[armyIndex];
			army_troop_count troops = armies.composition[armyIndex];
			for (ui8 troopType = 0; troopType < (ui8)TROOP_TYPE::TROOP_TYPE_COUNT && world_entity_is_valid(world, originArmy); troopType++)
			{
				ui16 deserters = (ui16)(((ui64)troops.count[troopType] * ia_min(GAME_PARAMETERS.army_wrong_origin_owner_desertion_percent, (ui32)100) + 99) / 100);
				ui16 left = world_army_desertion(world, originArmy, (TROOP_TYPE)troopType, deserters);
				if (world_entity_is_valid(world, originArmy)) armies.desertions[armyIndex] += left; // Destroyed: its stats are gone with it.
			}
			if (!world_entity_is_valid(world, originArmy)) continue;
		}

        // If army is in an engagement where troops are fighting (field battle, assault...), do not run its supply consumption / refill logic.
        entity_guid armyCurrentEngagement = armies.engagement[armyIndex];
        if (armyCurrentEngagement.is_valid() && world.entities->engagements.fighting_active[armyCurrentEngagement.get_index()]) continue;

		ui32 consumption = army_daily_consumption(armies.composition[armyIndex]);
		ui32 capacity = army_supply_capacity(world, (ui16)armyIndex);
		ui32 price = GAME_PARAMETERS.good_price_gold;
		ui16 settlementIndex = army_tile_owner(world, (ui16)armyIndex);
		bool inOwnerTerritory = army_is_in_owner_territory(world, (ui16)armyIndex);

		// Pay: Two days of the supply basis's consumption at the goods price, up to the gold capacity, as far as the treasury can afford.
        // Two days so that armies can accumulate supplies and gold if enough is available.
		ui32& gold = armies.gold[armyIndex];
		if (inOwnerTerritory && gold < capacity)
		{
			ui64 payment = (ui64)army_daily_consumption(army_supply_basis(world, (ui16)armyIndex)) * price * 2;
			ui32 paid = (ui32)ia_min(ia_min(payment, (ui64)(capacity - gold)), (ui64)player.treasury);
			player.treasury -= paid;
			player.expenses += paid;
			gold += paid;
		}

		// Purchase: goods from the market goods of the settlement the army stands in (its location equals the settlement's; besieged or not), up to the supplies capacity, as far as its gold goes.
		// Same owner only for now (friendly stub). The seller's owner gets the whole payment, and the goods sold count as trade volume.
		if (inOwnerTerritory && supplies < capacity && armies.location[armyIndex] == world.entities->settlements.location[settlementIndex])
		{
			world_entity_settlements& settlements = world.entities->settlements;
			ui32& marketGoods = settlements.trade[settlementIndex].market_goods;
			ui64 affordable = price > 0 ? gold / price : 0xFFFFFFFF;
			ui32 bought = (ui32)ia_min(ia_min((ui64)(capacity - supplies), affordable), (ui64)marketGoods);
			ui32 cost = bought * price;
			marketGoods -= bought;
			settlement_trade_volume_add(settlements, settlementIndex, bought);
			gold -= cost;
			world_player_state& seller = world.player_states[settlements.owner[settlementIndex]];
			seller.treasury += cost;
			seller.income += cost;
			supplies += bought;
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
			army_enforce_carry_capacity(world, (ui16)armyIndex);
			continue;
		}

		// Desertion: each troop type loses the unfunded fraction of the day's consumption (rounded down).

		// The army is destroyed when its last man leaves: stop at once, its slot is free.
		entity_guid army = armies.guids[armyIndex];
		army_troop_count troops = armies.composition[armyIndex];
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
