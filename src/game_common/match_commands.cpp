// Main implementation file for match input command functions.

#include "game_common/match/commands.h"
#include "game_common/match/world.h"
#include "world_sim/world_sim.h"
#include "world_sim/world_sim_army.h"
#include "world_sim/world_sim_population.h"

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
        .population = 500,
        .tier = 1,
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
	if (command.move_target.x >= world.terrain.size_tiles.x || command.move_target.y >= world.terrain.size_tiles.y) return false;

	ENTITY_TYPE entityType = command.entity.get_type();
	switch (entityType)
	{
		// With valid types, just need to check that player can control them.
	case ENTITY_TYPE::ARMY:
		// A raising army takes the order but only starts moving once raised.
		return world.entities.armies.owners[command.entity.get_index()] == player;
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
		world.entities.armies.movements[command.entity.get_index()].destination = command.move_target;
		// A new move order overrides any attack (or follow / merge) the army was committed to.
		world.entities.armies.action_targets[command.entity.get_index()] = INVALID_ENTITY_GUID;
		world.entities.armies.merging[command.entity.get_index()] = 0;
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

	if (!world_entity_is_valid(world, command.army)) return false;
	if (command.army.get_type() != ENTITY_TYPE::ARMY) return false;
	if (world.entities.armies.owners[command.army.get_index()] != player) return false;
	// Not ready yet: an empty warband would found a free settlement.
	if (army_is_raising(world, command.army.get_index())) { command_add_error(feedback, COMMAND_ERROR_ARMY_RAISING); return false; }

	// The army is kept if the location is invalid or there is no room for the settlement.
	if (!world_is_valid_settlement_location(world, world.entities.armies.locations[command.army.get_index()], player)) return false;

	const world_entity_settlements& settlements = world.entities.settlements;
	return settlements.active_count < settlements.max_count;
}

// Spawns a settlement at the army's location with its owner and the total manpower of its warbands as population, then destroys the army.
// Settling is the one case where the warbands' men actually leave their home settlements.
void command_found_settlement_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	const command_data_found_settlement& command = command_header.get_command_data<command_data_found_settlement>();

	// Checked again: earlier commands in the same tick may have destroyed this army, filled the last settlement slot or made the location invalid.
	if (!world_entity_is_valid(world, command.army)) return;
	if (command.army.get_type() != ENTITY_TYPE::ARMY) return;
	if (army_is_raising(world, command.army.get_index())) return;
	if (!world_is_valid_settlement_location(world, world.entities.armies.locations[command.army.get_index()], player)) return;

	const world_entity_settlements& settlements = world.entities.settlements;
	if (settlements.active_count >= settlements.max_count) return;

	ui16 armyIndex = command.army.get_index();
	const world_entity_armies& armies = world.entities.armies;
	ui32 men[world_entity_battles::UNIT_TYPES];
	army_total_unit_counts(world, armyIndex, men);

	world_entity_settlements::single newSettlement = {};
	newSettlement.owner = armies.owners[armyIndex];
	newSettlement.location = armies.locations[armyIndex];
	newSettlement.population = men[0] + men[1] + men[2] + men[3];
    newSettlement.tier = 1;

	const world_entity_armies::warband_list& list = armies.warbands[armyIndex];
	for (ui8 slot = 0; slot < list.count; slot++)
	{
		ui16 warbandIndex = list.indices[slot];
		world_warband_remove_men_from_home(world, warbandIndex, world.entities.warbands.compositions[warbandIndex]);
	}
	world_spawn_settlement(world, newSettlement);
	world_destroy_army(world, command.army);
}

// RAISE ARMY

// Cost / upkeep hooks: raising is free for now.
static bool raise_army_can_afford(const match_world_state& world, const command_data_raise_army& command, match_command_validity_feedback* feedback)
{
	return true;
}

