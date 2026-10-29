// Main implementation file for match input command functions.

#include "game_common/match/commands.h"
#include "game_common/match/world/world_terrain.h"
#include "game_common/match/world/world_state.h"

#include "world_sim/world_sim.h"

#include "world_sim/entities/world_sim_entity_worker.h"
#include "world_sim/entities/world_sim_entity_settlement.h"

// BEGIN COMMAND FUNCTION IMPLEMENTATIONS

// Validity check helpers (see the contract in commands.h).

// Records an error in the feedback if there is one. Returns whether the check can stop now (nobody asked for the other errors).
static inline bool command_add_error(match_command_validity_feedback* feedback, COMMAND_ERROR error)
{
	if (feedback == nullptr) return true;

	feedback->error_flags |= error;
	return false;
}

// Same for an invalid path error, keeping the first reason found.
static inline bool command_add_path_error(match_command_validity_feedback* feedback, COMMAND_INVALID_PATH_REASON reason)
{
	if (feedback != nullptr && feedback->invalid_path_reason == COMMAND_INVALID_PATH_NONE) feedback->invalid_path_reason = reason;
	return command_add_error(feedback, COMMAND_ERROR_INVALID_PATH);
}

// Result of a check that went through to its end: valid unless an error was recorded.
static inline bool command_is_valid(const match_command_validity_feedback* feedback)
{
	return feedback == nullptr || feedback->error_flags == 0;
}

// True if the tile (in the world) is land. Entities may path across water but only stand and stop on land.
static bool command_tile_is_land(const world_terrain& terrain, world_location loc)
{
	ui16 regionIndex, tileIndex;
	world_get_tile(terrain, loc, regionIndex, tileIndex);
	return (PATH_TERRAIN_LAND & (1 << (ui8)terrain.regions[regionIndex].terrain_types[tileIndex])) != 0;
}

// SERVER SPAWN PLAYER

bool command_server_spawn_player_validity_check(const match_world_state& world, match_player_id player,
        const match_command_header& command_header, match_command_validity_feedback* feedback)
{
    // Check that command was emitted by the server itself (INVALID player ID).
    if (player != INVALID_MATCH_PLAYER_ID) return false;

    // @TODO(Marc): More checks such as making sure the player doesn't own any entities.
    return true;
}

void command_server_spawn_player_apply(match_world_state& world, match_player_id player,
        const match_command_header& command_header)
{
    if (!command_server_spawn_player_validity_check(world, player, command_header, nullptr)) return;

    const command_data_server_spawn_player& command = command_header.get_command_data<command_data_server_spawn_player>();

    // Spawn start settlement for the player.

    world_entity_settlements::single startSettlement = {
        .owner = command.spawned_player,
        .location = command.spawn_location,
        .pop_unemployed = 200,
        .tax_rate = 5,
    };

    world_spawn_settlement(world, startSettlement);
}


// ENTITY SET MOVE TARGET

// Example of command sanity check function.
bool command_set_entity_move_target_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_set_entity_move_target& command = command_header.get_command_data<command_data_set_entity_move_target>();

	if (!world_entity_is_valid(world, command.entity)) return false;

	// Check that target coordinates are inside the world.
	if (command.move_target.x >= world.terrain->size_tiles.x || command.move_target.y >= world.terrain->size_tiles.y) return false;
	if (!command_tile_is_land(*world.terrain, command.move_target)) { command_add_path_error(feedback, COMMAND_INVALID_PATH_DESTINATION_INVALID); return false; }

	ENTITY_TYPE entityType = command.entity.get_type();
	switch (entityType)
	{
		// With valid types, just need to check that player can control them.
	case ENTITY_TYPE::ARMY:
		if (world.entities->armies.owners[command.entity.get_index()] != player) return false;
		// An army refuses move orders until done raising.
		if (world.entities->armies.raising[command.entity.get_index()]) { command_add_error(feedback, COMMAND_ERROR_ARMY_RAISING); return false; }
		return true;
	default:
		// Command invalid: entity can't move or can't be ordered to move somewhere.
		return false;
	}
}

