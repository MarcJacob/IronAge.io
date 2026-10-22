// Parameters of the procedural map generator. Zero is the default for every field.

#ifndef MATCH_MAP_GEN_PARAMS_INCLUDED
#define MATCH_MAP_GEN_PARAMS_INCLUDED

#include "core/std_types.h"

// Packed since it is part of the start params, which get sent over the network as-is.
#pragma pack(push, 1)

struct game_map_gen_params
{
	i8 sea_level_bias; // Positive = more water, negative = more land.
	i8 hills_bias; // Positive = more hills.
	i8 mountains_bias; // Positive = more mountains.
	ui16 feature_scale_tiles; // Size in tiles of the largest features (continents / seas). 0 = default (192, about 1.5 regions).
};

#pragma pack(pop)

#endif // MATCH_MAP_GEN_PARAMS_INCLUDED
