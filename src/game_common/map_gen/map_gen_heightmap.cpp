// Procedural elevation: continents / seas from fractal value noise, mountain ranges from ridged noise, sea around the world border.

// Elevation is 0 to 255. Land starts at the sea level (see map_gen_apply.cpp).
static constexpr ui32 MAP_GEN_DEFAULT_FEATURE_SCALE = 192; // Tiles (a region is 128).
static constexpr ui32 MAP_GEN_BASE_OCTAVES = 4; // Smallest feature is the scale / 8 (24 tiles by default).
static constexpr i32 MAP_GEN_BASE_CONTRAST = 2; // Spreads the (otherwise centered) noise over the elevation range.
static constexpr ui32 MAP_GEN_BORDER_SEA_WIDTH = 48; // Elevation is pulled down over this many tiles from the world border.
static constexpr i32 MAP_GEN_BORDER_SEA_DEPTH = 90; // Elevation lost at the very border.
static constexpr ui32 MAP_GEN_RIDGE_MIN = 26000; // Ridge noise (0 to 32768) above this raises mountains. Higher = narrower ranges.
static constexpr ui32 MAP_GEN_RIDGE_STEP = 90; // Ridge noise units per elevation unit gained above the ridge min.

void map_gen_heightmap_procedural(map_gen_heightmap& heightmap, const game_map_gen_params& params, ui32 seed)
{
	ui32 scale = params.feature_scale_tiles != 0 ? params.feature_scale_tiles : MAP_GEN_DEFAULT_FEATURE_SCALE;
	ui32 baseSeed = ia_hash_ui32(seed);
	ui32 ridgeSeed = ia_hash_ui32(seed ^ 0xA5A5A5A5u);
	i32 ridgeMin = (i32)MAP_GEN_RIDGE_MIN - (i32)params.mountains_bias * 100;

	for (ui32 y = 0; y < heightmap.size.y; y++)
	{
		for (ui32 x = 0; x < heightmap.size.x; x++)
		{
			// Base elevation: noise centered on 128.
			i32 elevation = 128 + ((i32)ia_fractal_noise_2d(x, y, scale, MAP_GEN_BASE_OCTAVES, baseSeed) - 32768) * MAP_GEN_BASE_CONTRAST / 256;

			// Mountain ranges: thin bands around the middle value of a coarse noise, only on land of any height.
			i32 ridge = 32768 - ia_abs((i32)ia_fractal_noise_2d(x, y, scale / 2, 2, ridgeSeed) - 32768);
			if (ridge > ridgeMin && elevation > 112) elevation += (ridge - ridgeMin) / (i32)MAP_GEN_RIDGE_STEP;

			// Sea around the world border so that continents do not run into its edge.
			ui32 borderDist = ia_min(ia_min(x, y), ia_min(heightmap.size.x - 1 - x, heightmap.size.y - 1 - y));
			if (borderDist < MAP_GEN_BORDER_SEA_WIDTH)
			{
				elevation -= (i32)(MAP_GEN_BORDER_SEA_WIDTH - borderDist) * MAP_GEN_BORDER_SEA_DEPTH / (i32)MAP_GEN_BORDER_SEA_WIDTH;
			}

			heightmap.heights[y * heightmap.size.x + x] = (ui8)ia_clamped(elevation, 0, 255);
		}
	}
}