// Example of a command apply function.
void command_set_entity_move_target_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	const command_data_set_entity_move_target& command = command_header.get_command_data<command_data_set_entity_move_target>();

	if (!world_entity_is_valid(world, command.entity)) return;

	ENTITY_TYPE entityType = command.entity.get_type();

	switch (entityType)
	{
		// With valid types, just need to check that player can control them.
	case ENTITY_TYPE::ARMY:
		if (world.entities->armies.raising[command.entity.get_index()]) break;
		world.entities->armies.movements[command.entity.get_index()].destination = command.move_target;
		// A new move order overrides any attack the army was committed to.
		world.entities->armies.action_targets[command.entity.get_index()] = INVALID_ENTITY_GUID;
		break;
	default:
		// Command invalid: entity can't move or can't be ordered to move somewhere.
		break;
	}

}

// FOUND SETTLEMENT

bool command_found_settlement_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_found_settlement& command = command_header.get_command_data<command_data_found_settlement>();

	// An active settlement is needed to check anything else.
	if (!world_entity_is_valid(world, command.settlement) || command.settlement.get_type() != ENTITY_TYPE::SETTLEMENT)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	ui16 settlementIndex = command.settlement.get_index();
	const world_entity_settlements& settlements = world.entities->settlements;
	if (settlements.owners[settlementIndex] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	// Reported before the target check so the preview can show it without a valid target.
	if (feedback != nullptr) feedback->goods_taken = settlement_colonist_goods(world, settlementIndex, GAME_PARAMETERS.found_settlement_colonists);

	// The target must be a place where the player could found a settlement (it is checked again when the colonists get there).
	if (!world_is_valid_settlement_location(world, command.target, player) && command_add_path_error(feedback, COMMAND_INVALID_PATH_DESTINATION_INVALID)) return false;

	// The colonists are taken out of the settlement's peasants.
	ui32 colonists = GAME_PARAMETERS.found_settlement_colonists;
	ui32 peasants = settlements.pop_peasants[settlementIndex];
	if (feedback != nullptr)
	{
		feedback->population_needed = colonists;
		feedback->population_current = peasants;
	}
	if (peasants < colonists && command_add_error(feedback, COMMAND_ERROR_NOT_ENOUGH_POPULATION)) return false;

	// Room for the worker, and for the settlement it will found.
	const world_entity_workers& workers = world.entities->workers;
	if ((workers.active_count >= workers.max_count || settlements.active_count >= settlements.max_count)
		&& command_add_error(feedback, COMMAND_ERROR_ENTITY_LIMIT_REACHED)) return false;

	return command_is_valid(feedback);
}

// Takes the colonists out of the settlement and spawns the worker carrying them. It walks to the target over the following ticks (see the construction system).
void command_found_settlement_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	// Checked again: earlier commands in the same tick may have changed the settlement or filled the last slots.
	if (!command_found_settlement_validity_check(world, player, command_header, nullptr)) return;

	const command_data_found_settlement& command = command_header.get_command_data<command_data_found_settlement>();

	// The goods share is computed before the colonists leave (it depends on who stays).
	ui16 originIndex = command.settlement.get_index();
	ui32 goods = settlement_colonist_goods(world, originIndex, GAME_PARAMETERS.found_settlement_colonists);
	world.entities->settlements.trade[originIndex].market_goods -= goods;
	ui32 colonists = settlement_remove_peasants(world, originIndex, GAME_PARAMETERS.found_settlement_colonists);

	world_entity_workers::single worker = {};
	worker.origin_entity = command.settlement;
	worker.work_type = WORK_TYPE::FOUND_SETTLEMENT;
	*(found_settlement_data*)worker.action_data.bytes = {
		.target = command.target,
		.colonists = colonists,
		.goods = goods,
	};

	world_spawn_worker(world, worker);
}

// RAISE ARMY

// Cost: raising itself is free, but the army starts with one day of its troops' consumption as supplies, paid by the settlement's owner from the treasury.
// The troops are the raise targets.
static ui32 raise_army_cost(const command_data_raise_army& command)
{
	army_troop_count targets = { command.target_levies, command.target_men_at_arms, command.target_horsemen, command.target_knights };
	return army_daily_consumption(targets);
}

