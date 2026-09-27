// Main symbols declaration for the ability to create and simulate game matches, used on clients and game servers as part of a Lockstep strategy for synchronization.

#ifndef GAME_MATCH_INCLUDED
#define GAME_MATCH_INCLUDED

#include "core.h"

// TODO(Marc): Temp defines that should really be parameters.
static constexpr ui16 MIN_WORLD_DIM_SIZE = 100; // Minimum width and height of a match's world.
static constexpr ui8 MATCH_DEFAULT_TICK_RATE = 20;	// Default Ticks per second. Used by game mechanics so time-relative elements can be defined using time units,
													// and by host app to know how many ticks it should have simulated for the match by now.
using world_location = vec2<ui16>;
using entity_id = ui32;

// Temporary arbitrary structure for the state of the various entities / objects inside the match world.
struct match_world_state
{
	ui16 entity_count;

	struct entity
	{
		world_location location;
		world_location target_location;
	} *entity_states;
};

using match_player_id = ui16;
using match_player_count = match_player_id; // Alias type for places where we need to count player.

static constexpr match_player_id INVALID_MATCH_PLAYER_ID = ~0;

// Params structure for the creation of a match. Contains all necessary components to determine the match's starting state, parameters, and resource requirements.
struct game_match_start_params
{
	vec2<ui16> world_dimensions; // Dimensions of the world map in number of tiles.

	ui16 player_count; // Number of players present in the match (including all types of players).
	ui8 tick_rate; // Number of ticks per second.
	ui32 max_tick; // Maximum number of ticks before forcing the match to end.

	// Extra Data. Layout:
	// - Player start location indexed by player.
	// ...

	ui16 extra_data_size; // Total amount of extra data bytes.
	ui8 _extra_data[]; // Dynamically-allocated data attached to this match, sized according to parameters like player count.

	inline world_location& get_player_start_pos(ui16 player_index) const
	{
		ASSERT(sizeof(world_location) * player_index < extra_data_size);

		world_location* world_locs = (world_location*)_extra_data;
		return world_locs[player_index];
	}
};

// Main memory ownership and definition structure for a single ongoing match.
struct game_match
{
	ui64 start_time; // Epoch time at which this match started its first simulation tick.
	game_match_start_params* start_params; // Parameters used to start the match.

	mem_arena* memory; // Memory arena this match will use to allocate memory as needed.

	ui32 tick; // Next tick to be computed.
	match_world_state world_state; // Observable world state associated to this match, recomputed on each tick.
};

// Returns the estimated maximum required memory for a match started with the given parameters.
constexpr ui64 match_get_required_mem(game_match_start_params& params);

// Create a new match from the memory it should use, a start time and start parameters.
// Returns false if any of the parameters were invalid / not enough memory was available.
bool match_start(mem_arena& match_mem, time_ms start_time, game_match_start_params& params, game_match& out_match);

// Advances time in a match's world simulation. Requires the aggregated inputs / commands to apply into the tick.
struct match_tick_commands;
void match_tick(game_match& match, const match_tick_commands& commands);

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