static void raise_army_charge(match_world_state& world, const command_data_raise_army& command)
{
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
	const world_entity_settlements& settlements = world.entities.settlements;

	bool locationInWorld = command.raise_location.x < world.terrain.size_tiles.x && command.raise_location.y < world.terrain.size_tiles.y;
	if (!locationInWorld && command_add_error(feedback, COMMAND_ERROR_TARGET_OUT_OF_BOUNDS)) return false;

	if (settlements.owners[settlementIndex] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	if (command.target_levies == 0 && command.target_men_at_arms == 0 && command.target_horsemen == 0 && command.target_knights == 0
		&& command_add_error(feedback, COMMAND_ERROR_NO_RAISE_TARGET)) return false;

	// A settlement has a limited number of raised armies at once. Reported as the entity limit.
	ui32 raisedArmies, raisedMen;
	world_settlement_raised(world, command.settlement, raisedArmies, raisedMen);
	if (raisedArmies >= SETTLEMENT_MAX_RAISED_ARMIES && command_add_error(feedback, COMMAND_ERROR_ENTITY_LIMIT_REACHED)) return false;

	// Raising levies must leave the settlement its minimum population. Pooled troops are only limited when drawn, so their targets can exceed the pool.
	ui32 populationNeeded = (ui32)command.target_levies + RAISE_MIN_POPULATION_LEFT;
	if (feedback != nullptr)
	{
		feedback->population_needed = populationNeeded;
		feedback->population_current = settlements.populations[settlementIndex];
	}
	if (settlements.populations[settlementIndex] < populationNeeded && command_add_error(feedback, COMMAND_ERROR_NOT_ENOUGH_POPULATION)) return false;

	if (!raise_army_can_afford(world, command, feedback) && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	// The raise location must be in the settlement's territory.
	if (locationInWorld)
	{
		ui16 regionIndex, tileIndex;
		world_get_tile(world.terrain, command.raise_location, regionIndex, tileIndex);
		if (world.terrain.regions[regionIndex].owners[tileIndex] != settlementIndex && command_add_error(feedback, COMMAND_ERROR_TARGET_OUT_OF_TERRITORY)) return false;
	}

	const world_entity_armies& armies = world.entities.armies;
	const world_entity_warbands& warbands = world.entities.warbands;
	if ((armies.active_count >= armies.max_count || warbands.active_count >= warbands.max_count)
		&& command_add_error(feedback, COMMAND_ERROR_ENTITY_LIMIT_REACHED)) return false;

	return command_is_valid(feedback);
}

// Spawns an empty warband, in a new army of its own, inside the settlement, heading for the raise location. It draws its men from the settlement right away
// and its army starts moving once it is raised (see world_system_armies_raising).
void command_raise_army_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	// Checked again: earlier commands in the same tick may have changed the settlement or filled the last army / warband slot.
	if (!command_raise_army_validity_check(world, player, command_header, nullptr)) return;

	const command_data_raise_army& command = command_header.get_command_data<command_data_raise_army>();

	ui16 settlementIndex = command.settlement.get_index();
	const world_entity_settlements& settlements = world.entities.settlements;

	world_entity_armies::single newArmy = {};
	newArmy.owner = settlements.owners[settlementIndex];
	newArmy.location = settlements.locations[settlementIndex];
	newArmy.movement.destination = command.raise_location;
	entity_guid army = world_spawn_army(world, newArmy);

	world_entity_warbands::single newWarband = {};
	newWarband.raise_target = { command.target_levies, command.target_men_at_arms, command.target_horsemen, command.target_knights };
	newWarband.home_settlement = command.settlement;
	newWarband.raising = 1;
	world_spawn_warband(world, newWarband, army);

	raise_army_charge(world, command);
}

// ENTITY SET ATTACK TARGET

bool command_set_entity_attack_target_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_set_entity_attack_target& command = command_header.get_command_data<command_data_set_entity_attack_target>();

	if (!world_entity_is_valid(world, command.attacker_entity)) return false;
	if (command.attacker_entity.get_type() != ENTITY_TYPE::ARMY) return false;
	if (world.entities.armies.owners[command.attacker_entity.get_index()] != player) return false;
	// A raising army takes the order but only starts its chase once raised.

	// Targets owned by the sender can't be attacked (see match_caravan_is_owned_by for caravans).
	ui16 targetIndex = command.target_entity.get_index();
	const match_world_state::entities_store& entities = world.entities;
	switch (command.target_entity.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
		return entities.settlements.owners[targetIndex] != player;
	case ENTITY_TYPE::ARMY:
		// Another own army is accompanied instead (see world_system_armies_target_reached).
		if (entities.armies.owners[targetIndex] != player) return true;
		return world_entity_is_valid(world, command.target_entity) && !(command.target_entity == command.attacker_entity);
	case ENTITY_TYPE::CARAVAN:
		return !world_caravan_is_owned_by(world, command.target_entity, player);
	default:
		return false;
	}
}