static bool raise_army_can_afford(const match_world_state& world, const command_data_raise_army& command, match_command_validity_feedback* feedback)
{
	match_player_id owner = world.entities->settlements.owners[command.settlement.get_index()];
	if (owner >= world.player_count) return false;

	if (raise_army_cost(command) > world.player_states[owner].treasury && command_add_error(feedback, COMMAND_ERROR_NOT_ENOUGH_TREASURY)) return false;
	return true;
}

// Takes the cost out of the treasury. The new army's supplies are set to it in apply.
static void raise_army_charge(match_world_state& world, const command_data_raise_army& command)
{
	match_player_id owner = world.entities->settlements.owners[command.settlement.get_index()];
	world_player_state& player = world.player_states[owner];
	ui32 cost = raise_army_cost(command);
	player.treasury -= cost;
	player.expenses += cost;
}

bool command_raise_army_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_raise_army& command = command_header.get_command_data<command_data_raise_army>();

	// Nothing else can be checked without a settlement.
	if (!world_entity_is_valid(world, command.settlement) || command.settlement.get_type() != ENTITY_TYPE::SETTLEMENT)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	ui16 settlementIndex = command.settlement.get_index();
	const world_entity_settlements& settlements = world.entities->settlements;

	bool locationInWorld = command.raise_location.x < world.terrain->size_tiles.x && command.raise_location.y < world.terrain->size_tiles.y;
	if (!locationInWorld && command_add_error(feedback, COMMAND_ERROR_TARGET_OUT_OF_BOUNDS)) return false;

	if (settlements.owners[settlementIndex] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	if (command.target_levies == 0 && command.target_men_at_arms == 0 && command.target_horsemen == 0 && command.target_knights == 0
		&& command_add_error(feedback, COMMAND_ERROR_NO_RAISE_TARGET)) return false;

	// Every type, levies included, is drawn from the garrison pool when raising: targets can exceed what the pool holds.

	if (!raise_army_can_afford(world, command, feedback) && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	// The raise location must be in the settlement's territory.
	if (locationInWorld)
	{
		ui16 regionIndex, tileIndex;
		world_get_tile(*world.terrain, command.raise_location, regionIndex, tileIndex);
		if (world.terrain->regions[regionIndex].owners[tileIndex] != settlementIndex && command_add_error(feedback, COMMAND_ERROR_TARGET_OUT_OF_TERRITORY)) return false;
		if (!command_tile_is_land(*world.terrain, command.raise_location) && command_add_path_error(feedback, COMMAND_INVALID_PATH_DESTINATION_INVALID)) return false;
	}

	const world_entity_armies& armies = world.entities->armies;
	if (armies.active_count >= armies.max_count && command_add_error(feedback, COMMAND_ERROR_ENTITY_LIMIT_REACHED)) return false;

	return command_is_valid(feedback);
}

// Spawns an empty army at the raise location. It draws its men from the settlement right away (see world_system_armies_raising).
void command_raise_army_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	// Checked again: earlier commands in the same tick may have changed the settlement or filled the last army slot.
	if (!command_raise_army_validity_check(world, player, command_header, nullptr)) return;

	const command_data_raise_army& command = command_header.get_command_data<command_data_raise_army>();

	ui16 settlementIndex = command.settlement.get_index();
	const world_entity_settlements& settlements = world.entities->settlements;

	world_entity_armies::single newArmy = {};
	newArmy.owner = settlements.owners[settlementIndex];
	newArmy.location = command.raise_location; // The spawn function defaults the destination to the location.
	newArmy.raised_troops = { command.target_levies, command.target_men_at_arms, command.target_horsemen, command.target_knights };
	newArmy.home_settlement = command.settlement;
	newArmy.raising = 1;
	newArmy.supplies = raise_army_cost(command);

	world_spawn_army(world, newArmy);
	raise_army_charge(world, command);
}

// ENTITY SET ATTACK TARGET

