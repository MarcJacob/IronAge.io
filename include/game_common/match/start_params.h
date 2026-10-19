// Header file specifically for the Start Parameters structure and its layout.

#ifndef MATCH_START_PARAMS_INCLUDED
#define MATCH_START_PARAMS_INCLUDED

#include "core.h"

// Pack the start params structure since, as of now, it gets sent over the network as-is.
#pragma pack(push, 1)

// Params structure for the creation of a match. Contains all necessary components to determine the match's starting state, parameters, and resource requirements.
struct game_match_start_params
{
	vec2<ui16> world_size_regions; // Dimensions of the world map in number of regions.

	ui16 player_count; // Number of players present in the match (including all types of players).
	ui8 tick_rate; // Number of ticks per second.

	ui32 random_seed; // Drives every random element of the match simulation. The same seed and same input commands will lead to the same outcome.
};

#pragma pack(pop)


#endif // MATCH_START_PARAMS_INCLUDED
