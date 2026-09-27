// Symbol declarations for commands that can be sent into a match to affect its next tick.
// Each command type is built in four parts: its initial declaration in the types enumeration, the definition of a payload structure,
// the type-to-payload-size matching in the function, and the declaration & implementation of its Sanity Check and Apply functions.

#ifndef GAME_COMMANDS_INCLUDED
#define GAME_COMMANDS_INCLUDED

#ifndef NDEBUG 
#define MATCH_COMMAND_DEBUG // Enables the addition of some safety properties & assertions in command structures to debug wrongful command reads / writes.
#endif

#include "core.h"
#include "game_match.h"

// Top-level type designations a match command can have.
enum class MATCH_COMMAND_TYPE : ui8
{
	SET_ENTITY_MOVE_TARGET,

	TYPE_COUNT,
};

// COMMAND STRUCTURES & PAYLOADS

// All structures must be aligned to 1 since they must exist in packed contexts for network transmission and structured reading.
#pragma pack(push, 1)

struct command_payload_set_entity_move_target
{
	entity_id target_entity;
	world_location new_target;
};

// Header & command size mapping definitions.

// Minimal definition for a single command in a buffer.
struct match_command_header
{
#ifdef MATCH_COMMAND_DEBUG
	// Byte size of the command data.
	ui8 _data_size;
#endif

	MATCH_COMMAND_TYPE type;


	ui8 command_data[];

	// Returns a typed reference of a command payload struct on the memory immediately following this structure.
	template<typename CommandPayloadType>
	inline CommandPayloadType& get_command_payload() const
	{
#ifdef MATCH_COMMAND_DEBUG
		ASSERT(_data_size == sizeof(CommandPayloadType));
#endif
		return *(CommandPayloadType*)command_data;
	}
};

// Defines a sequence of commands associated to a player ID.
struct match_command_sequence
{
#ifdef MATCH_COMMAND_DEBUG
	ui32 _total_size; // Total byte size of the commands buffer.
#endif

	match_player_id player_id; // Player ID wishing to apply the sequence of commands.

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
struct match_tick_commands
{
#ifdef MATCH_COMMAND_DEBUG
	// Total byte size of the sequences buffer.
	ui32 _total_size;
#endif

	match_player_count sequences_count;
	ui8 _sequences_buffer[];

	// Returns a typed reference of a match command sequence start struct at the provided byte offset.
	inline match_command_sequence& get_sequence_at(ui16 byte_offset) const
	{
		ASSERT(byte_offset < _total_size);
		return *(match_command_sequence*)&_sequences_buffer[byte_offset];
	}
};

#pragma pack(pop)

// Mapping between command type and size.
static inline ui8 get_command_size(MATCH_COMMAND_TYPE type)
{
	switch (type)
	{
	case(MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET):
		return sizeof(command_payload_set_entity_move_target);
	default:
		ASSERT_MSG(0, "Command type %d is missing a payload struct size association.", type);
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
		sum += get_command_size((MATCH_COMMAND_TYPE)commandTypeIndex);
	}
}
#endif

// Convenience tool for building a sequence of inputs inside a memory arena.
struct command_sequence_builder
{
	mem_arena* target_mem;
	match_command_sequence* _sequence_start; // Read only, use init() to create.

	// Initializes the builder by allocating the sequence start structure.
	// The target memory arena must be assigned !
	inline bool init(match_player_id player_id)
	{
		ASSERT(target_mem != nullptr);

		_sequence_start = target_mem->alloc<match_command_sequence>();
		if (_sequence_start == nullptr) return false;

		*_sequence_start = {};

		_sequence_start->player_id = player_id;
		return true;
	}

	// Attempts to allocate a new command. Returns the payload for parameterization. 
	// The header is automatically allocated in preceding memory with the correct command type value.
	// If allocation fails, will return nullptr. In this case, abort the whole building process or just set the arena back to its previous size.
	template<typename PayloadType>
	inline PayloadType* push_command(MATCH_COMMAND_TYPE command_type)
	{
		ASSERT(target_mem != nullptr);
		ASSERT(_sequence_start != nullptr);
		ASSERT(get_command_size(command_type) == sizeof(PayloadType));

		match_command_header* newCommandHeader = (match_command_header*)target_mem->alloc(sizeof(match_command_header) + sizeof(PayloadType), 1);
		if (newCommandHeader == nullptr) return nullptr;

		newCommandHeader->type = command_type;
		
#ifdef MATCH_COMMAND_DEBUG
		newCommandHeader->_data_size = sizeof(PayloadType);
#endif

		_sequence_start->command_count++;

#ifdef MATCH_COMMAND_DEBUG
		_sequence_start->_total_size += sizeof(newCommandHeader) + newCommandHeader->_data_size;
#endif

		return (PayloadType*)newCommandHeader->command_data;
	}
};

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

	// Creates a new sequence builder pointing on the same memory and initializing it with the provided player id.
	// The new builder replaces the current one in the structure if any.
	// Returns whether the buider was successfully initialized & replaced the previous one.
	inline bool push_sequence(match_player_id player_id)
	{
		ASSERT(target_mem != nullptr);
		ASSERT(_tick_commands_start != nullptr);

		// Init builder with same target memory and provided player id.
		command_sequence_builder newBuilder = {};
		newBuilder.target_mem = target_mem;

		if (!newBuilder.init(player_id)) return false;

		_sequence_builder = newBuilder;
		_tick_commands_start->sequences_count++;

#ifdef MATCH_COMMAND_DEBUG
		_tick_commands_start->_total_size += sizeof(match_command_sequence);
#endif

		return true;
	}

	// Calls the push_command function of the current sequence builder (will assert if none have been pushed !).
	// This is used so that the match_tick_commands structure can keep track of its total size.
	// Otherwise works the same way as command_sequence_builder::push_command().
	template<typename PayloadType>
	inline PayloadType* push_command(MATCH_COMMAND_TYPE command_type)
	{
		ASSERT(target_mem != nullptr);
		ASSERT(get_command_size(command_type) == sizeof(PayloadType));

#ifdef MATCH_COMMAND_DEBUG
		ui32 prevSequenceSize = _sequence_builder._sequence_start->_total_size;
#endif

		PayloadType* payload = _sequence_builder.push_command<PayloadType>(command_type);
		if (payload == nullptr) return nullptr;

#ifdef MATCH_COMMAND_DEBUG
		ui32 sequenceGrowth = _sequence_builder._sequence_start->_total_size - prevSequenceSize;
		_tick_commands_start->_total_size += sequenceGrowth;
#endif

		return payload;
	}

};

// COMMAND FUNCTIONS DECLARATIONS

// SET ENTITY MOVE TARGET

// VALIDITY CHECK
bool command_set_entity_move_target_validity_check(game_match& match, match_player_id player,
	command_payload_set_entity_move_target& command);

// APPLY
void command_set_entity_move_target_apply(game_match& match, match_player_id player,
	command_payload_set_entity_move_target& command);

// ...

#endif // GAME_COMMANDS_INCLUDED