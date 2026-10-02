// Symbol declarations for commands that can be sent into a match to affect its next tick.
// Each command type is built in four parts: its initial declaration in the types enumeration, the definition of a payload structure,
// the type-to-payload-size matching in the function, and the declaration & implementation of its Sanity Check and Apply functions.

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
	SPAWN_ARMY, // Spawns an army of levies from a settlement's population.

	TYPE_COUNT,
};

// COMMAND DATA STRUCTURES & FUNCTIONS 

// All structures must be aligned to 1 since they must exist in packed contexts for network transmission and structured reading.
#pragma pack(push, 1)

// SET ENTITY MOVE TARGET
struct command_data_set_entity_move_target
{
	entity_guid entity;
	world_location move_target;
};

// VALIDITY CHECK
bool command_set_entity_move_target_validity_check(const game_match& match, match_player_id player,
	const command_data_set_entity_move_target& command);

// APPLY
void command_set_entity_move_target_apply(game_match& match, match_player_id player,
	const command_data_set_entity_move_target& command);

// SET ENTITY ATTACK TARGET
struct command_data_set_entity_attack_target
{
	entity_guid attacker_entity;
	entity_guid target_entity;
};

// VALIDITY CHECK
bool command_set_entity_attack_target_validity_check(const game_match& match, match_player_id player,
	const command_data_set_entity_attack_target& command);

// APPLY
void command_set_entity_attack_target_apply(game_match& match, match_player_id player,
	const command_data_set_entity_attack_target& command);

// FOUND SETTLEMENT
struct command_data_found_settlement
{
	entity_guid army;
};

// VALIDITY CHECK
bool command_found_settlement_validity_check(const game_match& match, match_player_id player,
	const command_data_found_settlement& command);

// APPLY
void command_found_settlement_apply(game_match& match, match_player_id player,
	const command_data_found_settlement& command);

// SPAWN ARMY
struct command_data_spawn_army
{
	entity_guid settlement;
};

// VALIDITY CHECK
bool command_spawn_army_validity_check(const game_match& match, match_player_id player,
	const command_data_spawn_army& command);

// APPLY
void command_spawn_army_apply(game_match& match, match_player_id player,
	const command_data_spawn_army& command);

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

// Mapping between command type and size.
static inline ui8 get_command_data_size(MATCH_COMMAND_TYPE type)
{
	switch (type)
	{
	case MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET:
		return sizeof(command_data_set_entity_move_target);
	case MATCH_COMMAND_TYPE::SET_ENTITY_ATTACK_TARGET:
		return sizeof(command_data_set_entity_attack_target);
	case MATCH_COMMAND_TYPE::FOUND_SETTLEMENT:
		return sizeof(command_data_found_settlement);
	case MATCH_COMMAND_TYPE::SPAWN_ARMY:
		return sizeof(command_data_spawn_army);
	default:
		ASSERT_MSG(0, "Command type %d is missing a data struct size association.", type);
		return 0;
	}
}

#ifndef NDEBUG
// Simple testing function to ensure all declared command types have an associated size,
// and by extension, a payload structure.
static inline void TEST_COMMAND_SIZES_CHECK()
{
	ui16 sum = 0;
	for (ui8 commandTypeIndex = 0; commandTypeIndex < (ui8)MATCH_COMMAND_TYPE::TYPE_COUNT; commandTypeIndex++)
	{
		sum += get_command_data_size((MATCH_COMMAND_TYPE)commandTypeIndex);
	}
}
#endif

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

// Outputs a clone of the passed unvalidated command sequence structure into the given builder, with only valid commands for the specified target match.
// Returns whether the unvalidated sequence could be fully validated (IE no corrupted / "fatally invalid" command was present to prevent reading the whole sequence).
bool match_command_sequence_output_validated(const game_match& target_match, const match_command_sequence& unvalidated,
	match_player_id player_id, command_sequence_builder& output_builder);

// Convenience structure for building a set of tick command sequences to apply to a match inside of a memory arena.
// An example of how to use it can be found in the match test code.
struct match_tick_commands_builder
{
	mem_arena* target_mem;

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