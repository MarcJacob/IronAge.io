// Main implementation file for match input command functions.

#include "game_common/match/commands.h"
#include "game_common/match/world.h"
#include "world_sim/world_sim.h"

// BEGIN COMMAND FUNCTION IMPLEMENTATIONS

// SERVER SPAWN PLAYER

bool command_server_spawn_player_validity_check(const match_world_state& world, match_player_id player,
        const command_data_server_spawn_player& command)
{
    // Check that command was emitted by the server itself (INVALID player ID).
    if (player != INVALID_MATCH_PLAYER_ID) return false;

    // @TODO(Marc): More checks such as making sure the player doesn't own any entities.
    return true;
}

bool command_server_spawn_player_apply(match_world_state& world, match_player_id player,
        const command_data_server_spawn_player& command)
{
    if (!command_server_spawn_player_validity_check(world, player, command)) return false;

    // Spawn start settlement for the player.

    world_entity_settlements::single startSettlement = {
        .owner = command.spawned_player,
        .location = command.spawn_location,
        .population = 500,
        .tier = 1,
    };

    entity_guid newSettlement = world_spawn_settlement(world, startSettlement);
    return newSettlement.is_valid();
}


// ENTITY SET MOVE TARGET

// Example of command sanity check function.
bool command_set_entity_move_target_validity_check(const match_world_state& world, match_player_id player, const command_data_set_entity_move_target& command)
{
	if (!world_entity_is_valid(world, command.entity)) return false;

	// Check that target coordinates are inside the world.
	if (command.move_target.x < 0 
		|| command.move_target.x > world.terrain.size_tiles.x
		|| command.move_target.y < 0 
		|| command.move_target.y > world.terrain.size_tiles.y)
	{
		return false;
	}

	ENTITY_TYPE entityType = command.entity.get_type();
	switch (entityType)
	{
		// With valid types, just need to check that player can control them.
	case ENTITY_TYPE::ARMY:
		return world.entities.armies.owners[command.entity.get_index()] == player;
	default:
		// Command invalid: entity can't move or can't be ordered to move somewhere.
		return false;
	}
}

// Example of a command apply function.
void command_set_entity_move_target_apply(match_world_state& world, match_player_id player, const command_data_set_entity_move_target& command)
{
	if (!world_entity_is_valid(world, command.entity)) return;

	ENTITY_TYPE entityType = command.entity.get_type();

	switch (entityType)
	{
		// With valid types, just need to check that player can control them.
	case ENTITY_TYPE::ARMY:
		world.entities.armies.movements[command.entity.get_index()].destination = command.move_target;
		// A new move order overrides any attack the army was committed to.
		world.entities.armies.action_targets[command.entity.get_index()] = INVALID_ENTITY_GUID;
		break;
	default:
		// Command invalid: entity can't move or can't be ordered to move somewhere.
		break;
	}

}

// FOUND SETTLEMENT

bool command_found_settlement_validity_check(const match_world_state& world, match_player_id player, const command_data_found_settlement& command)
{
	if (!world_entity_is_valid(world, command.army)) return false;
	if (command.army.get_type() != ENTITY_TYPE::ARMY) return false;
	if (world.entities.armies.owners[command.army.get_index()] != player) return false;

	// The army is kept if the location is invalid or there is no room for the settlement.
	if (!world_is_valid_settlement_location(world, world.entities.armies.locations[command.army.get_index()], player)) return false;

	const world_entity_settlements& settlements = world.entities.settlements;
	return settlements.active_count < settlements.max_count;
}

// Spawns a settlement at the army's location with its owner and total manpower as population, then destroys the army.
void command_found_settlement_apply(match_world_state& world, match_player_id player, const command_data_found_settlement& command)
{
	// Checked again: earlier commands in the same tick may have destroyed this army, filled the last settlement slot or made the location invalid.
	if (!world_entity_is_valid(world, command.army)) return;
	if (command.army.get_type() != ENTITY_TYPE::ARMY) return;
	if (!world_is_valid_settlement_location(world, world.entities.armies.locations[command.army.get_index()], player)) return;

	const world_entity_settlements& settlements = world.entities.settlements;
	if (settlements.active_count >= settlements.max_count) return;

	ui16 armyIndex = command.army.get_index();
	const world_entity_armies& armies = world.entities.armies;
	const world_entity_armies::comp& composition = armies.compositions[armyIndex];

	world_entity_settlements::single newSettlement = {};
	newSettlement.owner = armies.owners[armyIndex];
	newSettlement.location = armies.locations[armyIndex];
	newSettlement.population = (ui32)composition.levies + composition.archers + composition.men_at_arms
		+ composition.horsemen + composition.knights;
    newSettlement.tier = 1;

	world_spawn_settlement(world, newSettlement);
	world_destroy_army(world, command.army);
}

// RAISE ARMY

constexpr ui32 RAISE_ARMY_POPULATION_COST = 100;

