// Procedural map generation: internal declarations.
//
// Self-contained Component of game common (the world simulation uses it, it knows nothing about the simulation).
// It only relies on the public terrain structures. To unplug it: remove the include in world_sim_main.cpp and the
// call in world_state_init, then the procedural_map / map_gen start params.
//
// Shape: a "heightmap" (one elevation byte per tile, whole world) is the intermediate. Any source can fill it
// (the procedural one today, an image loader later), and map_gen_apply_heightmap turns it into terrain tile types.

#ifndef MAP_GEN_INCLUDED
#define MAP_GEN_INCLUDED

#include "core.h"
#include "game_common/match/map_gen_params.h"
#include "game_common/match/world/world_terrain.h"

// Elevation of every tile of the world, row-major (index = y * size.x + x).
struct map_gen_heightmap
{
	world_dimensions size; // In tiles.
	ui8* heights;
};

// Fills the heightmap (heights must hold size.x * size.y bytes) with procedural elevation. Pure function of (position, params, seed).
void map_gen_heightmap_procedural(map_gen_heightmap& heightmap, const game_map_gen_params& params, ui32 seed);

// Classifies the heightmap into terrain tile types and writes them in the terrain, which must have the same size. Only touches terrain_types.
void map_gen_apply_heightmap(world_terrain& terrain, const map_gen_heightmap& heightmap, const game_map_gen_params& params);

// Entry point: generates the terrain's tile types from the seed. Uses scratch memory from the arena and gives it back.
// Returns false if there is not enough memory.
bool map_gen_generate(world_terrain& terrain, const game_map_gen_params& params, ui32 seed, mem_arena& scratch);

#endif // MAP_GEN_INCLUDED
