// Main implementation file for the Game Common code, which mostly covers starting and simulating a match.
// Used by client and game server to synchronize with eachother using a Lockstep strategy.

#include "core.h"

#include "game_common_internals.h"

#include "game_common/match/match.h"
#include "game_common/match/start_params.h"
#include "game_common/match/commands.h"

// Unity compile game common components.
#include "match_commands.cpp"
#include "match_AI_logic.cpp"
#include "game_common_tests.cpp"

// WORLD MEMORY LAYOUT PROPERTIES

constexpr ui32 SETTLEMENTS_PER_REGION = 10;
constexpr ui32 MICRO_ENTITIES_PER_TYPE_PER_REGION = 32;

bool world_alloc_settlements(mem_arena& memory, match_world_state& world, ui32 count)
{
	world_entity_settlements& settlements = world.entities.settlements;

	settlements.max_count = count;
	settlements.active_count = 0;

	settlements.guids = memory.alloc<entity_guid>(count);
	if (settlements.guids == nullptr) return false;

	settlements.owners = memory.alloc<match_player_id>(count);
	if (settlements.owners == nullptr) return false;

	settlements.locations = memory.alloc<world_location>(count);
	if (settlements.locations == nullptr) return false;

	settlements.populations = memory.alloc<ui32>(count);
	if (settlements.populations == nullptr) return false;

	settlements.local_wealth = memory.alloc<ui32>(count);
	if (settlements.local_wealth == nullptr) return false;

	settlements.tier = memory.alloc<ui8>(count);
	if (settlements.tier == nullptr) return false;

	settlements.trade_attractivity = memory.alloc<ui8>(count);
	if (settlements.trade_attractivity == nullptr) return false;

	settlements.area_influence = memory.alloc<ui8>(count);
	if (settlements.area_influence == nullptr) return false;

	return true;
}

bool world_alloc_caravans(mem_arena& memory, match_world_state& world, ui32 count)
{
	world_entity_caravans& caravans = world.entities.caravans;

	caravans.max_count = count;

	caravans.guids = memory.alloc<entity_guid>(count);
	if (caravans.guids == nullptr) return false;

	caravans.locations = memory.alloc<world_location>(count);
	if (caravans.locations == nullptr) return false;

	caravans.movements = memory.alloc<world_entity_movement>(count);
	if (caravans.movements == nullptr) return false;

	caravans.origin_settlements = memory.alloc<entity_guid>(count);
	if (caravans.origin_settlements == nullptr) return false;

	caravans.dest_settlements = memory.alloc<entity_guid>(count);
	if (caravans.dest_settlements == nullptr) return false;

	return true;
}

bool world_alloc_armies(mem_arena& memory, match_world_state& world, ui32 count)
{
	world_entity_armies& armies = world.entities.armies;

	armies.max_count = count;

	armies.guids = memory.alloc<entity_guid>(count);
	if (armies.guids == nullptr) return false;

	armies.owners = memory.alloc<match_player_id>(count);
	if (armies.owners == nullptr) return false;

	armies.locations = memory.alloc<world_location>(count);
	if (armies.locations == nullptr) return false;

	armies.movements = memory.alloc<world_entity_movement>(count);
	if (armies.movements == nullptr) return false;

	armies.compositions = memory.alloc<world_entity_armies::comp>(count);
	if (armies.compositions == nullptr) return false;

	armies.action_targets = memory.alloc<entity_guid>(count);
	if (armies.action_targets == nullptr) return false;

	armies.arrows_targets = memory.alloc<entity_guid>(count);
	if (armies.arrows_targets == nullptr) return false;

	return true;
}