// Commits the army to chasing the target (or accompanying it, for an own army). The chase and its resolution are handled by the match tick.
void command_set_entity_attack_target_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	// Checked again: earlier commands in the same tick may have destroyed the attacker or the target.
	if (!command_set_entity_attack_target_validity_check(world, player, command_header, nullptr)) return;

	const command_data_set_entity_attack_target& command = command_header.get_command_data<command_data_set_entity_attack_target>();

	world.entities.armies.action_targets[command.attacker_entity.get_index()] = command.target_entity;
	world.entities.armies.merging[command.attacker_entity.get_index()] = 0;
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
	const world_entity_settlements& settlements = world.entities.settlements;
	if (settlements.owners[settlementIndex] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

    // The origin and destination must be in the world, and not be equal to eachother.
    if (command.origin == command.destination && command_add_path_error(feedback, COMMAND_INVALID_PATH_ORIGIN_IS_DESTINATION)) return false;

	const world_terrain& terrain = world.terrain;
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

	const world_entity_builders& builders = world.entities.builders;
	if (builders.active_count >= builders.max_count && command_add_error(feedback, COMMAND_ERROR_ENTITY_LIMIT_REACHED)) return false;

	return command_is_valid(feedback);
}

// Spawns a road builder tied to the settlement. The road itself is built over the following ticks (see the construction system).
void command_build_road_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	// Checked again: earlier commands in the same tick may have changed the settlement or filled the last builder slot.
	if (!command_build_road_validity_check(world, player, command_header, nullptr)) return;

	const command_data_build_road& command = command_header.get_command_data<command_data_build_road>();

    // Initialize builder entity to build a road from origin to destination.
	world_entity_builders::single builder = {};
	builder.origin_entity = command.settlement;
	builder.build_type = BUILD_TYPE_ROAD;
    
    // Init payload. @TODO: Could probably just init head on tick 0 of the build logic...
	*(build_road_data*)builder.action_data.bytes = {
        .start = command.origin,
        .head = command.origin,
        .destination = command.destination 
    };

	world_spawn_builder(world, builder);
}

// SET SETTLEMENT TROOP CEILING

constexpr ui8 POOLABLE_TROOP_TYPE_COUNT = 3; // men at arms, horsemen, knights.

static ui16* settlement_troops_field(settlement_troops& troops, ui8 troopType)
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

	if (world.entities.settlements.owners[command.settlement.get_index()] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	// No dedicated error for this: reported as unspecified.
	if (command.troop_type >= POOLABLE_TROOP_TYPE_COUNT && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	return command_is_valid(feedback);
}

void command_set_settlement_troop_ceiling_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_set_settlement_troop_ceiling_validity_check(world, player, command_header, nullptr)) return;

	const command_data_set_settlement_troop_ceiling& command = command_header.get_command_data<command_data_set_settlement_troop_ceiling>();
	world_entity_settlements& settlements = world.entities.settlements;
	ui16 index = command.settlement.get_index();

	// Raw value stored as is, never clamped.
	*settlement_troops_field(settlements.troop_ceilings[index], command.troop_type) = command.ceiling;

	// Only lowering the ceiling itself below the current count loses the excess troops (a pool above max is kept).
	ui16* count = settlement_troops_field(settlements.local_troops[index], command.troop_type);
	if (*count > command.ceiling) *count = command.ceiling;
}

// SET ARMY RAISE TARGETS