bool command_set_entity_attack_target_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_set_entity_attack_target& command = command_header.get_command_data<command_data_set_entity_attack_target>();

	if (!world_entity_is_valid(world, command.attacker_entity)) return false;
	if (command.attacker_entity.get_type() != ENTITY_TYPE::ARMY) return false;
	if (world.entities->armies.owners[command.attacker_entity.get_index()] != player) return false;
	if (world.entities->armies.raising[command.attacker_entity.get_index()]) { command_add_error(feedback, COMMAND_ERROR_ARMY_RAISING); return false; }

	// Targets owned by the sender can't be attacked (see match_caravan_is_owned_by for caravans).
	ui16 targetIndex = command.target_entity.get_index();
	const world_entities& entities = *world.entities;
	switch (command.target_entity.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
		return entities.settlements.owners[targetIndex] != player;
	case ENTITY_TYPE::ARMY:
		return entities.armies.owners[targetIndex] != player;
	default:
		return false;
	}
}

// Commits the army to chasing the target. The chase and its resolution are handled by the match tick.
void command_set_entity_attack_target_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	// Checked again: earlier commands in the same tick may have destroyed the attacker or the target.
	if (!command_set_entity_attack_target_validity_check(world, player, command_header, nullptr)) return;

	const command_data_set_entity_attack_target& command = command_header.get_command_data<command_data_set_entity_attack_target>();

	world.entities->armies.action_targets[command.attacker_entity.get_index()] = command.target_entity;
}

// BUILD ROAD

bool command_build_road_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_build_road& command = command_header.get_command_data<command_data_build_road>();

    // Check validity of entity, it must be an active settlement. Nothing else can be checked without one.
	if (!world_entity_is_valid(world, command.settlement) || command.settlement.get_type() != ENTITY_TYPE::SETTLEMENT)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

    // The settlement must be owned by the player emitting the command.
	ui16 settlementIndex = command.settlement.get_index();
	const world_entity_settlements& settlements = world.entities->settlements;
	if (settlements.owners[settlementIndex] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

    // The origin and destination must be in the world, and not be equal to eachother.
    if (command.origin == command.destination && command_add_path_error(feedback, COMMAND_INVALID_PATH_ORIGIN_IS_DESTINATION)) return false;

	const world_terrain& terrain = *world.terrain;
	bool originInWorld = command.origin.x < terrain.size_tiles.x && command.origin.y < terrain.size_tiles.y;
	bool destinationInWorld = command.destination.x < terrain.size_tiles.x && command.destination.y < terrain.size_tiles.y;
	if ((!originInWorld || !destinationInWorld) && command_add_error(feedback, COMMAND_ERROR_TARGET_OUT_OF_BOUNDS)) return false;

	// The road starts from the settlement itself, or from a road tile in its territory.
	if (originInWorld && !(command.origin == settlements.locations[settlementIndex]))
	{
		ui16 regionIndex, tileIndex;
		world_get_tile(terrain, command.origin, regionIndex, tileIndex);
		bool originUsable = terrain.regions[regionIndex].status[tileIndex].is_road && terrain.regions[regionIndex].owners[tileIndex] == settlementIndex;
		if (!originUsable && command_add_path_error(feedback, COMMAND_INVALID_PATH_ORIGIN_INVALID)) return false;
	}

	const world_entity_workers& workers = world.entities->workers;
	if (workers.active_count >= workers.max_count && command_add_error(feedback, COMMAND_ERROR_ENTITY_LIMIT_REACHED)) return false;

	return command_is_valid(feedback);
}

// Spawns a road worker tied to the settlement. The road itself is built over the following ticks (see the construction system).
void command_build_road_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	// Checked again: earlier commands in the same tick may have changed the settlement or filled the last worker slot.
	if (!command_build_road_validity_check(world, player, command_header, nullptr)) return;

	const command_data_build_road& command = command_header.get_command_data<command_data_build_road>();

    // Initialize worker entity to build a road from origin to destination.
	world_entity_workers::single worker = {};
	worker.origin_entity = command.settlement;
	worker.work_type = WORK_TYPE::BUILD_ROAD;
	worker.daily_cost = GAME_PARAMETERS.road_crew_cost_per_day;
    
    // Init payload. @TODO: Could probably just init head on tick 0 of the build logic...
	*(build_road_data*)worker.action_data.bytes = {
        .start = command.origin,
        .head = command.origin,
        .destination = command.destination 
    };

	world_spawn_worker(world, worker);
}