match_world_state* world_state_init(mem_arena& memory, world_dimensions size_regions)
{
	// Estimate size:
	// - A terrain of the given size in regions
	// - N settlements per region
	// - X micro entities of each type per region.

	ui64 memoryStart = memory.allocated_count;
	ui16 regionCount = size_regions.x * size_regions.y;

	match_world_state* newWorld = memory.alloc<match_world_state>();
	if (newWorld == nullptr) goto WORLD_INIT_FAIL;

	newWorld->terrain.size_regions = size_regions;
	newWorld->terrain.size_tiles = size_regions * world_terrain::REGION_SIZE;

	newWorld->terrain.regions = memory.alloc<world_terrain::region>(regionCount);
	if (newWorld->terrain.regions == nullptr) goto WORLD_INIT_FAIL;

	// Alloc entities
	{
		const ui32 settlement_count = SETTLEMENTS_PER_REGION * regionCount;
		const ui32 micro_entity_count = MICRO_ENTITIES_PER_TYPE_PER_REGION * regionCount;

		if (!world_alloc_settlements(memory, *newWorld, settlement_count)) goto WORLD_INIT_FAIL;
		if (!world_alloc_caravans(memory, *newWorld, micro_entity_count)) goto WORLD_INIT_FAIL;
		if (!world_alloc_armies(memory, *newWorld, micro_entity_count)) goto WORLD_INIT_FAIL;
	}

	return newWorld;

WORLD_INIT_FAIL:
	memory.allocated_count = memoryStart;
	return nullptr;
}

// Spawns a new settlement in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid match_spawn_settlement(game_match& match, const world_entity_settlements::single& start_state)
{
	ASSERT(match.world != nullptr);
	match_world_state& world = *match.world;

	ui16 freeIndex = 0;
	while (freeIndex < world.entities.settlements.max_count && world.entities.settlements.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities.settlements.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::SETTLEMENT, freeIndex, match.tick % 0x1000); // 12-bit extra field.

	world.entities.settlements.guids[freeIndex] = newID;
	world.entities.settlements.active_count++;

	// Apply start state.
	world.entities.settlements.owners[freeIndex] = start_state.owner;
	world.entities.settlements.locations[freeIndex] = start_state.location;
	world.entities.settlements.populations[freeIndex] = start_state.population;
	world.entities.settlements.local_wealth[freeIndex] = start_state.local_wealth;
	world.entities.settlements.tier[freeIndex] = start_state.tier;
	world.entities.settlements.trade_attractivity[freeIndex] = start_state.trade_attractivity;
	world.entities.settlements.area_influence[freeIndex] = start_state.area_influence;

	return newID;
}

// Spawns a new caravan in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid match_spawn_caravan(game_match& match, const world_entity_caravans::single& start_state)
{
	ASSERT(match.world != nullptr);
	match_world_state& world = *match.world;

	ui16 freeIndex = 0;
	while (freeIndex < world.entities.caravans.max_count && world.entities.caravans.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities.caravans.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::CARAVAN, freeIndex, match.tick % 0x100); // 8-bit extra field.

	world.entities.caravans.guids[freeIndex] = newID;
	world.entities.caravans.active_count++;

	// Apply start state.
	world.entities.caravans.locations[freeIndex] = start_state.location;
	world.entities.caravans.movements[freeIndex] = start_state.movement;
	world.entities.caravans.origin_settlements[freeIndex] = start_state.origin_settlement;
	world.entities.caravans.dest_settlements[freeIndex] = start_state.dest_settlement;

	return newID;
}

// Spawns a new army in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid match_spawn_army(game_match& match, const world_entity_armies::single& start_state)
{
	ASSERT(match.world != nullptr);
	match_world_state& world = *match.world;

	ui16 freeIndex = 0;
	while (freeIndex < world.entities.armies.max_count && world.entities.armies.guids[freeIndex].is_valid())
	{
		freeIndex++;
	}

	ASSERT(freeIndex < world.entities.armies.max_count);

	// Compute GUID (stored and returned value are the same).
	entity_guid newID = entity_guid_new(ENTITY_TYPE::ARMY, freeIndex, match.tick % 0x100); // 8-bit extra field.

	world.entities.armies.guids[freeIndex] = newID;
	world.entities.armies.active_count++;

	// Apply start state.
	world.entities.armies.owners[freeIndex] = start_state.owner;
	world.entities.armies.locations[freeIndex] = start_state.location;
	world.entities.armies.movements[freeIndex] = start_state.movement;
	world.entities.armies.compositions[freeIndex] = start_state.composition;
	world.entities.armies.action_targets[freeIndex] = start_state.action_target;
	world.entities.armies.arrows_targets[freeIndex] = start_state.arrows_target;

	return newID;
}

