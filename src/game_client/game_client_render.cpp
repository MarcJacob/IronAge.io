// Render-state building for the client backend.

#include "game_client.h"
#include "game_client/game_client_backend.h"

static constexpr float ENTITY_RADIUS = 5.0f; // TODO: sync this with JS instead of assuming a fixed size for every entity.
static constexpr ui8 SETTLEMENT_RENDER_SIZE = 6; // Square size of a settlement icon in viewport tiles.

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

	render_state.entity_count = 0;
	render_state.entity_states = nullptr;

	if (settlements.active_count > 0)
	{
		render_entity* entityStates = render_memory.alloc<render_entity>(settlements.active_count);
		ui16 visibleCount = 0;

		for (ui16 settlementIndex = 0; settlementIndex < settlements.active_count; settlementIndex++)
		{
			vec2<float> settlementLoc = { (float)settlements.locations[settlementIndex].x, (float)settlements.locations[settlementIndex].y };
			if (!is_visible_in_viewport(settlementLoc, bottomLeft, viewportSize)) continue;

			render_entity& outEntity = entityStates[visibleCount];
			outEntity.entity_type = ENTITY_TYPE::SETTLEMENT;
			outEntity.owner = settlements.owners[settlementIndex];
			outEntity.viewport_location = { settlementLoc.x - bottomLeft.x, settlementLoc.y - bottomLeft.y };
			outEntity.size_viewport = SETTLEMENT_RENDER_SIZE;

			visibleCount++;
		}

		render_state.entity_count = visibleCount;
		render_state.entity_states = entityStates;
	}
}