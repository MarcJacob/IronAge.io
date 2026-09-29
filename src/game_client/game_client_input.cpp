// Implementation for client backend input state handling.

#include "core.h"

#include "game_client.h"

static constexpr float VIEWPORT_PAN_SPEED_FRACTION = 0.5f; // Fraction of current viewport size crossed per second, at full pan input.
static constexpr float VIEWPORT_ZOOM_EASE_RATE = 3.0f; // How fast zoom_level eases toward its target, per second.

void game_client_input_set_target_loc(game_client& backend, int x, int y)
{
	backend.input.pending_target.x = x;
	backend.input.pending_target.y = y;
	backend.input.pending = true;
}

void game_client_set_viewport_input(game_client& backend, vec2<float> pan_vector, float zoom_delta, vec2f zoom_target)
{
	game_client_input_state::viewport& viewportInput = backend.input.viewport_control;

	viewportInput.movement_vec = pan_vector;
	viewportInput.zoom_level = zoom_delta;
	viewportInput.zoom_target = zoom_target;
}

void game_client_apply_viewport_input(game_client& backend, float delta_time)
{
	client_viewport_state& viewport = backend.player_viewport;
	game_client_input_state::viewport& viewportInput = backend.input.viewport_control;

	vec2<float> viewportSize = client_backend_get_viewport_size(viewport);

	viewport.bottom_left_corner.x += viewportInput.movement_vec.x * (VIEWPORT_PAN_SPEED_FRACTION * viewportSize.x) * delta_time;
	viewport.bottom_left_corner.y += viewportInput.movement_vec.y * (VIEWPORT_PAN_SPEED_FRACTION * viewportSize.y) * delta_time;

	// World point currently under the cursor, before zoom changes viewport size - held fixed across the zoom below.
	vec2<float> cursorWorldLoc = {
		viewport.bottom_left_corner.x + viewportInput.zoom_target.x * viewportSize.x,
		viewport.bottom_left_corner.y + viewportInput.zoom_target.y * viewportSize.y,
	};

	float zoomEaseAmount = ia_min(VIEWPORT_ZOOM_EASE_RATE * delta_time, 1.0f);
	viewport.zoom_level += (viewportInput.zoom_level - viewport.zoom_level) * zoomEaseAmount;
	viewport.zoom_level = ia_max(0.0f, ia_min(1.0f, viewport.zoom_level));

	vec2<float> newViewportSize = client_backend_get_viewport_size(viewport);
	viewport.bottom_left_corner.x = cursorWorldLoc.x - viewportInput.zoom_target.x * newViewportSize.x;
	viewport.bottom_left_corner.y = cursorWorldLoc.y - viewportInput.zoom_target.y * newViewportSize.y;

	// Lazy clamp: keep the corner loosely within world bound (using maximum viewport to world size).
	viewport.bottom_left_corner.x = ia_max(-viewport.viewport_size_max.x / 2, ia_min((float)viewport.world_size.x, viewport.bottom_left_corner.x));
	viewport.bottom_left_corner.y = ia_max(-viewport.viewport_size_max.y / 2, ia_min((float)viewport.world_size.y, viewport.bottom_left_corner.y));

}