void match_destroy_army(game_match& match, entity_guid army)
{
	ASSERT(match_entity_is_valid(match, army) && army.get_type() == ENTITY_TYPE::ARMY);
	world_entity_armies& armies = match.world->entities.armies;

	// Same pattern as despawned caravans: the other property arrays keep stale values, every loop skips slots with an invalid GUID.
	armies.guids[army.get_index()] = INVALID_ENTITY_GUID;
	armies.active_count--;
}

void match_destroy_settlement(game_match& match, entity_guid settlement)
{
	ASSERT(match_entity_is_valid(match, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	world_entity_settlements& settlements = match.world->entities.settlements;

	// Same pattern as destroyed armies: the other property arrays keep stale values, every loop skips slots with an invalid GUID.
	settlements.guids[settlement.get_index()] = INVALID_ENTITY_GUID;
	settlements.active_count--;
}

void match_settlement_decrease_population(game_match& match, entity_guid settlement, ui32 amount)
{
	ASSERT(match_entity_is_valid(match, settlement) && settlement.get_type() == ENTITY_TYPE::SETTLEMENT);
	ui32& population = match.world->entities.settlements.populations[settlement.get_index()];

	population = amount < population ? population - amount : 0;
	if (population == 0)
	{
		match_destroy_settlement(match, settlement);
	}
}

world_location world_get_entity_location(game_match& match, entity_guid entity)
{
	ENTITY_TYPE type = entity.get_type();

	switch (type)
	{
	case ENTITY_TYPE::SETTLEMENT:
		return match.world->entities.settlements.locations[entity.get_index()];
	case ENTITY_TYPE::CARAVAN:
		return match.world->entities.caravans.locations[entity.get_index()];
	case ENTITY_TYPE::ARMY:
		return match.world->entities.armies.locations[entity.get_index()];
	default:
		// .. TODO(Marc): Need to be able to report error on the match / world level somehow. Provide a function pointer ?
		return INVALID_WORLD_LOCATION;
	}
}

bool match_entity_is_valid(const game_match& match, entity_guid entity)
{
	if (!entity.is_valid()) return false;

	const world_entity_container* container;

	switch (entity.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
		container = &match.world->entities.settlements;
		break;
	case ENTITY_TYPE::CARAVAN:
		container = &match.world->entities.caravans;
		break;
	case ENTITY_TYPE::ARMY:
		container = &match.world->entities.armies;
		break;
	default:
		return false;
	}

	// The stored GUID is zero for inactive slots, and differs in its extra bits if the slot was reused by a newer entity.
	ui16 index = entity.get_index();
	return index < container->max_count && container->guids[index] == entity;
}

bool query_entity_state_settlement(const game_match& match, entity_guid entity, world_entity_settlements::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::SETTLEMENT || !match_entity_is_valid(match, entity)) return false;

	const world_entity_settlements& settlements = match.world->entities.settlements;
	ui16 i = entity.get_index();

	out_state = {
		.owner = settlements.owners[i],
		.location = settlements.locations[i],
		.population = settlements.populations[i],
		.local_wealth = settlements.local_wealth[i],
		.tier = settlements.tier[i],
		.trade_attractivity = settlements.trade_attractivity[i],
		.area_influence = settlements.area_influence[i],
	};
	return true;
}

bool query_entity_state_caravan(const game_match& match, entity_guid entity, world_entity_caravans::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::CARAVAN || !match_entity_is_valid(match, entity)) return false;

	const world_entity_caravans& caravans = match.world->entities.caravans;
	ui16 i = entity.get_index();

	out_state = {
		.location = caravans.locations[i],
		.movement = caravans.movements[i],
		.origin_settlement = caravans.origin_settlements[i],
		.dest_settlement = caravans.dest_settlements[i],
	};
	return true;
}

bool match_caravan_is_owned_by(const game_match& match, entity_guid caravan, match_player_id player)
{
	world_entity_caravans::single state;
	if (!query_entity_state_caravan(match, caravan, state)) return false;

	world_entity_settlements::single settlement;
	if (query_entity_state_settlement(match, state.origin_settlement, settlement) && settlement.owner == player) return true;
	return query_entity_state_settlement(match, state.dest_settlement, settlement) && settlement.owner == player;
}