bool command_raise_army_validity_check(const match_world_state& world, match_player_id player, const command_data_raise_army& command)
{
	if (!world_entity_is_valid(world, command.settlement)) return false;
	if (command.settlement.get_type() != ENTITY_TYPE::SETTLEMENT) return false;
	if (command.raise_location.x >= world.terrain.size_tiles.x || command.raise_location.y >= world.terrain.size_tiles.y) return false;

	ui16 settlementIndex = command.settlement.get_index();
	const world_entity_settlements& settlements = world.entities.settlements;
	if (settlements.owners[settlementIndex] != player) return false;
	if (settlements.populations[settlementIndex] < RAISE_ARMY_POPULATION_COST) return false;

	// The raise location must be in the settlement's territory.
	ui16 regionIndex, tileIndex;
	world_get_tile(world.terrain, command.raise_location, regionIndex, tileIndex);
	if (world.terrain.regions[regionIndex].owners[tileIndex] != settlementIndex) return false;

	const world_entity_armies& armies = world.entities.armies;
	return armies.active_count < armies.max_count;
}

// Takes levies out of the settlement's population and raises them as an army at its location, moving to the raise location.
void command_raise_army_apply(match_world_state& world, match_player_id player, const command_data_raise_army& command)
{
	// Checked again: earlier commands in the same tick may have changed the settlement or filled the last army slot.
	if (!command_raise_army_validity_check(world, player, command)) return;

	ui16 settlementIndex = command.settlement.get_index();
	const world_entity_settlements& settlements = world.entities.settlements;

	world_entity_armies::single newArmy = {};
	newArmy.owner = settlements.owners[settlementIndex];
	newArmy.location = settlements.locations[settlementIndex];
	newArmy.composition.levies = RAISE_ARMY_POPULATION_COST;

	// Spawn first: decreasing the population may destroy the settlement.
	entity_guid armyGuid = world_spawn_army(world, newArmy);
	// Set after spawning: the spawn function defaults an all-zero destination to the army's location.
	world.entities.armies.movements[armyGuid.get_index()].destination = command.raise_location;
	world_settlement_decrease_population(world, command.settlement, RAISE_ARMY_POPULATION_COST);
}

// ENTITY SET ATTACK TARGET

bool command_set_entity_attack_target_validity_check(const match_world_state& world, match_player_id player, const command_data_set_entity_attack_target& command)
{
	if (!world_entity_is_valid(world, command.attacker_entity)) return false;
	if (command.attacker_entity.get_type() != ENTITY_TYPE::ARMY) return false;
	if (world.entities.armies.owners[command.attacker_entity.get_index()] != player) return false;

	// Targets owned by the sender can't be attacked (see match_caravan_is_owned_by for caravans).
	ui16 targetIndex = command.target_entity.get_index();
	const match_world_state::entities_store& entities = world.entities;
	switch (command.target_entity.get_type())
	{
	case ENTITY_TYPE::SETTLEMENT:
		return entities.settlements.owners[targetIndex] != player;
	case ENTITY_TYPE::ARMY:
		return entities.armies.owners[targetIndex] != player;
	case ENTITY_TYPE::CARAVAN:
		return !world_caravan_is_owned_by(world, command.target_entity, player);
	default:
		return false;
	}
}

// Commits the army to chasing the target. The chase and its resolution are handled by the match tick.
void command_set_entity_attack_target_apply(match_world_state& world, match_player_id player, const command_data_set_entity_attack_target& command)
{
	// Checked again: earlier commands in the same tick may have destroyed the attacker or the target.
	if (!command_set_entity_attack_target_validity_check(world, player, command)) return;

	world.entities.armies.action_targets[command.attacker_entity.get_index()] = command.target_entity;
}

// BUILD ROAD

bool command_build_road_validity_check(const match_world_state& world, match_player_id player, const command_data_build_road& command)
{
    // Check validity of entity, it must be an active settlement.
	if (!world_entity_is_valid(world, command.settlement)) return false;
	if (command.settlement.get_type() != ENTITY_TYPE::SETTLEMENT) return false;

    // The settlement must be owned by the player emitting the command.
	ui16 settlementIndex = command.settlement.get_index();
	const world_entity_settlements& settlements = world.entities.settlements;
	if (settlements.owners[settlementIndex] != player) return false;

    // The origin and destination must be in the world, and not be equal to eachother.
    if (command.origin == command.destination) return false;

	const world_terrain& terrain = world.terrain;
	if (command.origin.x >= terrain.size_tiles.x || command.origin.y >= terrain.size_tiles.y) return false;
	if (command.destination.x >= terrain.size_tiles.x || command.destination.y >= terrain.size_tiles.y) return false;

	// The road starts from the settlement itself, or from a road tile in its territory.
	if (!(command.origin == settlements.locations[settlementIndex]))
	{
		ui16 regionIndex, tileIndex;
		world_get_tile(terrain, command.origin, regionIndex, tileIndex);
		if (!terrain.regions[regionIndex].status[tileIndex].is_road) return false;
		if (terrain.regions[regionIndex].owners[tileIndex] != settlementIndex) return false;
	}

	const world_entity_builders& builders = world.entities.builders;
	return builders.active_count < builders.max_count;
}

