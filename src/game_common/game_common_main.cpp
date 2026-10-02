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

	// Compute GUID
	world.entities.settlements.guids[freeIndex] = {
		._type = ENTITY_TYPE::SETTLEMENT,
	};
	world.entities.settlements.guids[freeIndex]._macro = {
		._index = freeIndex,
		._extra = (ui16)(match.tick % 0x100),
	};

	world.entities.settlements.active_count++;

	entity_guid newID = { 0 };
	newID._type = ENTITY_TYPE::SETTLEMENT;
	newID._macro._index = freeIndex;
	newID._macro._extra = match.tick % 0xFFFF;

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

	// Compute GUID
	world.entities.caravans.guids[freeIndex] = {
		._type = ENTITY_TYPE::CARAVAN,
	};
	world.entities.caravans.guids[freeIndex]._micro = {
		._index = freeIndex,
		._extra = (ui8)(match.tick % 0xFF),
	};

	world.entities.caravans.active_count++;

	entity_guid newID = { 0 };
	newID._type = ENTITY_TYPE::CARAVAN;
	newID._micro._index = freeIndex;
	newID._micro._extra = match.tick % 0x100;

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

	// Compute GUID
	world.entities.armies.guids[freeIndex] = {
		._type = ENTITY_TYPE::ARMY,
	};
	world.entities.armies.guids[freeIndex]._micro = {
		._index = freeIndex,
		._extra = (ui8)(match.tick % 0xFF),
	};

	world.entities.armies.active_count++;

	entity_guid newID = { 0 };
	newID._type = ENTITY_TYPE::ARMY;
	newID._micro._index = freeIndex;
	newID._micro._extra = match.tick % 0x100;

	// Apply start state.
	world.entities.armies.owners[freeIndex] = start_state.owner;
	world.entities.armies.locations[freeIndex] = start_state.location;
	world.entities.armies.movements[freeIndex] = start_state.movement;
	world.entities.armies.compositions[freeIndex] = start_state.composition;
	world.entities.armies.action_targets[freeIndex] = start_state.action_target;
	world.entities.armies.arrows_targets[freeIndex] = start_state.arrows_target;

	return newID;
}

world_location world_get_entity_location(game_match& match, entity_guid entity)
{
	ENTITY_TYPE type = entity._type;
	
	switch (type)
	{
	case ENTITY_TYPE::SETTLEMENT:
		return match.world->entities.settlements.locations[entity._macro._index];
	case ENTITY_TYPE::CARAVAN:
		return match.world->entities.caravans.locations[entity._micro._index];
	case ENTITY_TYPE::ARMY:
		return match.world->entities.armies.locations[entity._micro._index];
	default:
		// .. TODO(Marc): Need to be able to report error on the match / world level somehow. Provide a function pointer ?
		return INVALID_WORLD_LOCATION;
	}
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
				.travel_speed = 10,
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

	// Temp: Randomly spawn caravans at settlements.
	for (ui32 settlementIndex = 0; settlementIndex < world.entities.settlements.max_count; settlementIndex++)
	{
		if (!settlements.guids[settlementIndex].is_valid()) continue;

		// Randomly spawn caravan if there are more settlements on the map.
		if (settlements.active_count > 1 && match.main_rand_gen.next_range(0.f, 1.f) < 0.001f)
		{
			ui16 destRandIndex = match.main_rand_gen.next_range<ui16>(0, world.entities.settlements.active_count);
			while (destRandIndex == settlementIndex || !settlements.guids[destRandIndex].is_valid())
			{
				destRandIndex = (destRandIndex + 1) % world.entities.settlements.active_count;
			}

			world_entity_caravans::single newCaravan = {
				.location = settlements.locations[settlementIndex],
				.movement =
				{
					.travel_speed = 10,
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

		const world_location& destLoc = settlements.locations[caravans.dest_settlements[caravanIndex]._macro._index];
		caravans.movements[caravanIndex].move_target = destLoc;
	}

	// TEMP: Move armies and caravans towards their destination.
	auto process_entity_movement = [&](const world_entity_movement& movement, world_location& location)
		{
			// Ignore move speed. Manhattan movement.
			// TODO: Actual movement algorithm using fractional tile locations for precise movement.

			if (location.x > movement.move_target.x) location.x--;
			else if (location.x < movement.move_target.x) location.x++;
			if (location.y > movement.move_target.y) location.y--;
			else if (location.y < movement.move_target.y) location.y++;
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

	// Once caravans reach close enough to their destination, despawn them and increase origin & destination local wealth.
	for (ui32 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
	{
		if (!caravans.guids[caravanIndex].is_valid()) continue;

		const world_location& caravanLoc = caravans.locations[caravanIndex];
		const world_location& destLoc = settlements.locations[caravans.dest_settlements[caravanIndex]._macro._index];

		if (vec2_dist_squared(caravanLoc, destLoc) < 5)
		{
			// Despawn caravan.
			caravans.guids[caravanIndex] = INVALID_ENTITY_GUID;
			caravans.active_count--;

			// Increase local wealth in origin and destination town.
			settlements.local_wealth[caravans.dest_settlements[caravanIndex]._macro._index]++;
			settlements.local_wealth[caravans.origin_settlements[caravanIndex]._macro._index]++;
		}
	}
}
