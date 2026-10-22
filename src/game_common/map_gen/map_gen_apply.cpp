// Turns a heightmap (from any source) into terrain tile types.

static constexpr i32 MAP_GEN_DEFAULT_SEA_LEVEL = 105; // Elevation below this is water.
static constexpr i32 MAP_GEN_DEFAULT_HILLS_HEIGHT = 40; // Elevation above the sea level where hills start.
static constexpr i32 MAP_GEN_DEFAULT_MOUNTAINS_HEIGHT = 85; // Elevation above the sea level where mountains start.
static constexpr i32 MAP_GEN_COAST_RADIUS = 2; // Water within this many tiles of land is coast.

void map_gen_apply_heightmap(world_terrain& terrain, const map_gen_heightmap& heightmap, const game_map_gen_params& params)
{
	ASSERT(heightmap.size == terrain.size_tiles);

	i32 seaLevel = MAP_GEN_DEFAULT_SEA_LEVEL + params.sea_level_bias;
	i32 hillsLevel = seaLevel + MAP_GEN_DEFAULT_HILLS_HEIGHT - params.hills_bias;
	i32 mountainsLevel = seaLevel + MAP_GEN_DEFAULT_MOUNTAINS_HEIGHT - params.mountains_bias;

	for (i32 y = 0; y < heightmap.size.y; y++)
	{
		for (i32 x = 0; x < heightmap.size.x; x++)
		{
			i32 elevation = heightmap.heights[y * heightmap.size.x + x];
			TERRAIN_TILE_TYPE type = TERRAIN_TILE_TYPE::LAND_PLAINS;

			if (elevation >= mountainsLevel) type = TERRAIN_TILE_TYPE::LAND_MOUNTAINS;
			else if (elevation >= hillsLevel) type = TERRAIN_TILE_TYPE::LAND_HILLS;
			else if (elevation < seaLevel)
			{
				type = TERRAIN_TILE_TYPE::WATER_SEA;
				for (i32 dy = -MAP_GEN_COAST_RADIUS; dy <= MAP_GEN_COAST_RADIUS && type == TERRAIN_TILE_TYPE::WATER_SEA; dy++)
				{
					for (i32 dx = -MAP_GEN_COAST_RADIUS; dx <= MAP_GEN_COAST_RADIUS; dx++)
					{
						i32 nx = x + dx;
						i32 ny = y + dy;
						if (nx < 0 || ny < 0 || nx >= heightmap.size.x || ny >= heightmap.size.y) continue;
						if (heightmap.heights[ny * heightmap.size.x + nx] >= seaLevel)
						{
							type = TERRAIN_TILE_TYPE::WATER_COAST;
							break;
						}
					}
				}
			}

			ui16 regionIndex, tileIndex;
			world_get_tile(terrain, { (ui16)x, (ui16)y }, regionIndex, tileIndex);
			terrain.regions[regionIndex].terrain_types[tileIndex] = type;
		}
	}
}
