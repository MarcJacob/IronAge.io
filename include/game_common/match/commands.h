// Symbol declarations for commands that can be sent into a match to affect its next tick.
// Each command type is built in four parts: its initial declaration in the types enumeration, the definition of a payload structure,
// the declaration & implementation of its Sanity Check and Apply functions, and its row in the command table (name, size, functions).

#ifndef GAME_COMMANDS_INCLUDED
#define GAME_COMMANDS_INCLUDED

#ifndef NDEBUG 
#define MATCH_COMMAND_DEBUG // Enables the addition of some safety properties & assertions in command structures to debug wrongful command reads / writes.
#endif

#include "core.h"
#include "match.h"
#include "world.h"

// Top-level type designations a match command can have.
enum class MATCH_COMMAND_TYPE : ui8
{
	SET_ENTITY_MOVE_TARGET, // Orders an entity to move to a target location if it can.
	SET_ENTITY_ATTACK_TARGET, // Orders an army to chase and attack another entity (not owned by the same player) until the attack resolves.
	FOUND_SETTLEMENT, // Turns an army into a settlement at its location.
	RAISE_ARMY, // Raises an army of levies from a settlement's population, moving to a raise location.

    // SERVER COMMANDS

    SERVER_SPAWN_PLAYER, // Spawn starting entity/ies for a player. Called at game start.

	BUILD_ROAD, // Has a settlement start building a road between two tiles, within its territory.

	TYPE_COUNT,
};

// COMMAND DATA STRUCTURES & FUNCTIONS
// Every command has the same two functions, taking the command header (type + data) and reading their own data struct from it:
// bool <name>_validity_check(const match_world_state&, match_player_id, const match_command_header&)
// void <name>_apply(match_world_state&, match_player_id, const match_command_header&)

struct match_command_header;

// All structures must be aligned to 1 since they must exist in packed contexts for network transmission and structured reading.
#pragma pack(push, 1)

// SERVER SPAWN PLAYER
struct command_data_server_spawn_player
{
    world_location spawn_location;
    match_player_id spawned_player;
};

bool command_server_spawn_player_validity_check(const match_world_state& world, match_player_id player,
        const match_command_header& command_header);

void command_server_spawn_player_apply(match_world_state& world, match_player_id player,
        const match_command_header& command_header);

// SET ENTITY MOVE TARGET
struct command_data_set_entity_move_target
{
	entity_guid entity;
	world_location move_target;
};

bool command_set_entity_move_target_validity_check(const match_world_state& world, match_player_id player,
	const match_command_header& command_header);

void command_set_entity_move_target_apply(match_world_state& world, match_player_id player,
	const match_command_header& command_header);

// SET ENTITY ATTACK TARGET
struct command_data_set_entity_attack_target
{
	entity_guid attacker_entity;
	entity_guid target_entity;
};

bool command_set_entity_attack_target_validity_check(const match_world_state& world, match_player_id player,
	const match_command_header& command_header);

void command_set_entity_attack_target_apply(match_world_state& world, match_player_id player,
	const match_command_header& command_header);

// FOUND SETTLEMENT
struct command_data_found_settlement
{
	entity_guid army;
};

bool command_found_settlement_validity_check(const match_world_state& world, match_player_id player,
	const match_command_header& command_header);

void command_found_settlement_apply(match_world_state& world, match_player_id player,
	const match_command_header& command_header);

// RAISE ARMY
struct command_data_raise_army
{
	entity_guid settlement;
	world_location raise_location; // Tile the new army starts moving to.
};

bool command_raise_army_validity_check(const match_world_state& world, match_player_id player,
	const match_command_header& command_header);

void command_raise_army_apply(match_world_state& world, match_player_id player,
	const match_command_header& command_header);

// BUILD ROAD
// @TODO(Marc): I'd like for there to be a single command (or at least, one per "archetype") for starting build projects.
// Available build types is going to be a center piece of the entire game economy and the subject of a lot of iterations, so it needs to be a very flexible system.
struct command_data_build_road
{
	entity_guid settlement;
	world_location origin; // Must be the settlement's tile or a road tile in its territory.
	world_location destination;
};

bool command_build_road_validity_check(const match_world_state& world, match_player_id player,
	const match_command_header& command_header);

void command_build_road_apply(match_world_state& world, match_player_id player,
	const match_command_header& command_header);

// Header & command size mapping definitions.

// Minimal definition for a single command in a buffer.
struct match_command_header
{
#ifdef MATCH_COMMAND_DEBUG
	// Byte size of the command data to use for debugging purposes.
	ui8 _data_size;
#endif

	MATCH_COMMAND_TYPE type;


	ui8 command_data[];

