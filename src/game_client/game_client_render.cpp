// Render-state building for the client backend.

#include "game_client.h"
#include "game_client/game_client_backend.h"

static constexpr float ENTITY_RADIUS = 5.0f; // TODO: sync this with JS instead of assuming a fixed size for every entity.
static constexpr ui8 SETTLEMENT_RENDER_SIZE = 12; // Square size of a settlement icon in viewport tiles.
static constexpr ui8 CARAVAN_RENDER_SIZE = 6; // Square size of a caravan icon in viewport tiles.
static constexpr ui8 ARMY_RENDER_SIZE = 6; // Square size of an army icon in viewport tiles.

// Whether a circle of ENTITY_RADIUS centered on loc overlaps [bottom_left, bottom_left + size].
static bool is_visible_in_viewport(vec2<float> loc, vec2<float> bottom_left, vec2<float> size)
{
	float closestX = ia_max(bottom_left.x, ia_min(loc.x, bottom_left.x + size.x));
	float closestY = ia_max(bottom_left.y, ia_min(loc.y, bottom_left.y + size.y));

	float dx = loc.x - closestX;
	float dy = loc.y - closestY;

	return (dx * dx + dy * dy) <= (ENTITY_RADIUS * ENTITY_RADIUS);
}

client_render_state* game_client_get_render_state(game_client& backend)
{
	return &backend.render_state;
}

void game_client_rebuild_render_state(client_render_state& render_state, mem_arena& render_memory,
	const game_match& match, const client_viewport_state& viewport, match_player_id controlled_player_id)
{
	// Clear existing render memory.
	render_memory.clear();

	const match_world_state& world = *match.world;
	vec2f viewportSize = viewport.view_rect_max - viewport.view_rect_min;
	vec2f bottomLeft = viewport.view_rect_min;

	render_state.viewport.viewport_bottom_left = { bottomLeft.x, bottomLeft.y };
	render_state.viewport.viewport_width = viewportSize.x;
	render_state.viewport.viewport_height = viewportSize.y;

	render_state.controlled_player_id = controlled_player_id;
	render_state.world_size = world.terrain.size_tiles;

	// Build render entity states.

	const world_entity_settlements& settlements = world.entities.settlements;
	const world_entity_caravans& caravans = world.entities.caravans;
	const world_entity_armies& armies = world.entities.armies;

	render_state.entity_count = 0;
	render_state.entity_states = nullptr;

	// TODO: Come up with a way to simplify this code. A lot of entities share the same properties that are relevant to the render state,
	// and share the same pattern for initializing it.

	ui32 totalEntityCount = settlements.active_count
		+ caravans.active_count
		+ armies.active_count;

	if (totalEntityCount > 0)
	{
		render_entity* entityStates = render_memory.alloc<render_entity>(totalEntityCount);
		ui16 visibleCount = 0;

		// SETTLEMENTS
		for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
		{
			vec2<float> settlementLoc = { (float)settlements.locations[settlementIndex].x, (float)settlements.locations[settlementIndex].y };
			if (!settlements.guids[settlementIndex].is_valid() || !is_visible_in_viewport(settlementLoc, bottomLeft, viewportSize)) continue;

			render_entity& outEntity = entityStates[visibleCount];
			outEntity.entity_type = ENTITY_TYPE::SETTLEMENT;
			outEntity.owner = settlements.owners[settlementIndex];
			outEntity.guid = settlements.guids[settlementIndex];
			outEntity.viewport_location = { settlementLoc.x - bottomLeft.x, settlementLoc.y - bottomLeft.y };
			outEntity.size_viewport = SETTLEMENT_RENDER_SIZE;

			visibleCount++;
		}

		// CARAVANS
		for (ui16 caravanIndex = 0; caravanIndex < caravans.max_count; caravanIndex++)
		{
			vec2<float> caravanLoc = { (float)caravans.locations[caravanIndex].x, (float)caravans.locations[caravanIndex].y };
			if (!caravans.guids[caravanIndex].is_valid() || !is_visible_in_viewport(caravanLoc, bottomLeft, viewportSize)) continue;

			render_entity& outEntity = entityStates[visibleCount];
			outEntity.entity_type = ENTITY_TYPE::CARAVAN;

			// Caravans are only "ours" (origin or destination owned by the controlled player) or not.
			match_player_id owner = match_caravan_is_owned_by(match, caravans.guids[caravanIndex], controlled_player_id)
				? controlled_player_id : INVALID_MATCH_PLAYER_ID;

			outEntity.owner = owner;
			outEntity.guid = caravans.guids[caravanIndex];
			outEntity.viewport_location = { caravanLoc.x - bottomLeft.x, caravanLoc.y - bottomLeft.y };
			outEntity.size_viewport = CARAVAN_RENDER_SIZE;

			visibleCount++;
		}

		// ARMIES
		for (ui16 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
		{
			vec2<float> armyLoc = { (float)armies.locations[armyIndex].x, (float)armies.locations[armyIndex].y };
			if (!armies.guids[armyIndex].is_valid() || !is_visible_in_viewport(armyLoc, bottomLeft, viewportSize)) continue;

			render_entity& outEntity = entityStates[visibleCount];
			outEntity.entity_type = ENTITY_TYPE::ARMY;
			outEntity.owner = armies.owners[armyIndex];
			outEntity.guid = armies.guids[armyIndex];
			outEntity.viewport_location = { armyLoc.x - bottomLeft.x, armyLoc.y - bottomLeft.y };
			outEntity.size_viewport = ARMY_RENDER_SIZE;

			visibleCount++;
		}


		render_state.entity_count = visibleCount;
		render_state.entity_states = entityStates;
	}
}