// SET SETTLEMENT TROOP CEILING

constexpr ui8 POOLABLE_TROOP_TYPE_COUNT = 3; // men at arms, horsemen, knights.

static ui16* settlement_troops_field(army_troop_count& troops, ui8 troopType)
{
	switch (troopType)
	{
	case 0: return &troops.men_at_arms;
	case 1: return &troops.horsemen;
	default: return &troops.knights;
	}
}

bool command_set_settlement_troop_ceiling_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_set_settlement_troop_ceiling& command = command_header.get_command_data<command_data_set_settlement_troop_ceiling>();

	if (!world_entity_is_valid(world, command.settlement) || command.settlement.get_type() != ENTITY_TYPE::SETTLEMENT)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	if (world.entities->settlements.owners[command.settlement.get_index()] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	// No dedicated error for this: reported as unspecified.
	if (command.troop_type >= POOLABLE_TROOP_TYPE_COUNT && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	return command_is_valid(feedback);
}

void command_set_settlement_troop_ceiling_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_set_settlement_troop_ceiling_validity_check(world, player, command_header, nullptr)) return;

	const command_data_set_settlement_troop_ceiling& command = command_header.get_command_data<command_data_set_settlement_troop_ceiling>();
	world_entity_settlements& settlements = world.entities->settlements;
	ui16 index = command.settlement.get_index();

	// Raw value stored as is, never clamped.
	*settlement_troops_field(settlements.troop_ceilings[index], command.troop_type) = command.ceiling;

	// The ceiling applies to garrison + raised: lowering it below that total releases only HOME troops back to peasants (raised ones are never touched).
	ui16 garrisonTotal = *settlement_troops_field(settlements.garrison[index], command.troop_type);
	ui16 raised = *settlement_troops_field(settlements.raised[index], command.troop_type);
	ui16 garrisonAllowed = command.ceiling > raised ? command.ceiling - raised : 0;
	if (garrisonTotal > garrisonAllowed) settlement_discharge_soldiers(world, index, command.troop_type + TROOP_TYPE_MEN_AT_ARMS, garrisonTotal - garrisonAllowed);
}

// SET SETTLEMENT TAX RATE

bool command_set_settlement_tax_rate_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_set_settlement_tax_rate& command = command_header.get_command_data<command_data_set_settlement_tax_rate>();

	if (!world_entity_is_valid(world, command.settlement) || command.settlement.get_type() != ENTITY_TYPE::SETTLEMENT)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	if (world.entities->settlements.owners[command.settlement.get_index()] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	if ((command.tax_rate < GAME_PARAMETERS.tax_rate_min || command.tax_rate > GAME_PARAMETERS.tax_rate_max)
		&& command_add_error(feedback, COMMAND_ERROR_VALUE_OUT_OF_RANGE)) return false;

	return command_is_valid(feedback);
}

void command_set_settlement_tax_rate_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_set_settlement_tax_rate_validity_check(world, player, command_header, nullptr)) return;

	const command_data_set_settlement_tax_rate& command = command_header.get_command_data<command_data_set_settlement_tax_rate>();
	world.entities->settlements.tax_rates[command.settlement.get_index()] = command.tax_rate;
}

// BUILD SETTLEMENT BUILDING