bool command_set_army_raise_targets_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_set_army_raise_targets& command = command_header.get_command_data<command_data_set_army_raise_targets>();

	if (!world_entity_is_valid(world, command.warband) || command.warband.get_type() != ENTITY_TYPE::WARBAND)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	const world_entity_warbands& warbands = world.entities.warbands;
	ui16 warbandIndex = command.warband.get_index();

	if (warbands.owners[warbandIndex] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	// No dedicated error for a warband that is not raising: reported as unspecified.
	if (!warbands.raising[warbandIndex] && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	if (command.target_levies == 0 && command.target_men_at_arms == 0 && command.target_horsemen == 0 && command.target_knights == 0
		&& command_add_error(feedback, COMMAND_ERROR_NO_RAISE_TARGET)) return false;

	// Same levy rule as RAISE_ARMY, against the home settlement's current population.
	entity_guid home = warbands.home_settlements[warbandIndex];
	if (!world_entity_is_valid(world, home) || home.get_type() != ENTITY_TYPE::SETTLEMENT)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	ui32 populationNeeded = (ui32)command.target_levies + RAISE_MIN_POPULATION_LEFT;
	ui32 populationCurrent = world.entities.settlements.populations[home.get_index()];
	if (feedback != nullptr)
	{
		feedback->population_needed = populationNeeded;
		feedback->population_current = populationCurrent;
	}
	if (populationCurrent < populationNeeded && command_add_error(feedback, COMMAND_ERROR_NOT_ENOUGH_POPULATION)) return false;

	return command_is_valid(feedback);
}

void command_set_army_raise_targets_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_set_army_raise_targets_validity_check(world, player, command_header, nullptr)) return;

	const command_data_set_army_raise_targets& command = command_header.get_command_data<command_data_set_army_raise_targets>();

	// Targets below the current counts are allowed: nothing is returned, the warband just completes.
	world.entities.warbands.raise_targets[command.warband.get_index()] = { command.target_levies, command.target_men_at_arms, command.target_horsemen, command.target_knights };
}

// STOP RAISING ARMY

// Deliberately independent of the army's position and battle state: an army stuck unable to reach its raise location must still be stoppable.
bool command_stop_raising_army_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_stop_raising_army& command = command_header.get_command_data<command_data_stop_raising_army>();

	if (!world_entity_is_valid(world, command.warband) || command.warband.get_type() != ENTITY_TYPE::WARBAND)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	const world_entity_warbands& warbands = world.entities.warbands;
	ui16 warbandIndex = command.warband.get_index();

	if (warbands.owners[warbandIndex] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	// No dedicated error for a warband that is not raising: reported as unspecified.
	if (!warbands.raising[warbandIndex] && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	return command_is_valid(feedback);
}

void command_stop_raising_army_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_stop_raising_army_validity_check(world, player, command_header, nullptr)) return;

	const command_data_stop_raising_army& command = command_header.get_command_data<command_data_stop_raising_army>();

	// Raise targets are left untouched: the warband remembers what it was raised to.
	world.entities.warbands.raising[command.warband.get_index()] = 0;
}

// MERGE ARMY

// Two own armies, different, whose warbands fit in one. Not checked: battles (an army in a battle acts once released, like attack orders).
bool command_merge_army_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_merge_army& command = command_header.get_command_data<command_data_merge_army>();

	if (!world_entity_is_valid(world, command.army) || command.army.get_type() != ENTITY_TYPE::ARMY
		|| !world_entity_is_valid(world, command.into_army) || command.into_army.get_type() != ENTITY_TYPE::ARMY
		|| command.army == command.into_army)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	const world_entity_armies& armies = world.entities.armies;
	ui16 armyIndex = command.army.get_index();
	ui16 intoIndex = command.into_army.get_index();

	if ((armies.owners[armyIndex] != player || armies.owners[intoIndex] != player) && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	if ((ui32)armies.warbands[armyIndex].count + armies.warbands[intoIndex].count > ARMY_MAX_WARBANDS
		&& command_add_error(feedback, COMMAND_ERROR_ENTITY_LIMIT_REACHED)) return false;

	return command_is_valid(feedback);
}

// Commits the army to following the other one: the merge happens in the match tick once within reach (see world_system_armies_target_reached).
void command_merge_army_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_merge_army_validity_check(world, player, command_header, nullptr)) return;

	const command_data_merge_army& command = command_header.get_command_data<command_data_merge_army>();
	ui16 armyIndex = command.army.get_index();

	world.entities.armies.action_targets[armyIndex] = command.into_army;
	world.entities.armies.merging[armyIndex] = 1;
}

// DETACH WARBANDS

