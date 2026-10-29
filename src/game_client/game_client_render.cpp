// Render-state building for the client backend.

#include "game_client.h"
#include "game_client/game_client_backend.h"
#include "game_common/match/world/world_state.h"


static constexpr ui8 SETTLEMENT_RENDER_SIZE = 12; // Square size of a settlement icon in viewport tiles.
static constexpr ui8 CARAVAN_RENDER_SIZE = 6; // Square size of a caravan icon in viewport tiles.
static constexpr ui8 ARMY_RENDER_SIZE = 6; // Square size of an army icon in viewport tiles.

static bool is_visible_in_viewport(vec2<float> loc, const client_render_state::viewport_state& viewport)
{
    return (loc.x >= viewport.bottom_left.x && loc.x < viewport.bottom_left.x + viewport.dimensions.x)
        && (loc.y >= viewport.bottom_left.y && loc.y < viewport.bottom_left.y + viewport.dimensions.y);
}

// Whether the tiles within the given inclusive tile bounds overlap the viewport at all.
static bool bounds_touch_viewport(world_location bounds_min, world_location bounds_max, const client_render_state::viewport_state& viewport)
{
    return (float)bounds_max.x + 1.f > viewport.bottom_left.x && (float)bounds_min.x < viewport.bottom_left.x + viewport.dimensions.x
        && (float)bounds_max.y + 1.f > viewport.bottom_left.y && (float)bounds_min.y < viewport.bottom_left.y + viewport.dimensions.y;
}

client_render_state* game_client_get_render_state(game_client& backend)
{
	return &backend.render_state;
}