bool command_build_settlement_building_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_build_settlement_building& command = command_header.get_command_data<command_data_build_settlement_building>();

	if (!world_entity_is_valid(world, command.settlement) || command.settlement.get_type() != ENTITY_TYPE::SETTLEMENT)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	if (world.entities->settlements.owners[command.settlement.get_index()] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	if (command.building_type >= SETTLEMENT_BUILDING_TYPE_COUNT && command_add_error(feedback, COMMAND_ERROR_VALUE_OUT_OF_RANGE)) return false;

	// Coastal-only rows (Trade Port). No dedicated error flag: a plain rejection.
	world_location dock;
	if (command.building_type < SETTLEMENT_BUILDING_TYPE_COUNT && SETTLEMENT_BUILDING_TABLE[command.building_type].requires_coast
		&& !settlement_dock_tile(world, command.settlement.get_index(), dock) && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	// No limit on parallel projects, only on worker slots.
	const world_entity_workers& workers = world.entities->workers;
	if (workers.active_count >= workers.max_count && command_add_error(feedback, COMMAND_ERROR_ENTITY_LIMIT_REACHED)) return false;

	return command_is_valid(feedback);
}

// Spawns a construction worker tied to the settlement. The project is financed daily by the worker system.
void command_build_settlement_building_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_build_settlement_building_validity_check(world, player, command_header, nullptr)) return;

	const command_data_build_settlement_building& command = command_header.get_command_data<command_data_build_settlement_building>();

	world_entity_workers::single worker = {};
	worker.origin_entity = command.settlement;
	worker.work_type = WORK_TYPE::BUILD_SETTLEMENT_BUILDING;
	worker.daily_cost = SETTLEMENT_BUILDING_TABLE[command.building_type].cost_per_day; // Type checked by the validity check.
	*(build_settlement_building_data*)worker.action_data.bytes = { .building_type = command.building_type };

	world_spawn_worker(world, worker);
}

// SET ARMY RAISE TARGETS

bool command_set_army_raise_targets_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_set_army_raise_targets& command = command_header.get_command_data<command_data_set_army_raise_targets>();

	if (!world_entity_is_valid(world, command.army) || command.army.get_type() != ENTITY_TYPE::ARMY)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	const world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = command.army.get_index();

	if (armies.owners[armyIndex] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	// No dedicated error for an army that is not raising: reported as unspecified.
	if (!armies.raising[armyIndex] && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	if (command.target_levies == 0 && command.target_men_at_arms == 0 && command.target_horsemen == 0 && command.target_knights == 0
		&& command_add_error(feedback, COMMAND_ERROR_NO_RAISE_TARGET)) return false;

	// The home settlement must still exist.
	entity_guid home = armies.home_settlements[armyIndex];
	if (!world_entity_is_valid(world, home) || home.get_type() != ENTITY_TYPE::SETTLEMENT)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	return command_is_valid(feedback);
}

void command_set_army_raise_targets_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_set_army_raise_targets_validity_check(world, player, command_header, nullptr)) return;

	const command_data_set_army_raise_targets& command = command_header.get_command_data<command_data_set_army_raise_targets>();

	// Targets below the current counts are allowed: nothing is returned, the army just completes.
	world.entities->armies.raised_troops[command.army.get_index()] = { command.target_levies, command.target_men_at_arms, command.target_horsemen, command.target_knights };
}

// STOP RAISING ARMY

// Deliberately independent of the army's position and battle state: an army stuck unable to reach its raise location must still be stoppable.
bool command_stop_raising_army_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_stop_raising_army& command = command_header.get_command_data<command_data_stop_raising_army>();

	if (!world_entity_is_valid(world, command.army) || command.army.get_type() != ENTITY_TYPE::ARMY)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	const world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = command.army.get_index();

	if (armies.owners[armyIndex] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	// No dedicated error for an army that is not raising: reported as unspecified.
	if (!armies.raising[armyIndex] && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	return command_is_valid(feedback);
}

void command_stop_raising_army_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_stop_raising_army_validity_check(world, player, command_header, nullptr)) return;

	const command_data_stop_raising_army& command = command_header.get_command_data<command_data_stop_raising_army>();
	world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = command.army.get_index();

	// Raise targets are left untouched: the army remembers what it was raised to.
	armies.raising[armyIndex] = 0;

	// Stopped before any man joined: there is no army.
	if (army_total_manpower(armies.compositions[armyIndex]) == 0) world_destroy_army(world, command.army);
}

