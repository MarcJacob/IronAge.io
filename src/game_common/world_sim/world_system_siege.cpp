// Siege systems: armies joining and leaving sieges, siege battles (assault / sally), siege end and the attrition of besieged settlements.

#include "game_common/match/world.h"
#include "world_sim.h"
#include "world_sim_battle.h"
#include "world_sim_population.h"

// Share of its population and of each local troop count a starving besieged settlement loses every growth tick (at least 1 while there is any).
constexpr ui32 SIEGE_ATTRITION_PERCENT = 5;

// Takes the army at the given slot out of a siege's list. The slots after it move down one, keeping the joining order.
static void siege_remove_army_slot(world_entity_sieges::state& siege, ui8 slot)
{
	for (ui8 next = slot + 1; next < siege.army_count; next++) siege.armies[next - 1] = siege.armies[next];
	siege.army_count--;
}

bool world_siege_add_army(match_world_state& world, entity_guid siege, entity_guid army)
{
	ASSERT(world_entity_is_valid(world, siege) && siege.get_type() == ENTITY_TYPE::SIEGE);
	ASSERT(world_entity_is_valid(world, army) && army.get_type() == ENTITY_TYPE::ARMY);
	world_entity_sieges::state& state = world.entities.sieges.states[siege.get_index()];
	world_entity_armies& armies = world.entities.armies;
	ui16 armyIndex = army.get_index();

	if (state.army_count >= SIEGE_MAX_ARMIES) return false;

	state.armies[state.army_count] = army;
	state.army_count++;
	armies.sieges[armyIndex] = siege;

	// An assault or sally in progress: the newcomer joins the attackers.
	if (world_entity_is_valid(world, state.battle) && !armies.battles[armyIndex].is_valid()) battle_join(world, state.battle, army, 0);
	return true;
}

void world_siege_remove_army(match_world_state& world, entity_guid army)
{
	ASSERT(world_entity_is_valid(world, army) && army.get_type() == ENTITY_TYPE::ARMY);
	world_entity_armies& armies = world.entities.armies;
	ui16 armyIndex = army.get_index();

	entity_guid siege = armies.sieges[armyIndex];
	armies.sieges[armyIndex] = INVALID_ENTITY_GUID;
	if (!world_entity_is_valid(world, siege)) return;

	world_entity_sieges::state& state = world.entities.sieges.states[siege.get_index()];
	for (ui8 slot = 0; slot < state.army_count; slot++)
	{
		if (state.armies[slot] == army)
		{
			siege_remove_army_slot(state, slot);
			break;
		}
	}

	if (state.army_count == 0) world_destroy_siege(world, siege);
}

void world_siege_start_battle(match_world_state& world, entity_guid siege, bool defense_bonus)
{
	ASSERT(world_entity_is_valid(world, siege) && siege.get_type() == ENTITY_TYPE::SIEGE);
	world_entity_sieges::state& state = world.entities.sieges.states[siege.get_index()];
	world_entity_armies& armies = world.entities.armies;

	entity_guid defenders = world_spawn_settlement_defenders(world, state.settlement);

	// Fought at the settlement. Same siege outcome rules as any siege battle (see world_system_battles).
	world_entity_battles::state start = {};
	start.location = state.location;
	start.siege_settlement = state.settlement;
	start.siege_owner = state.commander;
	start.siege_defenders = defenders;
	start.defense_bonus = defense_bonus ? 1 : 0;
	entity_guid battle = world_spawn_battle(world, start);

	// Armies destroyed earlier this tick may still be listed (pruned by world_system_sieges): only the ones still in the siege join.
	for (ui8 slot = 0; slot < state.army_count; slot++)
	{
		entity_guid army = state.armies[slot];
		if (!world_entity_is_valid(world, army) || !(armies.sieges[army.get_index()] == siege)) continue;
		if (armies.battles[army.get_index()].is_valid()) continue; // Already fighting elsewhere.

		battle_join(world, battle, army, 0);
	}
	battle_join(world, battle, defenders, 1);

	state.battle = battle;
}

// Removes the siege's share of a count, at least 1 while there is any.
template<typename Count>
static inline void siege_attrition(Count& count)
{
	if (count == 0) return;

	Count lost = (Count)((ui64)count * SIEGE_ATTRITION_PERCENT / 100);
	if (lost == 0) lost = 1;
	count = (Count)(count - lost);
}

void world_system_sieges(match_world_state& world)
{
	world_entity_sieges& sieges = world.entities.sieges;
	world_entity_settlements& settlements = world.entities.settlements;
	world_entity_armies& armies = world.entities.armies;

	for (ui32 siegeIndex = 0; siegeIndex < sieges.max_count; siegeIndex++)
	{
		if (!sieges.guids[siegeIndex].is_valid()) continue;

		entity_guid siege = sieges.guids[siegeIndex];
		world_entity_sieges::state& state = sieges.states[siegeIndex];

		// Armies destroyed since (battle, merge, settling, disbanding) are no longer in it.
		for (i32 slot = (i32)state.army_count - 1; slot >= 0; slot--)
		{
			entity_guid army = state.armies[slot];
			if (!world_entity_is_valid(world, army) || !(armies.sieges[army.get_index()] == siege)) siege_remove_army_slot(state, (ui8)slot);
		}

		// The siege ends once its battle is over (whoever won), when the settlement is gone or changed hands, or with no army left.
		bool battleOver = state.battle.is_valid() && !world_entity_is_valid(world, state.battle);
		bool settlementLost = !world_entity_is_valid(world, state.settlement) || settlements.owners[state.settlement.get_index()] != state.defender;
		if (battleOver || settlementLost || state.army_count == 0)
		{
			world_destroy_siege(world, siege);
			continue;
		}

		// Starvation, on the settlement's growth ticks (same staggering as population growth), counted since the siege began.
		if ((state.settlement.get_extra() + world.age) % GROWTH_TICK_PERIOD != 0) continue;
		if (state.growth_ticks < SIEGE_SURRENDER_GROWTH_TICKS) state.growth_ticks++;
		if (state.growth_ticks <= SIEGE_STARVATION_GRACE_GROWTH_TICKS) continue;

		ui16 settlementIndex = state.settlement.get_index();
		ui32& population = settlements.populations[settlementIndex];
		settlement_troops& pool = settlements.local_troops[settlementIndex];

		// Surrender: every defender left dies (population kept at 1, as 0 would stop its growth for good) and the commander takes the settlement,
		// as when an assault is won (see world_system_battles). Not during an assault or sally: that battle decides instead.
		if (state.growth_ticks >= SIEGE_SURRENDER_GROWTH_TICKS && !world_entity_is_valid(world, state.battle))
		{
			population = 1;
			pool.men_at_arms = 0;
			pool.horsemen = 0;
			pool.knights = 0;
			settlements.owners[settlementIndex] = state.commander;
			world_destroy_siege(world, siege);
			continue;
		}

		if (population > 1) siege_attrition(population); // From 2 up, what is lost is always less than the population.
		siege_attrition(pool.men_at_arms);
		siege_attrition(pool.horsemen);
		siege_attrition(pool.knights);
	}
}
