// Main symbols declaration for the ability to create and simulate game matches, used on clients and game servers as part of a Lockstep strategy for synchronization.

#ifndef GAME_MATCH_PROC_INCLUDED
#define GAME_MATCH_PROC_INCLUDED

#include "core.h"

// TODO(Marc): Temp defines that should really be parameters.
#define MIN_WORLD_DIM_SIZE (100) // Minimum width and height of a match's world.

// Params structure for the creation of a match. Contains all necessary components to determine the match's starting state, parameters, and resource requirements.
struct game_match_create_params
{
	ui16 world_width, world_height; // Dimensions of the world map in number of tiles.
};

// Main memory ownership and definition structure for a single ongoing match.
struct game_match
{
	ui64 start_time; // Epoch time at which this match started its first simulation tick.

	game_match_create_params params;

	struct
	{
		ui8* start_ptr;
		ui32 size;
	} memory; // Allocated memory for this match. TODO(Marc): make it dynamic ?
};

// Creates a new game match instance from the provided creation parameters.
// The match instance is placed at the provided memory pointer.
// TODO(Marc): Move this out of the header !!
static inline game_match* match_create(ui8* memory, ui32 mem_size, game_match_create_params& params)
{ 
	if (params.world_width < MIN_WORLD_DIM_SIZE
		|| params.world_width < MIN_WORLD_DIM_SIZE)
	{
		return nullptr;
	}

	game_match* newMatch = (game_match*)(memory);
	*newMatch = {};
	newMatch->params = params;
}

#endif // GAME_MATCH_PROC_INCLUDED