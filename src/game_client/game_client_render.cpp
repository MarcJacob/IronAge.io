// Render-state building for the client backend.

#include "game_client.h"
#include "game_client/game_client_backend.h"

static constexpr float ENTITY_RADIUS = 5.0f; // TODO: sync this with JS instead of assuming a fixed size for every entity.

// Gets the size of viewport in world units.
vec2<float> client_backend_get_viewport_size(const client_viewport_state& viewport)
{
	return {
		viewport.viewport_size_min.x + (viewport.viewport_size_max.x - viewport.viewport_size_min.x) * viewport.zoom_level,
		viewport.viewport_size_min.y + (viewport.viewport_size_max.y - viewport.viewport_size_min.y) * viewport.zoom_level,
	};
}

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

	const match_world_state& world = match.world_state;
	vec2<float> viewportSize = client_backend_get_viewport_size(viewport);
	vec2<float> bottomLeft = viewport.bottom_left_corner;

	render_state.viewport.viewport_bottom_left = { (i32)bottomLeft.x, (i32)bottomLeft.y };
	render_state.viewport.viewport_width = (ui16)viewportSize.x;
	render_state.viewport.viewport_height = (ui16)viewportSize.y;

	render_state.controlled_player_id = controlled_player_id;
	render_state.world_size = match.start_params->world_dimensions;

	render_entity* entities = render_memory.alloc<render_entity>(world.entity_count);

	ui16 visibleCount = 0;
	for (entity_id entityID = 0; entityID < world.entity_count; entityID++)
	{
		const match_world_state::entity& entity = world.entity_states[entityID];
		vec2<float> loc = { (float)entity.location.x, (float)entity.location.y };

		if (!is_visible_in_viewport(loc, bottomLeft, viewportSize)) continue;

		render_entity& renderEntity = entities[visibleCount++];
		renderEntity.viewport_x = loc.x - bottomLeft.x;
		renderEntity.viewport_y = loc.y - bottomLeft.y;
		renderEntity.target_viewport_x = (float)entity.target_location.x - bottomLeft.x;
		renderEntity.target_viewport_y = (float)entity.target_location.y - bottomLeft.y;
	}

	render_state.entity_count = visibleCount;
	render_state.entity_states = entities;
}