// Spawns a road builder tied to the settlement. The road itself is built over the following ticks (see the construction system).
void command_build_road_apply(match_world_state& world, match_player_id player, const command_data_build_road& command)
{
	// Checked again: earlier commands in the same tick may have changed the settlement or filled the last builder slot.
	if (!command_build_road_validity_check(world, player, command)) return;

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

// END COMMAND FUNCTION IMPLEMENTATIONS

// BEGIN CORE COMMAND HANDLING FUNCTIONS

// Checks that an input command would be valid to apply to the match over its next tick.
bool match_command_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command)
{
	switch (command.type)
	{
		case MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET:
			return command_set_entity_move_target_validity_check(world, player, command.get_command_data<command_data_set_entity_move_target>());
		case MATCH_COMMAND_TYPE::SET_ENTITY_ATTACK_TARGET:
			return command_set_entity_attack_target_validity_check(world, player, command.get_command_data<command_data_set_entity_attack_target>());
		case MATCH_COMMAND_TYPE::FOUND_SETTLEMENT:
			return command_found_settlement_validity_check(world, player, command.get_command_data<command_data_found_settlement>());
		case MATCH_COMMAND_TYPE::RAISE_ARMY:
			return command_raise_army_validity_check(world, player, command.get_command_data<command_data_raise_army>());
        case MATCH_COMMAND_TYPE::SERVER_SPAWN_PLAYER:
            return command_server_spawn_player_validity_check(world, player, command.get_command_data<command_data_server_spawn_player>());
		case MATCH_COMMAND_TYPE::BUILD_ROAD:
			return command_build_road_validity_check(world, player, command.get_command_data<command_data_build_road>());
		default:
			ASSERT_MSG(0, "No validity check logic associated with command type.");
			return false;
	}
}

inline void match_command_apply(match_world_state& world, match_player_id player, const match_command_header& command)
{
	// Route to type-specific logic.
	switch (command.type)
	{
	case MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET:
		command_set_entity_move_target_apply(world, player,
			command.get_command_data<command_data_set_entity_move_target>());
		break;
	case MATCH_COMMAND_TYPE::SET_ENTITY_ATTACK_TARGET:
		command_set_entity_attack_target_apply(world, player,
			command.get_command_data<command_data_set_entity_attack_target>());
		break;
	case MATCH_COMMAND_TYPE::FOUND_SETTLEMENT:
		command_found_settlement_apply(world, player, command.get_command_data<command_data_found_settlement>());
		break;
	case MATCH_COMMAND_TYPE::RAISE_ARMY:
		command_raise_army_apply(world, player, command.get_command_data<command_data_raise_army>());
		break;
    case MATCH_COMMAND_TYPE::SERVER_SPAWN_PLAYER:
        command_server_spawn_player_apply(world, player, command.get_command_data<command_data_server_spawn_player>());
        break;
	case MATCH_COMMAND_TYPE::BUILD_ROAD:
		command_build_road_apply(world, player, command.get_command_data<command_data_build_road>());
		break;
	default:
		ASSERT_MSG(0, "No apply logic associated with command type.");
	}
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

		// Route to type-specific logic.
		bool isValid = false;
		switch (commandHeader.type)
		{
		case MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET:
			isValid = command_set_entity_move_target_validity_check(world, player_id,
				commandHeader.get_command_data<command_data_set_entity_move_target>());
			break;
		case MATCH_COMMAND_TYPE::SET_ENTITY_ATTACK_TARGET:
			isValid = command_set_entity_attack_target_validity_check(world, player_id,
				commandHeader.get_command_data<command_data_set_entity_attack_target>());
			break;
		case MATCH_COMMAND_TYPE::FOUND_SETTLEMENT:
			isValid = command_found_settlement_validity_check(world, player_id,
				commandHeader.get_command_data<command_data_found_settlement>());
			break;
		case MATCH_COMMAND_TYPE::RAISE_ARMY:
			isValid = command_raise_army_validity_check(world, player_id,
				commandHeader.get_command_data<command_data_raise_army>());
			break;
		case MATCH_COMMAND_TYPE::BUILD_ROAD:
			isValid = command_build_road_validity_check(world, player_id,
				commandHeader.get_command_data<command_data_build_road>());
			break;
		default:
			// Only assert if the command type IS valid but not handled.
			if ((i8)commandHeader.type >= 0 && (i8)commandHeader.type < (i8)MATCH_COMMAND_TYPE::TYPE_COUNT)
			{
				ASSERT_MSG(0, "No validity check logic associated with command type %d.", commandHeader.type);
			}

			// From there the whole unvalidated sequence becomes impossible to keep reading safely.
			// Stop process now and signal caller that the sequence couldn't be fully validated.
			return false;
		}
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