	// Returns a typed reference of a command payload struct on the memory immediately following this structure.
	template<typename CommandPayloadType>
	inline CommandPayloadType& get_command_data() const
	{
#ifdef MATCH_COMMAND_DEBUG
		ASSERT(_data_size == sizeof(CommandPayloadType));
#endif
		return *(CommandPayloadType*)command_data;
	}
};

// Defines a sequence of commands.
struct match_command_sequence
{
#ifdef MATCH_COMMAND_DEBUG
	ui32 _total_size; // Total byte size of the commands buffer.
#endif

	ui8 command_count;
	ui8 _commands_buffer[]; // Buffer of match commands started by their header.

	// Returns a typed reference of a command header struct at the provided byte offset.
	inline match_command_header& get_sequence_at(ui16 byte_offset) const
	{
#ifdef MATCH_COMMAND_DEBUG
		ASSERT(byte_offset < _total_size);
#endif
		return *(match_command_header*)&_commands_buffer[byte_offset];
	}
};

// Full buffer of tick commands exclusively applied over a match tick.
// Each entry in _sequences_buffer is a match_player_id followed by a match_command_sequence.
struct match_tick_commands
{
	// Total byte size of the sequences buffer.
	ui32 total_size;

	match_player_count sequences_count;
	ui8 _sequences_buffer[];

	// Returns the player ID of the sequence entry at the provided byte offset.
	inline match_player_id get_sequence_player_at(ui16 byte_offset) const
	{
		ASSERT(byte_offset < total_size);
		return *(match_player_id*)&_sequences_buffer[byte_offset];
	}

	// Returns a typed reference of the match command sequence at the provided byte offset.
	inline match_command_sequence& get_sequence_at(ui16 byte_offset) const
	{
		ASSERT(byte_offset < total_size);
		return *(match_command_sequence*)&_sequences_buffer[byte_offset];
	}
};

#pragma pack(pop)

// COMMAND TABLE
// One row per command type, in MATCH_COMMAND_TYPE order (enforced by static_asserts below).

struct match_command_def
{
	const char* name; // Bare enum identifier of the command type.
	MATCH_COMMAND_TYPE type;
	ui8 data_size;
	bool (*validity_check)(const match_world_state&, match_player_id, const match_command_header&);
	void (*apply)(match_world_state&, match_player_id, const match_command_header&);
};

// WARNING: pointers to this table or its rows, or to the validity / apply functions themselves, are NOT stable across server hot reloads
// (the table lives in the reloadable DLL's static data). Never store them in match state or any persistent memory:
// keep the command type and look the row up at call time with match_command_get_def().
static constexpr match_command_def MATCH_COMMAND_TABLE[] =
{
	{ "SET_ENTITY_MOVE_TARGET", MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET, sizeof(command_data_set_entity_move_target),
		command_set_entity_move_target_validity_check, command_set_entity_move_target_apply },
	{ "SET_ENTITY_ATTACK_TARGET", MATCH_COMMAND_TYPE::SET_ENTITY_ATTACK_TARGET, sizeof(command_data_set_entity_attack_target),
		command_set_entity_attack_target_validity_check, command_set_entity_attack_target_apply },
	{ "FOUND_SETTLEMENT", MATCH_COMMAND_TYPE::FOUND_SETTLEMENT, sizeof(command_data_found_settlement),
		command_found_settlement_validity_check, command_found_settlement_apply },
	{ "RAISE_ARMY", MATCH_COMMAND_TYPE::RAISE_ARMY, sizeof(command_data_raise_army),
		command_raise_army_validity_check, command_raise_army_apply },
	{ "SERVER_SPAWN_PLAYER", MATCH_COMMAND_TYPE::SERVER_SPAWN_PLAYER, sizeof(command_data_server_spawn_player),
		command_server_spawn_player_validity_check, command_server_spawn_player_apply },
	{ "BUILD_ROAD", MATCH_COMMAND_TYPE::BUILD_ROAD, sizeof(command_data_build_road),
		command_build_road_validity_check, command_build_road_apply },
};

constexpr ui8 MATCH_COMMAND_TABLE_COUNT = sizeof(MATCH_COMMAND_TABLE) / sizeof(MATCH_COMMAND_TABLE[0]);

static constexpr bool match_command_table_is_valid()
{
	for (ui8 row = 0; row < MATCH_COMMAND_TABLE_COUNT; row++)
	{
		if ((ui8)MATCH_COMMAND_TABLE[row].type != row) return false;
		if (MATCH_COMMAND_TABLE[row].data_size == 0) return false;
	}
	return true;
}

