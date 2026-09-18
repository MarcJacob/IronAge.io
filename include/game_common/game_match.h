// Main symbols declaration for the ability to create and simulate game matches, used on clients and game servers as part of a Lockstep strategy for synchronization.

#ifndef GAME_MATCH_INCLUDED
#define GAME_MATCH_INCLUDED

#include "core.h"

// TODO(Marc): Temp defines that should really be parameters.
#define MIN_WORLD_DIM_SIZE (100) // Minimum width and height of a match's world.

// Params structure for the creation of a match. Contains all necessary components to determine the match's starting state, parameters, and resource requirements.
struct game_match_create_params
{
	ui16 world_width, world_height; // Dimensions of the world map in number of tiles.

	ui8 tick_rate; // Number of ticks per second. This, alongside the match start time, allows knowing how far behind or ahead in time the local match simulation is.
};

struct match_world_state
{
	int entity_loc_x, entity_loc_y;
	int entity_target_x, entity_target_y;
};

// Main memory ownership and definition structure for a single ongoing match.
struct game_match
{
	ui64 start_time; // Epoch time at which this match started its first simulation tick.

	game_match_create_params start_params; // Parameters used to start the match.

	mem_arena* memory; // Memory arena assigned to this match. Usually the match structure itself will be placed within it.

	ui32 tick; // Next tick to be computed.

	match_world_state* world_state;
};

// Creates a new game match instance from the provided creation parameters.
// The match places itself at the start of the provided memory.
// TODO(Marc): Move this out of the header !!
game_match* match_create(mem_arena& match_mem, game_match_create_params& params);

// Initializes the match world state and registers its start time for the purpose of linking tick to time through the tick rate (TODO).
void match_start(game_match& match, ui64 start_time_epoch);

// Contains the aggregates Commands to be applied to a match over its next tick.
struct match_tick_commands
{
	int target_x, target_y;
};

// Advances time in a match's world simulation. Requires the aggregated inputs / commands to apply into the tick.
void match_tick(game_match& match, const match_tick_commands& input);

// Scripted test scenario shared by all hosts, used to compare simulation results across platforms (determinism checks).
// Creates and starts a match in the provided memory, then runs the whole scenario.
// Returns the finished match, ready to be dumped, or nullptr if the match couldn't be created.
game_match* match_run_test_scenario(mem_arena& match_mem);

// Wrapper for a byte-dump function with convenient methods.
struct match_dump_stream
{
	typedef void (*dump_func_ptr)(void* state, ui8* bytes, ui64 byte_count);
	dump_func_ptr dump_func;
	ui64 dump_size;

	void* state; // User-set state pointer passed to the dump function.

	inline void dump(ui8* bytes, ui64 byte_count) { 
		if (dump_func != nullptr) dump_func(state, bytes, byte_count);
		dump_size += byte_count;
	}

	template<typename Type>
	inline void dump(Type& item) { dump((ui8*)&item, sizeof(Type)); }

	template<typename Type>
	inline void dump(Type* items, ui64 item_count) { dump((ui8*)items, item_count * sizeof(Type)); }
};

// Dumps the match's current state into the target memory using a deterministic binary format.
// Used for testing determinism, taking snapshots...
// 
// Returns the size of the produced snapshot if successful, with 0 meaning there was an error.
// If dump_stream's dump function is null, then will perform a "dry run" for the purpose of determining the size of the snapshot so exactly enough memory can be allocated.
// Otherwise will use dump function to send bytes to whatever we're dumping into.
ui64 match_dump_gamestate(game_match& match, match_dump_stream& dump_stream);

#endif // GAME_MATCH_INCLUDED