// DISBAND ARMY

bool command_disband_army_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_disband_army& command = command_header.get_command_data<command_data_disband_army>();

	if (!world_entity_is_valid(world, command.army) || command.army.get_type() != ENTITY_TYPE::ARMY)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	const world_entity_armies& armies = world.entities->armies;
	ui16 armyIndex = command.army.get_index();

	if (armies.owners[armyIndex] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	// An army in a battle is counted in it: no dedicated error, reported as unspecified.
	if (armies.battles[armyIndex].is_valid() && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	// The men go back to the home settlement, which must still exist.
	entity_guid home = armies.home_settlements[armyIndex];
	if (!home.is_valid() || !world_entity_is_valid(world, home) || home.get_type() != ENTITY_TYPE::SETTLEMENT)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	// The army must stand on a tile of its home settlement's territory.
	ui16 regionIndex, tileIndex;
	world_get_tile(*world.terrain, armies.locations[armyIndex], regionIndex, tileIndex);
	if (world.terrain->regions[regionIndex].owners[tileIndex] != home.get_index() && command_add_error(feedback, COMMAND_ERROR_NOT_IN_HOME_TERRITORY)) return false;

	return command_is_valid(feedback);
}

void command_disband_army_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_disband_army_validity_check(world, player, command_header, nullptr)) return;

	const command_data_disband_army& command = command_header.get_command_data<command_data_disband_army>();
	world_disband_army(world, command.army, world.entities->armies.home_settlements[command.army.get_index()]);
}

// SET EXPORT RATIO

bool command_set_export_ratio_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_set_export_ratio& command = command_header.get_command_data<command_data_set_export_ratio>();

	if (!world_entity_is_valid(world, command.settlement) || command.settlement.get_type() != ENTITY_TYPE::SETTLEMENT)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	if (world.entities->settlements.owners[command.settlement.get_index()] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	// ui8: never below 0. Above 100 is refused whatever the parameter says.
	if (command.export_ratio > ia_min(GAME_PARAMETERS.trade.export_ratio_max_percent, (ui32)100) && command_add_error(feedback, COMMAND_ERROR_VALUE_OUT_OF_RANGE)) return false;

	return command_is_valid(feedback);
}

void command_set_export_ratio_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_set_export_ratio_validity_check(world, player, command_header, nullptr)) return;

	const command_data_set_export_ratio& command = command_header.get_command_data<command_data_set_export_ratio>();
	world.entities->settlements.trade[command.settlement.get_index()].export_ratio = command.export_ratio;
}

// SET ALLOW FOREIGN EXPORT

bool command_set_allow_foreign_export_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_set_allow_foreign_export& command = command_header.get_command_data<command_data_set_allow_foreign_export>();

	if (!world_entity_is_valid(world, command.settlement) || command.settlement.get_type() != ENTITY_TYPE::SETTLEMENT)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	if (world.entities->settlements.owners[command.settlement.get_index()] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	return command_is_valid(feedback);
}

void command_set_allow_foreign_export_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_set_allow_foreign_export_validity_check(world, player, command_header, nullptr)) return;

	const command_data_set_allow_foreign_export& command = command_header.get_command_data<command_data_set_allow_foreign_export>();
	world.entities->settlements.trade[command.settlement.get_index()].allow_foreign_export = command.allow_foreign_export != 0 ? 1 : 0;
}

// BEGIN ASSAULT

bool command_begin_assault_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_begin_assault& command = command_header.get_command_data<command_data_begin_assault>();

	if (!world_entity_is_valid(world, command.engagement) || command.engagement.get_type() != ENTITY_TYPE::BATTLE
		|| world.entities->battles.states[command.engagement.get_index()].kind != world_entity_battles::KIND_SIEGE)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	const world_entity_battles::state& siege = world.entities->battles.states[command.engagement.get_index()];

	if (siege.attacker.commander != player && command_add_error(feedback, COMMAND_ERROR_NOT_COMMANDER)) return false;
	if (siege.phase != world_entity_battles::PHASE_BESIEGING && command_add_error(feedback, COMMAND_ERROR_WRONG_PHASE)) return false;

	return command_is_valid(feedback);
}

