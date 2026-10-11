// Render-state building for the client backend.

#include "game_client.h"
#include "game_client/game_client_backend.h"
#include "game_common/match/world.h"

static constexpr ui8 SETTLEMENT_RENDER_SIZE = 12; // Square size of a settlement icon in viewport tiles.
static constexpr ui8 CARAVAN_RENDER_SIZE = 6; // Square size of a caravan icon in viewport tiles.
static constexpr ui8 ARMY_RENDER_SIZE = 6; // Square size of an army icon in viewport tiles.

static bool is_visible_in_viewport(vec2<float> loc, const client_render_state::viewport_state& viewport)
{
    return (loc.x >= viewport.bottom_left.x && loc.x < viewport.bottom_left.x + viewport.dimensions.x)
        && (loc.y >= viewport.bottom_left.y && loc.y < viewport.bottom_left.y + viewport.dimensions.y);
}

client_render_state* game_client_get_render_state(game_client& backend)
{
	return &backend.render_state;
}

void game_client_render_entities(client_render_state& render_state, const game_match& match)
{
		// Build render entity states.
    const match_world_state& world = *match.world;

	const world_entity_settlements& settlements = world.entities.settlements;
	const world_entity_caravans& caravans = world.entities.caravans;
	const world_entity_armies& armies = world.entities.armies;

	render_state.frame.entity_count = 0;
	render_state.frame.entity_states = nullptr;

	// TODO: Come up with a way to simplify this code. A lot of entities share the same properties that are relevant to the render state,
	// and share the same pattern for initializing it.

	ui32 totalEntityCount = settlements.active_count
		+ caravans.active_count
		+ armies.active_count;

	if (totalEntityCount > 0)
	{
		render_entity* entityStates = render_state.frame.memory.alloc<render_entity>(totalEntityCount);
		ui16 visibleCount = 0;

		// SETTLEMENTS
		for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
		{
			vec2<float> settlementLoc = settlements.locations[settlementIndex];
			if (!settlements.guids[settlementIndex].is_valid() || !is_visible_in_viewport(settlementLoc, render_state.viewport)) continue;

			render_entity& outEntity = entityStates[visibleCount];
			outEntity.entity_type = ENTITY_TYPE::SETTLEMENT;
			outEntity.owner = settlements.owners[settlementIndex];
			outEntity.guid = settlements.guids[settlementIndex];
			outEntity.viewport_location = settlementLoc - render_state.viewport.bottom_left;

			visibleCount++;
		}

		// CARAVANS
		for (ui16 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
		{
			vec2<float> caravanLoc = caravans.locations[caravanIndex];
			if (!caravans.guids[caravanIndex].is_valid() || !is_visible_in_viewport(caravanLoc, render_state.viewport)) continue;

			render_entity& outEntity = entityStates[visibleCount];
			outEntity.entity_type = ENTITY_TYPE::CARAVAN;

            // @TEMP: Let's never mark caravan ownership until we figure out what to put in there exactly.
			match_player_id owner = INVALID_MATCH_PLAYER_ID;

			outEntity.owner = owner;
			outEntity.guid = caravans.guids[caravanIndex];
			outEntity.viewport_location = caravanLoc - render_state.viewport.bottom_left;

			visibleCount++;
		}

		// ARMIES
		for (ui16 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
		{
			vec2<float> armyLoc = armies.locations[armyIndex];
			if (!armies.guids[armyIndex].is_valid() || !is_visible_in_viewport(armyLoc, render_state.viewport)) continue;

			render_entity& outEntity = entityStates[visibleCount];
			outEntity.entity_type = ENTITY_TYPE::ARMY;
			outEntity.owner = armies.owners[armyIndex];
			outEntity.guid = armies.guids[armyIndex];
			outEntity.viewport_location = armyLoc - render_state.viewport.bottom_left;

			visibleCount++;
		}

		render_state.frame.entity_count = visibleCount;
		render_state.frame.entity_states = entityStates;
	}
}

void game_client_render_influence(client_render_state& render_state, const game_match& match)
{
    ASSERT(match.world != nullptr);

    const world_terrain& terrain = match.world->terrain;
    for (ui16 regionX = 0; regionX < terrain.size_regions.x; regionX++)
    {
        for (ui16 regionY = 0; regionY < terrain.size_regions.y; regionY++)
        {
            ui16 regionIndex = regionY * terrain.size_regions.x + regionX;
            for (ui16 tileY = 0; tileY < world_terrain::REGION_SIZE; tileY++)
            {
                for (ui16 tileX = 0; tileX < world_terrain::REGION_SIZE; tileX++)
                {
                    ui16 tileIndex = tileY * world_terrain::REGION_SIZE + tileX;
                    ui16 worldX = regionX * world_terrain::REGION_SIZE + tileX;
                    ui16 worldY = regionY * world_terrain::REGION_SIZE + tileY;
                    ui64 worldTileIndex = (ui64)worldY * terrain.size_tiles.x + worldX;

                    render_state.tiles.influence_tiles_bitmap[worldTileIndex] = terrain.regions[regionIndex].influence_levels[tileIndex];
                    render_state.tiles.contestation_tiles_bitmap[worldTileIndex] = terrain.regions[regionIndex].contestation_levels[tileIndex];
                }
            }
        }
    }
}

void game_client_rebuild_render_state(client_render_state& render_state, const game_match& match)
{
    const match_world_state& world = *match.world;
	render_state.world_size = world.terrain.size_tiles;

    // Rebuild frame data.
    render_state.frame.memory.clear();
    game_client_render_entities(render_state, match);

    // Update tile influence & contestation bitmaps.
    game_client_render_influence(render_state, match);
}