bool command_detach_warbands_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_detach_warbands& command = command_header.get_command_data<command_data_detach_warbands>();

	if (!world_entity_is_valid(world, command.army) || command.army.get_type() != ENTITY_TYPE::ARMY)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	const world_entity_armies& armies = world.entities.armies;
	ui16 armyIndex = command.army.get_index();

	if (armies.owners[armyIndex] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	// At least one slot, only slots of existing warbands, and not all of them (a single-warband army can't detach anything).
	ui64 slots = ((ui64)command.slots_high << 32) | command.slots_low;
	ui8 count = armies.warbands[armyIndex].count;
	ui64 allSlots = count >= 64 ? ~(ui64)0 : (((ui64)1 << count) - 1);
	if ((slots == 0 || (slots & ~allSlots) != 0 || slots == allSlots) && command_add_error(feedback, COMMAND_ERROR_INVALID_SELECTION)) return false;

	// Pinned: no dedicated error for an army in a battle, reported as unspecified.
	if (armies.battles[armyIndex].is_valid() && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	if (armies.active_count >= armies.max_count && command_add_error(feedback, COMMAND_ERROR_ENTITY_LIMIT_REACHED)) return false;

	return command_is_valid(feedback);
}

// The selected warbands form a new army next to the old one, with no orders.
void command_detach_warbands_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_detach_warbands_validity_check(world, player, command_header, nullptr)) return;

	const command_data_detach_warbands& command = command_header.get_command_data<command_data_detach_warbands>();
	world_army_detach(world, command.army, ((ui64)command.slots_high << 32) | command.slots_low);
}

// SIEGE COMMANDS (assault, lift, detach from, sally out)

// Records an invalid entity error unless the guid is a valid siege. Returns whether it is one (nothing else can be checked otherwise).
static bool siege_check_entity(const match_world_state& world, entity_guid siege, match_command_validity_feedback* feedback)
{
	if (world_entity_is_valid(world, siege) && siege.get_type() == ENTITY_TYPE::SIEGE) return true;

	command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
	return false;
}

// Whether a siege can start its battle (assault or sally out) now: none in progress, at least one besieger not fighting elsewhere,
// and room for the defenders' army and warband and for the battle. No dedicated error for the first two: reported as unspecified.
// Ends the caller's check: returns its result (with feedback, valid unless any error was recorded, before or here).
static bool siege_can_start_battle(const match_world_state& world, entity_guid siege, match_command_validity_feedback* feedback)
{
	const world_entity_sieges::state& state = world.entities.sieges.states[siege.get_index()];
	const world_entity_armies& armies = world.entities.armies;

	bool freeBesieger = false;
	for (ui8 slot = 0; slot < state.army_count; slot++)
	{
		entity_guid army = state.armies[slot];
		if (world_entity_is_valid(world, army) && armies.sieges[army.get_index()] == siege && !armies.battles[army.get_index()].is_valid()) freeBesieger = true;
	}
	if ((world_entity_is_valid(world, state.battle) || !freeBesieger) && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	const world_entity_warbands& warbands = world.entities.warbands;
	const world_entity_battles& battles = world.entities.battles;
	if ((armies.active_count >= armies.max_count || warbands.active_count >= warbands.max_count || battles.active_count >= battles.max_count)
		&& command_add_error(feedback, COMMAND_ERROR_ENTITY_LIMIT_REACHED)) return false;

	return command_is_valid(feedback);
}

bool command_assault_siege_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_assault_siege& command = command_header.get_command_data<command_data_assault_siege>();

	if (!siege_check_entity(world, command.siege, feedback)) return false;

	if (world.entities.sieges.states[command.siege.get_index()].commander != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	return siege_can_start_battle(world, command.siege, feedback);
}

void command_assault_siege_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_assault_siege_validity_check(world, player, command_header, nullptr)) return;

	const command_data_assault_siege& command = command_header.get_command_data<command_data_assault_siege>();
	world_siege_start_battle(world, command.siege, true);
}

bool command_lift_siege_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_lift_siege& command = command_header.get_command_data<command_data_lift_siege>();

	if (!siege_check_entity(world, command.siege, feedback)) return false;

	const world_entity_sieges::state& state = world.entities.sieges.states[command.siege.get_index()];
	if (state.commander != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	// Not while its battle is being fought: no dedicated error, reported as unspecified.
	if (world_entity_is_valid(world, state.battle) && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	return command_is_valid(feedback);
}

void command_lift_siege_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_lift_siege_validity_check(world, player, command_header, nullptr)) return;

	const command_data_lift_siege& command = command_header.get_command_data<command_data_lift_siege>();
	world_destroy_siege(world, command.siege);
}