bool query_entity_state_army(const game_match& match, entity_guid entity, world_entity_armies::single& out_state)
{
	if (entity.get_type() != ENTITY_TYPE::ARMY || !match_entity_is_valid(match, entity)) return false;

	const world_entity_armies& armies = match.world->entities.armies;
	ui16 i = entity.get_index();

	out_state = {
		.owner = armies.owners[i],
		.location = armies.locations[i],
		.movement = armies.movements[i],
		.composition = armies.compositions[i],
		.action_target = armies.action_targets[i],
		.arrows_target = armies.arrows_targets[i],
	};
	return true;
}

constexpr ui64 match_get_required_mem(game_match_start_params& params)
{
	return MiB(32); // For now let's just keep it static at 32 mibibyte.
}

bool match_start(mem_arena& match_mem, time_ms start_time, game_match_start_params& params, game_match& out_match)
{
	if (params.world_size_regions.x == 0 || params.world_size_regions.y == 0)
	{
		return false; // Invalid params.
	}

	// TODO: Instead of checking total memory size of the arena, we should instead check how much it has *left* in case it already contains stuff (not recommended).
	if (match_mem.mem_size < match_get_required_mem(params))
	{
		return false; // Not enough memory.
	}

	out_match = {};
	out_match.start_params = &params;
	out_match.memory = &match_mem;
	out_match.main_rand_gen = rand_generator_create_32(params.random_seed);

	out_match.start_time = start_time;

	// Initialize match world state.
	out_match.world = world_state_init(match_mem, params.world_size_regions);
	if (out_match.world == nullptr) return false;

	// Apply start params to world state

	for (ui32 settlementIndex = 0; settlementIndex < params.start_settlement_count; settlementIndex++)
	{
		world_entity_settlements::single& settlementStartState = params.get_settlement_start_state(settlementIndex);
		entity_guid settlementGUID = match_spawn_settlement(out_match, settlementStartState);

		// Temp: Spawn an army near every settlement.
		world_location startLoc = world_get_entity_location(out_match, settlementGUID) + vec2i{ -10, 10 };

		world_entity_armies::single newArmy = {
			.owner = settlementStartState.owner,
			.location = startLoc,
			.movement = {
				.travel_speed = 50,
				.move_target = startLoc,
			},
			.composition = {
				.levies = 100,
			},
		};

		entity_guid armyGUID = match_spawn_army(out_match, newArmy);
	}

	return true;
}

// ATTACK RESOLUTION HELPERS

// An army reaches its attack target when within this many tiles of it.
constexpr i32 ATTACK_REACH_TILES = 5;

// Squared distance between two world locations, in integer math.
static inline i32 world_location_dist_squared(world_location a, world_location b)
{
	i32 dx = (i32)a.x - (i32)b.x;
	i32 dy = (i32)a.y - (i32)b.y;
	return dx * dx + dy * dy;
}

static inline ui64 army_total_manpower(const world_entity_armies::comp& composition)
{
	return (ui64)composition.levies + composition.archers + composition.men_at_arms + composition.horsemen + composition.knights;
}

template<typename Count>
static inline void army_take_manpower(Count& count, ui64& remaining)
{
	ui64 taken = remaining < count ? remaining : count;
	count = (Count)(count - taken);
	remaining -= taken;
}

// Removes manpower from an army, cheapest units first.
static inline void army_remove_manpower(world_entity_armies::comp& composition, ui64 amount)
{
	army_take_manpower(composition.levies, amount);
	army_take_manpower(composition.archers, amount);
	army_take_manpower(composition.men_at_arms, amount);
	army_take_manpower(composition.horsemen, amount);
	army_take_manpower(composition.knights, amount);
}

// Effective manpower of an army for a fight, in hundredths: manpower x a random 50% to 150% advantage.
static inline ui64 army_roll_effective_manpower(game_match& match, ui64 manpower)
{
	ui32 advantagePercent = match.main_rand_gen.next_range<ui32>(50, 151);
	if (advantagePercent > 150) advantagePercent = 150; // The range function may land on its max.
	return manpower * advantagePercent;
}

