// Render-state building for the client backend.

#include "game_client_backend.h"

// Rebuilds backend.render_state from the local match's current world state.
// TODO: Double buffering system ? TODO: Viewport / camera, interpolation - see
// management/work_units/client_input_2026-09-27.md.
void client_backend_rebuild_render_state(client_backend& backend)
{
	mem_arena& renderMemory = backend.render_memory;
	renderMemory.clear();

	match_world_state& world = client_backend_get_local_match(backend).world_state;

	backend.render_state.entity_count = world.entity_count;
	backend.render_state.entity_states = renderMemory.alloc<match_world_state::entity>(world.entity_count);

	for (entity_id entityID = 0; entityID < world.entity_count; entityID++)
	{
		// Straight copy should work so long as the entity structure remains a POD structure. Which it really should.
		match_world_state::entity& entity = backend.render_state.entity_states[entityID];
		entity = world.entity_states[entityID];
	}
}