void command_begin_assault_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_begin_assault_validity_check(world, player, command_header, nullptr)) return;

	const command_data_begin_assault& command = command_header.get_command_data<command_data_begin_assault>();
	world_entity_battles::state& siege = world.entities->battles.states[command.engagement.get_index()];
	siege.phase = world_entity_battles::PHASE_ASSAULT;
	siege.phase_tick = world.age;
}

// END COMMAND FUNCTION IMPLEMENTATIONS

// BEGIN CORE COMMAND HANDLING FUNCTIONS

// Checks that an input command would be valid to apply to the match over its next tick.
bool match_command_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command,
	match_command_validity_feedback* feedback)
{
	if (feedback != nullptr) *feedback = {};

	// Callers must have range-checked the type of untrusted commands already.
	const match_command_def* def = match_command_get_def(command.type);
	if (def == nullptr)
	{
		if (feedback != nullptr) feedback->error_flags = COMMAND_ERROR_UNSPECIFIED;
		return false;
	}

	bool isValid = def->validity_check(world, player, command, feedback);

	// Checks not collecting details (or not interested) still have to report that the command is invalid.
	if (!isValid && feedback != nullptr && feedback->error_flags == 0) feedback->error_flags = COMMAND_ERROR_UNSPECIFIED;
	return isValid;
}

inline void match_command_apply(match_world_state& world, match_player_id player, const match_command_header& command)
{
	// Route to type-specific logic.
	const match_command_def* def = match_command_get_def(command.type);
	if (def == nullptr) return;

	def->apply(world, player, command);
}

void match_command_apply_all(match_world_state& world, const match_tick_commands& tick_commands)
{
	ui32 commandBufferPos = 0;

	for (ui16 sequenceIndex = 0; sequenceIndex < tick_commands.sequences_count; sequenceIndex++)
	{
		match_player_id sequencePlayerId = tick_commands.get_sequence_player_at(commandBufferPos);
		commandBufferPos += sizeof(match_player_id);

		match_command_sequence& sequence = tick_commands.get_sequence_at(commandBufferPos);
		commandBufferPos += sizeof(match_command_sequence);

		ui32 sequenceBufferPos = 0;
		for (ui16 commandIndex = 0; commandIndex < sequence.command_count; commandIndex++)
		{
			const match_command_header& commandHeader = sequence.get_sequence_at(sequenceBufferPos);
			sequenceBufferPos += sizeof(match_command_header);

			match_command_apply(world, sequencePlayerId, commandHeader);

			sequenceBufferPos += get_command_data_size(commandHeader.type);
		}
		commandBufferPos += sequenceBufferPos;
	}
}

bool match_command_sequence_output_validated(const game_match& target_match, const match_command_sequence& unvalidated,
	match_player_id player_id, command_sequence_builder& output_builder)
{
    ASSERT(target_match.world != nullptr);
    match_world_state& world = *target_match.world;

	ui32 sequenceBufferPos = 0;
	for (ui16 commandIndex = 0; commandIndex < unvalidated.command_count; commandIndex++)
	{
		const match_command_header& commandHeader = unvalidated.get_sequence_at(sequenceBufferPos);
		sequenceBufferPos += sizeof(match_command_header);

		// An unknown type has no known data size: the whole unvalidated sequence becomes impossible to keep reading safely.
		// Stop process now and signal caller that the sequence couldn't be fully validated.
		if ((ui8)commandHeader.type >= (ui8)MATCH_COMMAND_TYPE::TYPE_COUNT) return false;

		bool isValid = match_command_validity_check(world, player_id, commandHeader);
		sequenceBufferPos += get_command_data_size(commandHeader.type);

		// If command is valid, add it to the output sequence. Otherwise discard it.
		if (isValid)
		{
			void* validatedPayload = output_builder.push_command(commandHeader.type);
			ia_memcpy(validatedPayload, commandHeader.command_data, get_command_data_size(commandHeader.type));
		}
	}

	return true;
}