// Resolves an attack of an army that reached its target. The attacker is destroyed here if it loses.
static void match_resolve_attack(game_match& match, entity_guid attacker, entity_guid target)
{
	world_entity_settlements& settlements = match.world->entities.settlements;
	world_entity_caravans& caravans = match.world->entities.caravans;
	world_entity_armies& armies = match.world->entities.armies;

	ui16 attackerIndex = attacker.get_index();
	ui16 targetIndex = target.get_index();
	match_player_id attackerOwner = armies.owners[attackerIndex];

	switch (target.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
		settlements.owners[targetIndex] = attackerOwner;
		break;
	case ENTITY_TYPE::CARAVAN:
	{
		// The caravan is redirected to the closest settlement of the attacker. Without one, it is left alone.
		i32 bestDist = 0;
		entity_guid bestSettlement = INVALID_ENTITY_GUID;
		for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
		{
			if (!settlements.guids[settlementIndex].is_valid() || settlements.owners[settlementIndex] != attackerOwner) continue;

			i32 dist = world_location_dist_squared(caravans.locations[targetIndex], settlements.locations[settlementIndex]);
			if (!bestSettlement.is_valid() || dist < bestDist)
			{
				bestDist = dist;
				bestSettlement = settlements.guids[settlementIndex];
			}
		}
		if (bestSettlement.is_valid()) caravans.dest_settlements[targetIndex] = bestSettlement;
		break;
	}
	case ENTITY_TYPE::ARMY:
	{
		ui64 attackerManpower = army_total_manpower(armies.compositions[attackerIndex]);
		ui64 defenderManpower = army_total_manpower(armies.compositions[targetIndex]);

		// Roll attacker first, then defender. Manpower is assumed to stay under ~1e8 so the loss computation below can't overflow.
		ui64 attackerEffective = army_roll_effective_manpower(match, attackerManpower);
		ui64 defenderEffective = army_roll_effective_manpower(match, defenderManpower);

		if (attackerEffective == defenderEffective)
		{
			match_destroy_army(match, attacker);
			match_destroy_army(match, target);
		}
		else
		{
			// The winner, whichever side, loses winner_manpower x (loser_eff / winner_eff), rounded up so a fight always costs something.
			// The ratio is below 1 here, but the loss is still capped at the winner's manpower.
			bool attackerWins = attackerEffective > defenderEffective;
			entity_guid winner = attackerWins ? attacker : target;
			entity_guid loser = attackerWins ? target : attacker;
			ui64 winnerManpower = attackerWins ? attackerManpower : defenderManpower;
			ui64 winnerEffective = attackerWins ? attackerEffective : defenderEffective;
			ui64 loserEffective = attackerWins ? defenderEffective : attackerEffective;

			ui64 loss = (winnerManpower * loserEffective + winnerEffective - 1) / winnerEffective;
			if (loss > winnerManpower) loss = winnerManpower;

			match_destroy_army(match, loser);
			army_remove_manpower(armies.compositions[winner.get_index()], loss);
			if (loss == winnerManpower) match_destroy_army(match, winner);
		}
		break;
	}
	default:
		break;
	}
}

