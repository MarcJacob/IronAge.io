// Builder symbols shared between world simulation themes: what can be built, and the builder entity lifecycle.

#ifndef WORLD_SIM_BUILDER_INCLUDED
#define WORLD_SIM_BUILDER_INCLUDED

#include "game_common/match/world/world_entities.h"

// Spawns a new builder in the world with the provided start state and returns its GUID.
// Returns INVALID_ENTITY_GUID if the origin is not a valid entity. Asserts if its type is not the one the build type requires.
entity_guid world_spawn_worker(match_world_state& world, const world_entity_workers::single& start_state);
void world_destroy_worker(match_world_state& world, entity_guid builder);

// Allocates the workers' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_workers(match_world_state& world, mem_arena& memory, ui32 count);

#endif // WORLD_SIM_BUILDER_INCLUDED
