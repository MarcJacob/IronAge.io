// Map generation entry point. Unity-includes the rest of the subsystem.

#include "map_gen.h"

#include "map_gen_heightmap.cpp"
#include "map_gen_apply.cpp"

bool map_gen_generate(world_terrain& terrain, const game_map_gen_params& params, ui32 seed, mem_arena& scratch)
{
	ui64 scratchStart = scratch.allocated_count;

	map_gen_heightmap heightmap = {};
	heightmap.size = terrain.size_tiles;
	heightmap.heights = scratch.alloc<ui8>((ui64)heightmap.size.x * heightmap.size.y, false);
	if (heightmap.heights == nullptr) return false;

	map_gen_heightmap_procedural(heightmap, params, seed);
	map_gen_apply_heightmap(terrain, heightmap, params);

	scratch.allocated_count = scratchStart;
	return true;
}