// Advances time in a match's world simulation. Requires the aggregated inputs / commands to apply into the tick.
void match_tick(game_match& match, const match_tick_commands& commands)
{
	match.tick++;

	// Start by applying all input commands sequentially.
	match_command_apply_all(match, commands);

	// Run world behavior.
	match_world_state& world = *match.world;

	// TEMP WORLD SIMULATION CODE
	// TODO(Marc): Move to a dedicated subcomponent of the game common code. Make it multithreadable.

	// Pseudo-code / Sequence:
	// - 1: Settlement growth / eco tick (log growth based on local wealth / population).
	// - 2: Caravans & Army movement target determination (caravans flee from nearby non-friendly armies or go towards destination, armies go to attack target or move target if set, if not engaged).
	// - 3: Caravans & Army movement (Move towards target at constant speed)
	// - 4: Army engagement / fighting (If army touches its target (ENTITY_WORLD_SIZE), start engagement.
	//		- Engagement system: Create Engagement entity.
	// - 5: Engagement entities: caravans are instantly flagged for destruction, armies take manpower damage and deal manpower damage based on some simple formula. If no manpower left, get destroyed.
	// - 6: Caravans arrival in target settlement (Increase origin settlement local wealth by 1) then get destroyed.
	// - 7: Caravans automatically spawn from settlements if 1000 / local wealth % match tick == 0. Destination is chosen at as much random as possible without libc deterministically.

	world_entity_settlements& settlements = world.entities.settlements;
	world_entity_caravans& caravans = world.entities.caravans;
	world_entity_armies& armies = world.entities.armies;

	// Temp: Settlements have a 1% chance per tick to grow by 5% of their population (at least 1).
	for (ui32 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
	{
		if (!settlements.guids[settlementIndex].is_valid() || settlements.populations[settlementIndex] == 0) continue;

		if (match.main_rand_gen.next_range(0.f, 1.f) < 0.01f)
		{
			ui32 growth = settlements.populations[settlementIndex] * 5 / 100;
			settlements.populations[settlementIndex] += growth > 0 ? growth : 1;
		}
	}

	// Temp: Randomly spawn caravans at settlements.
	for (ui32 settlementIndex = 0; settlementIndex < world.entities.settlements.max_count; settlementIndex++)
	{
		if (!settlements.guids[settlementIndex].is_valid()) continue;

		// Randomly spawn caravan if there are more settlements on the map.
		if (settlements.active_count > 1 && match.main_rand_gen.next_range(0.f, 1.f) < 0.001f)
		{
			// Pick a random active settlement other than this one. Slots can be empty now that settlements get destroyed.
			ui16 destRandOrdinal = match.main_rand_gen.next_range<ui16>(0, settlements.active_count - 1) % (settlements.active_count - 1);
			ui16 destRandIndex = 0;
			for (;; destRandIndex++)
			{
				if (destRandIndex == settlementIndex || !settlements.guids[destRandIndex].is_valid()) continue;
				if (destRandOrdinal == 0) break;
				destRandOrdinal--;
			}

			world_entity_caravans::single newCaravan = {
				.location = settlements.locations[settlementIndex],
				.movement =
				{
					.travel_speed = 40,
					.move_target = settlements.locations[destRandIndex],
				},
				.origin_settlement = settlements.guids[settlementIndex],
				.dest_settlement = settlements.guids[destRandIndex],
			};
			match_spawn_caravan(match, newCaravan);
		}
	}

	// Make sure caravans have their movement target set to their destination settlement.
	for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
	{
		if (!caravans.guids[caravanIndex].is_valid()) continue;

		// Despawn caravans whose destination settlement was destroyed.
		if (!match_entity_is_valid(match, caravans.dest_settlements[caravanIndex]))
		{
			caravans.guids[caravanIndex] = INVALID_ENTITY_GUID;
			caravans.active_count--;
			continue;
		}

		const world_location& destLoc = settlements.locations[caravans.dest_settlements[caravanIndex].get_index()];
		caravans.movements[caravanIndex].move_target = destLoc;
	}

	// Armies committed to an attack chase their target's current location. They stop if the target no longer exists.
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || !armies.action_targets[armyIndex].is_valid()) continue;

		if (match_entity_is_valid(match, armies.action_targets[armyIndex]))
		{
			armies.movements[armyIndex].move_target = world_get_entity_location(match, armies.action_targets[armyIndex]);
		}
		else
		{
			armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
			armies.movements[armyIndex].move_target = armies.locations[armyIndex];
		}
	}

	// TEMP: Move armies and caravans towards their destination.
	auto process_entity_movement = [&](world_entity_movement& movement, world_location& location)
		{
			if (movement.move_target == location) return;

			vec2<i32> travelVec = (vec2<i32>)(movement.move_target) - location;

			// Figure out a distribution over the 0, 256 range of both axis representing an absolute travel direction.
			// The relative values of the axis will determine which will get more of the movement.
			vec2<ui8> travelDir;
			if (travelVec.y == 0) travelDir = { 255, 0 };
			else if (travelVec.x == 0) travelDir = { 0, 255 };
			else
			{
				ui16 travelAbsX = ia_abs(travelVec.x), travelAbsY = ia_abs(travelVec.y);
				if (travelAbsX == travelAbsY) travelDir = { 127, 127 };
				// Get a reasonably accurate measure of how much larger one is compared to the other and use it to determine distribution.
				if (travelAbsX > travelAbsY)
				{
					ui32 xMult = travelAbsX * 1000 / travelAbsY;
					ui8 parts = 255 * 1000 / (xMult + 1000);
					travelDir.x = 255 - parts;
					travelDir.y = parts;
				}
				else
				{
					ui32 yMult = travelAbsY * 1000 / travelAbsX;
					ui8 parts = 255 * 1000 / (yMult + 1000);
					travelDir.y = 255 - parts;
					travelDir.x = parts;
				}
			}

			// Turn the travel vector into a signs vector.
			travelVec = { (travelVec.x >= 0) ? 1 : -1, (travelVec.y >= 0) ? 1 : -1 };

			// With absolute travel vector, we can determine where to apply the movement value of the entity.
			i16 xTravel = travelDir.x * 1000 / 255 * movement.travel_speed / 100;
			i16 yTravel = travelDir.y * 1000 / 255 * movement.travel_speed / 100;
			
			i16 newFracX = movement.fractional_loc.x + (i16)xTravel * travelVec.x;
			i16 newFracY = movement.fractional_loc.y + (i16)yTravel * travelVec.y;

			while (newFracX > 100 && location.x < movement.move_target.x) { location.x++; newFracX -= 200; }
			while (newFracX < -100 && location.x > movement.move_target.x) { location.x--; newFracX += 200; }

			while (newFracY > 100 && location.y < movement.move_target.y) { location.y++; newFracY -= 200; }
			while (newFracY < -100 && location.y > movement.move_target.y) { location.y--; newFracY += 200; }

			movement.fractional_loc.x = newFracX * (location.x != movement.move_target.x);
			movement.fractional_loc.y = newFracY * (location.y != movement.move_target.y);
		};

	for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
	{
		if (!caravans.guids[caravanIndex].is_valid()) continue;

		process_entity_movement(caravans.movements[caravanIndex], caravans.locations[caravanIndex]);
	}

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid()) continue;

		process_entity_movement(armies.movements[armyIndex], armies.locations[armyIndex]);
	}

	// Armies that got close enough to their attack target resolve the attack, which ends the chase.
	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid() || !armies.action_targets[armyIndex].is_valid()) continue;

		// An earlier resolution this tick may have destroyed the target.
		entity_guid target = armies.action_targets[armyIndex];
		if (!match_entity_is_valid(match, target)) continue; // Handled by the chase code next tick.

		if (world_location_dist_squared(armies.locations[armyIndex], world_get_entity_location(match, target))
			> ATTACK_REACH_TILES * ATTACK_REACH_TILES) continue;

		armies.action_targets[armyIndex] = INVALID_ENTITY_GUID;
		armies.movements[armyIndex].move_target = armies.locations[armyIndex];
		match_resolve_attack(match, armies.guids[armyIndex], target);
	}

	// Once caravans reach close enough to their destination, despawn them and increase origin & destination local wealth.
	for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
	{
		if (!caravans.guids[caravanIndex].is_valid()) continue;

		const world_location& caravanLoc = caravans.locations[caravanIndex];
		const world_location& destLoc = settlements.locations[caravans.dest_settlements[caravanIndex].get_index()];

		if (vec2_dist_squared(caravanLoc, destLoc) < 5)
		{
			// Despawn caravan.
			caravans.guids[caravanIndex] = INVALID_ENTITY_GUID;
			caravans.active_count--;

			// Both origin and destination town earn 20 + distance in tiles + destination wealth / 10 + origin wealth / 20, read before either is increased.
			// A destroyed origin counts as 0 distance and 0 wealth, and earns nothing.
			ui16 destIndex = caravans.dest_settlements[caravanIndex].get_index();
			bool originValid = match_entity_is_valid(match, caravans.origin_settlements[caravanIndex]);
			ui16 originIndex = originValid ? caravans.origin_settlements[caravanIndex].get_index() : 0;

			ui32 distanceTiles = originValid ? (ui32)ia_sqrt((ui32)world_location_dist_squared(settlements.locations[originIndex], destLoc)) : 0;
			ui32 originWealth = originValid ? settlements.local_wealth[originIndex] : 0;
			ui32 earnings = 20 + distanceTiles + settlements.local_wealth[destIndex] / 10 + originWealth / 20;

			settlements.local_wealth[destIndex] += earnings;
			if (originValid) settlements.local_wealth[originIndex] += earnings;
		}
	}
}