static_assert(MATCH_COMMAND_TABLE_COUNT == (ui8)MATCH_COMMAND_TYPE::TYPE_COUNT, "Command table row count differs from MATCH_COMMAND_TYPE::TYPE_COUNT: a command is missing its row.");
static_assert(match_command_table_is_valid(), "Command table rows must be in MATCH_COMMAND_TYPE order and have a non-zero data size.");

// Returns the table row of a command type. Asserts and returns nullptr on an unknown type.
// Untrusted types must be range-checked against TYPE_COUNT BEFORE calling this, without asserting.
static inline const match_command_def* match_command_get_def(MATCH_COMMAND_TYPE type)
{
	if ((ui8)type >= (ui8)MATCH_COMMAND_TYPE::TYPE_COUNT)
	{
		ASSERT_MSG(0, "Command type %d is not in the command table.", (int)type);
		return nullptr;
	}
	return &MATCH_COMMAND_TABLE[(ui8)type];
}

// Returns the command type with the given name. Asserts and returns TYPE_COUNT if there is none.
static inline MATCH_COMMAND_TYPE match_command_type_from_name(const char* name)
{
	for (ui8 row = 0; row < MATCH_COMMAND_TABLE_COUNT; row++)
	{
		if (ia_str_equal(MATCH_COMMAND_TABLE[row].name, name)) return MATCH_COMMAND_TABLE[row].type;
	}
	ASSERT_MSG(0, "No command type has the given name.");
	return MATCH_COMMAND_TYPE::TYPE_COUNT;
}

// Mapping between command type and size. Asserts and returns 0 on an unknown type (see match_command_get_def).
static inline ui8 get_command_data_size(MATCH_COMMAND_TYPE type)
{
	const match_command_def* def = match_command_get_def(type);
	return def != nullptr ? def->data_size : 0;
}

// Convenience tool for building a sequence of inputs inside a memory arena.
struct command_sequence_builder
{
	mem_arena* target_mem;
	match_command_sequence* _sequence_start; // Read only, use init() to create.

	ui32 total_size; // Total byte size of the sequence.

	// Initializes the builder by allocating the sequence start structure.
	// The target memory arena must be assigned !
	inline bool init()
	{
		ASSERT(target_mem != nullptr);

		_sequence_start = target_mem->alloc<match_command_sequence>();
		if (_sequence_start == nullptr) return false;

		*_sequence_start = {};

		total_size = sizeof(match_command_sequence);

		return true;
	}

	// Pushes a buffer of commands to this sequence, assuming the memory can be directly interpreted as series of <Command Header><Payload> structures.
	// Useful to copy commands from an existing sequence.
	inline bool push_commands_buffer(const ui8* commands_buff, ui16 buff_size, ui8 command_count)
	{
		ASSERT(target_mem != nullptr);
		ASSERT(_sequence_start != nullptr);

		void* sequenceContinuation = target_mem->alloc(buff_size, 1);
		if (sequenceContinuation == nullptr) return false;

		ia_memcpy(sequenceContinuation, commands_buff, buff_size);

		_sequence_start->command_count += command_count;

#ifdef MATCH_COMMAND_DEBUG
		_sequence_start->_total_size += buff_size;
#endif

		total_size += buff_size;

		return true;
	}

	// Attempts to allocate a new command. Returns the pointer to payload. 
	// The header is automatically allocated in preceding memory with the correct command type value.
	// If allocation fails, will return nullptr. In this case, abort the whole building process or just set the arena back to its previous size.
	inline void* push_command(MATCH_COMMAND_TYPE command_type)
	{
		ASSERT(target_mem != nullptr);
		ASSERT(_sequence_start != nullptr);

		ui8 payloadSize = get_command_data_size(command_type);

		match_command_header* newCommandHeader = (match_command_header*)target_mem->alloc(sizeof(match_command_header) + payloadSize, 1);
		if (newCommandHeader == nullptr) return nullptr;

		newCommandHeader->type = command_type;
		
#ifdef MATCH_COMMAND_DEBUG
		newCommandHeader->_data_size = payloadSize;
#endif

		_sequence_start->command_count++;

#ifdef MATCH_COMMAND_DEBUG
		_sequence_start->_total_size += sizeof(match_command_header) + newCommandHeader->_data_size;
#endif

		total_size += sizeof(match_command_header) + get_command_data_size(newCommandHeader->type);

		return (void*)newCommandHeader->command_data;

	}

	// Attempts to allocate a new command. Returns the payload for parameterization. 
	// The header is automatically allocated in preceding memory with the correct command type value.
	// If allocation fails, will return nullptr. In this case, abort the whole building process or just set the arena back to its previous size.
	template<typename PayloadType>
	inline PayloadType* push_command(MATCH_COMMAND_TYPE command_type)
	{	
		ASSERT(target_mem != nullptr);
		ASSERT(_sequence_start != nullptr);
		ASSERT(get_command_data_size(command_type) == sizeof(PayloadType));

		return (PayloadType*)push_command(command_type);
	}
};