// Accepted from the siege's commander or the army's owner (an army of another player may have joined the siege).
bool command_detach_from_siege_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_detach_from_siege& command = command_header.get_command_data<command_data_detach_from_siege>();

	if (!world_entity_is_valid(world, command.army) || command.army.get_type() != ENTITY_TYPE::ARMY)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	ui16 armyIndex = command.army.get_index();
	entity_guid siege = world.entities.armies.sieges[armyIndex];
	if (!world_entity_is_valid(world, siege))
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY); // Not in a siege.
		return false;
	}

	if (world.entities.armies.owners[armyIndex] != player && world.entities.sieges.states[siege.get_index()].commander != player
		&& command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	return command_is_valid(feedback);
}

void command_detach_from_siege_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_detach_from_siege_validity_check(world, player, command_header, nullptr)) return;

	const command_data_detach_from_siege& command = command_header.get_command_data<command_data_detach_from_siege>();
	world_siege_remove_army(world, command.army);
}

bool command_sally_out_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_sally_out& command = command_header.get_command_data<command_data_sally_out>();

	if (!siege_check_entity(world, command.siege, feedback)) return false;

	// The defender is the settlement's owner for as long as the siege lasts.
	if (world.entities.sieges.states[command.siege.get_index()].defender != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	return siege_can_start_battle(world, command.siege, feedback);
}

void command_sally_out_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_sally_out_validity_check(world, player, command_header, nullptr)) return;

	const command_data_sally_out& command = command_header.get_command_data<command_data_sally_out>();
	world_siege_start_battle(world, command.siege, false);
}

// DISBAND ARMY

// The army must stand in the territory of the home settlement of each of its warbands, still held by the warband's owner.
bool command_disband_army_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command_header,
	match_command_validity_feedback* feedback)
{
	const command_data_disband_army& command = command_header.get_command_data<command_data_disband_army>();

	if (!world_entity_is_valid(world, command.army) || command.army.get_type() != ENTITY_TYPE::ARMY)
	{
		command_add_error(feedback, COMMAND_ERROR_INVALID_ENTITY);
		return false;
	}

	const world_entity_armies& armies = world.entities.armies;
	const world_entity_warbands& warbands = world.entities.warbands;
	ui16 armyIndex = command.army.get_index();

	if (armies.owners[armyIndex] != player && command_add_error(feedback, COMMAND_ERROR_NOT_OWNER)) return false;

	// Pinned: no dedicated error for an army in a battle, reported as unspecified.
	if (armies.battles[armyIndex].is_valid() && command_add_error(feedback, COMMAND_ERROR_UNSPECIFIED)) return false;

	ui16 regionIndex, tileIndex;
	world_get_tile(world.terrain, armies.locations[armyIndex], regionIndex, tileIndex);
	ui16 tileOwner = world.terrain.regions[regionIndex].owners[tileIndex];

	const world_entity_armies::warband_list& list = armies.warbands[armyIndex];
	for (ui8 slot = 0; slot < list.count; slot++)
	{
		ui16 warbandIndex = list.indices[slot];
		entity_guid home = warbands.home_settlements[warbandIndex];
		bool atHome = world_entity_is_valid(world, home) && home.get_type() == ENTITY_TYPE::SETTLEMENT
			&& world.entities.settlements.owners[home.get_index()] == warbands.owners[warbandIndex] && tileOwner == home.get_index();
		if (!atHome)
		{
			if (command_add_error(feedback, COMMAND_ERROR_TARGET_OUT_OF_TERRITORY)) return false;
			break;
		}
	}

	return command_is_valid(feedback);
}

// The men of a warband are still counted in its home settlement (see world_warband_remove_men_from_home): removing the army is all it takes to give them back.
void command_disband_army_apply(match_world_state& world, match_player_id player, const match_command_header& command_header)
{
	if (!command_disband_army_validity_check(world, player, command_header, nullptr)) return;

	const command_data_disband_army& command = command_header.get_command_data<command_data_disband_army>();
	world_siege_remove_army(world, command.army); // Does nothing outside a siege.
	world_destroy_army(world, command.army);
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