void game_client_render_entities(client_render_state& render_state, const game_match& match)
{
		// Build render entity states.
    const match_world_state& world = *match.world;

	const world_entity_settlements& settlements = world.entities->settlements;
	const world_entity_armies& armies = world.entities->armies;
	const world_entity_workers& workers = world.entities->workers;
	const world_entity_battles& battles = world.entities->battles;
	const world_entity_caravans& caravans = world.entities->caravans;

	render_state.frame.entity_count = 0;
	render_state.frame.entity_states = nullptr;

	// TODO: Come up with a way to simplify this code. A lot of entities share the same properties that are relevant to the render state,
	// and share the same pattern for initializing it.

	ui32 totalEntityCount = settlements.active_count
		+ armies.active_count
		+ workers.active_count
		+ battles.active_count
		+ caravans.active_count;

	if (totalEntityCount > 0)
	{
		render_entity* entityStates = render_state.frame.memory.alloc<render_entity>(totalEntityCount);
		ui16 visibleCount = 0;

		// SETTLEMENTS
		for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
		{
			vec2<float> settlementLoc = settlements.locations[settlementIndex];
			// Settlements are visible if any of their territory is (the frontend resolves tile owners through them).
			if (!settlements.guids[settlementIndex].is_valid()
				|| !bounds_touch_viewport(settlements.territories[settlementIndex].bounds_min, settlements.territories[settlementIndex].bounds_max, render_state.viewport)) continue;

			render_entity& outEntity = entityStates[visibleCount];
			outEntity.entity_type = ENTITY_TYPE::SETTLEMENT;
			outEntity.owner = settlements.owners[settlementIndex];
			outEntity.guid = settlements.guids[settlementIndex];
			outEntity.viewport_location = settlementLoc - render_state.viewport.bottom_left;
			outEntity.flags = 0;
			outEntity.fractional_offset = {};

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
			outEntity.fractional_offset = armies.movements[armyIndex].fractional_loc;
			outEntity.flags = 0;

			visibleCount++;
		}

		// WORKERS
		for (ui16 workerIndex = 0; workerIndex < workers.max_count; workerIndex++)
		{
            // Active workers who are outside their origin entity.
			if (!workers.guids[workerIndex].is_valid()
                    || world_entity_get_location(world, workers.origin_entity[workerIndex]) == workers.locations[workerIndex]) continue;

			vec2<float> workerLoc = workers.locations[workerIndex];
			if (!is_visible_in_viewport(workerLoc, render_state.viewport)) continue;

			render_entity& outEntity = entityStates[visibleCount];
			outEntity.entity_type = ENTITY_TYPE::WORKER;
			outEntity.owner = world_entity_get_owner(world, workers.guids[workerIndex]);
			outEntity.guid = workers.guids[workerIndex];
			outEntity.viewport_location = workerLoc - render_state.viewport.bottom_left;
			outEntity.fractional_offset = workers.movements[workerIndex].fractional_loc;
			outEntity.flags = 0;

			visibleCount++;
		}

		// BATTLES
		for (ui16 battleIndex = 0; battleIndex < battles.max_count; battleIndex++)
		{
			vec2<float> battleLoc = battles.states[battleIndex].location;
			if (!battles.guids[battleIndex].is_valid() || !is_visible_in_viewport(battleLoc, render_state.viewport)) continue;

			render_entity& outEntity = entityStates[visibleCount];
			outEntity.entity_type = ENTITY_TYPE::BATTLE;
			outEntity.owner = INVALID_MATCH_PLAYER_ID;
			outEntity.guid = battles.guids[battleIndex];
			outEntity.viewport_location = battleLoc - render_state.viewport.bottom_left;
			// Bit 0: siege. Bit 1: idle (besieging phase). Mirrored by BATTLE_FLAG_* in backend.ts.
			const world_entity_battles::state& battleState = battles.states[battleIndex];
			outEntity.flags = (battleState.kind == world_entity_battles::KIND_SIEGE ? 1 : 0) | (battleState.phase == world_entity_battles::PHASE_BESIEGING ? 2 : 0);
			outEntity.fractional_offset = {};

			visibleCount++;
		}

		// CARAVANS
		for (ui16 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
		{
			vec2<float> caravanLoc = caravans.locations[caravanIndex];
			if (!caravans.guids[caravanIndex].is_valid() || !is_visible_in_viewport(caravanLoc, render_state.viewport)) continue;

			// Caravans have no owner of their own: they are tinted as their origin settlement, if it still exists.
			render_entity& outEntity = entityStates[visibleCount];
			outEntity.entity_type = ENTITY_TYPE::CARAVAN;
			outEntity.owner = world_entity_get_owner(world, caravans.guids[caravanIndex]);
			outEntity.guid = caravans.guids[caravanIndex];
			outEntity.viewport_location = caravanLoc - render_state.viewport.bottom_left;
			outEntity.fractional_offset = caravans.movements[caravanIndex].fractional_loc;
			outEntity.flags = (ui8)caravans.flags[caravanIndex];

			visibleCount++;
		}

		render_state.frame.entity_count = visibleCount;
		render_state.frame.entity_states = entityStates;
	}
}

void game_client_render_influence(client_render_state& render_state, const game_match& match)
{
    ASSERT(match.world != nullptr);

    const world_terrain& terrain = *match.world->terrain;
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

                    ui16 ownerSettlement = terrain.regions[regionIndex].owners[tileIndex];
                    bool isOwned = ownerSettlement != world_terrain::UNOWNED_TILE;

                    render_state.tiles.owner_settlement_tiles_bitmap[worldTileIndex] = ownerSettlement;

                    bool isBorder = terrain.regions[regionIndex].status[tileIndex].border_count > 0 && isOwned;
                    bool isRoad = terrain.regions[regionIndex].status[tileIndex].is_road;

                    render_state.tiles.tile_flags_bitmap[worldTileIndex] = (ui8)RENDER_TILE_FLAGS::IS_BORDER * isBorder
                        | (ui8)RENDER_TILE_FLAGS::IS_ROAD * isRoad
                        | (ui8)RENDER_TILE_FLAGS::IS_OWNED * isOwned;
                }
            }
        }
    }
}

void game_client_rebuild_render_state(client_render_state& render_state, const game_match& match)
{
    const match_world_state& world = *match.world;
	render_state.world_size = world.terrain->size_tiles;

    // Rebuild frame data.
    render_state.frame.memory.clear();
    game_client_render_entities(render_state, match);

    // Update tile influence bitmaps.
    game_client_render_influence(render_state, match);
}