// Checks that an input command would be valid to apply to the match over its next tick.
bool match_command_validity_check(const match_world_state& world, match_player_id player, const match_command_header& command);

// Outputs a clone of the passed unvalidated command sequence structure into the given builder, with only valid commands for the specified target match.
// Returns whether the unvalidated sequence could be fully validated (IE no corrupted / "fatally invalid" command was present to prevent reading the whole sequence).
bool match_command_sequence_output_validated(const game_match& target_match, const match_command_sequence& unvalidated,
	match_player_id player_id, command_sequence_builder& output_builder);

// Convenience structure for building a set of tick command sequences to apply to a match inside of a memory arena.
// An example of how to use it can be found in the match test code.
struct match_tick_commands_builder
{
	mem_arena* target_mem; // Memory the builder is building into.

	match_tick_commands* _tick_commands_start;

	command_sequence_builder _sequence_builder; // Current sequence builder used to push commands. Initialized / Replaced when calling push_sequence.
												// Do not use directly, use the enclosing tick commands builder's push_command().

	// Initializes the builder by allocating the tick commands structure from target memory.
	// Does NOT push the first sequence !
	inline bool init()
	{
		ASSERT(target_mem != nullptr);

		_tick_commands_start = target_mem->alloc<match_tick_commands>();
		if (_tick_commands_start == nullptr) return false;

		*_tick_commands_start = {};
		return true;
    }

	// Writes player_id, then creates a new sequence builder right after it, pointing on the same memory. The new
	// builder replaces the current one in the structure if any.
	// Returns whether the player id was written and the builder was successfully initialized & replaced the previous one.
	inline bool push_new_sequence(match_player_id player_id)
	{
		ASSERT(target_mem != nullptr);
		ASSERT(_tick_commands_start != nullptr);

		match_player_id* playerIdSlot = (match_player_id*)target_mem->alloc(sizeof(match_player_id), 1);
		if (playerIdSlot == nullptr) return false;
		*playerIdSlot = player_id;

		command_sequence_builder newBuilder = {};
		newBuilder.target_mem = target_mem;

		if (!newBuilder.init()) return false;

		_sequence_builder = newBuilder;
		_tick_commands_start->sequences_count++;

		// Increment total size.
		_tick_commands_start->total_size += sizeof(match_player_id) + sizeof(match_command_sequence);

		return true;
	}

	// Pushes a new sequence in the tick commands structure, ensuring each command is valid. Invalid commands are discarded.
	inline void push_validated_sequence(match_player_id player_id, const game_match& target_match, const match_command_sequence& unvalidated)
	{
		ASSERT(target_mem != nullptr);

		push_new_sequence(player_id);
		bool res = match_command_sequence_output_validated(target_match, unvalidated, player_id, _sequence_builder);

		// Increment total size.
		_tick_commands_start->total_size += _sequence_builder.total_size - sizeof(match_command_sequence); // size of the sequence struct was already counted in.
	}

	// Calls the push_commands_buffer function of the current sequence builder (will assert if none have been pushed !).
	// This is used so the match_tick_commands structure can keep track of its total size.
	// Otherwise works the same way as command_sequence_builder::push_commands_buffer().
	inline bool push_commands_buffer(const ui8* commands_buff, ui16 buff_size, ui8 command_count)
	{
		ASSERT(target_mem != nullptr);
		ASSERT(_sequence_builder.target_mem != nullptr);

		if (!_sequence_builder.push_commands_buffer(commands_buff, buff_size, command_count)) 
			return false;

		_tick_commands_start->total_size += buff_size;

		return true;
	}

	// Calls the push_command function of the current sequence builder (will assert if none have been pushed !).
	// This is used so that the match_tick_commands structure can keep track of its total size.
	// Otherwise works the same way as command_sequence_builder::push_command().
	template<typename PayloadType>
	inline PayloadType* push_command(MATCH_COMMAND_TYPE command_type)
	{
		ASSERT(target_mem != nullptr);
		ASSERT(_sequence_builder.target_mem != nullptr);
		ASSERT(get_command_data_size(command_type) == sizeof(PayloadType));

		PayloadType* payload = _sequence_builder.push_command<PayloadType>(command_type);
		if (payload == nullptr) return nullptr;

		// Increment total size.
		_tick_commands_start->total_size += sizeof(match_command_header) + sizeof(PayloadType);

		return payload;
	}
};

#endif // GAME_COMMANDS_INCLUDED
