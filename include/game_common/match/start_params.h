// Header file specifically for the Start Parameters structure and its layout.

#ifndef MATCH_START_PARAMS_INCLUDED
#define MATCH_START_PARAMS_INCLUDED

#include "core.h"
#include "world.h"

// Pack the start params structure since, as of now, it gets sent over the network as-is.
#pragma pack(push, 1)

// Params structure for the creation of a match. Contains all necessary components to determine the match's starting state, parameters, and resource requirements.
struct game_match_start_params
{
	vec2<ui16> world_size_regions; // Dimensions of the world map in number of regions.

	ui16 player_count; // Number of players present in the match (including all types of players).
	ui8 tick_rate; // Number of ticks per second.

	ui32 random_seed; // Drives every random element of the match simulation. The same seed and same input commands will lead to the same outcome.

	ui16 start_settlement_count; // Number of settlements at the start of the game. Find their states in extra data.

	// Extra Data. Layout:
	// - Settlement Start states
	// ...

	ui16 extra_data_size; // Total amount of extra data bytes.
	ui8 _extra_data[]; // Dynamically-allocated data attached to this match, sized according to parameters like player count.

	// Access a settlement start date in the extra data for reading or editing.
	inline world_entity_settlements::single& get_settlement_start_state(ui16 index) const
	{
		ASSERT(sizeof(world_entity_settlements::single) * index < extra_data_size);
		ASSERT(index < start_settlement_count);

		world_entity_settlements::single* settlements = (world_entity_settlements::single*)_extra_data;
		return settlements[index];
	}
};

#pragma pack(pop)


#endif // MATCH_START_PARAMS_INCLUDED
