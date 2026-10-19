// Builder symbols shared between world simulation themes: what can be built, and the builder entity lifecycle.

#ifndef WORLD_SIM_BUILDER_INCLUDED
#define WORLD_SIM_BUILDER_INCLUDED

#include "game_common/match/world/world_entities.h"

// Everything that can be built.
enum BUILD_TYPE : ui16
{
    BUILD_TYPE_ROAD,
    BUILD_TYPE_COUNT,
};

// Common properties of a build type. Its progression logic is selected by the BUILD_TYPE in the construction system.
struct build_type_definition
{
    ENTITY_TYPE origin_entity_type; // Type of entity a builder of this type must be tied to.
};

static const build_type_definition BUILD_TYPE_DEFINITIONS[BUILD_TYPE_COUNT] = {
    { ENTITY_TYPE::SETTLEMENT }, // BUILD_TYPE_ROAD
};

// Layout of the action data of a road project.
struct build_road_data
{
    world_location start; // Tile the road starts from.
    world_location head; // Last tile built, which is where the road continues from.
    world_location destination; // Tile the road is going towards.
};
static_assert(sizeof(build_road_data) <= WORLD_BUILD_ACTION_DATA_SIZE, "Road action data doesn't fit the builder's action data.");

// Spawns a new builder in the world with the provided start state and returns its GUID.
// Returns INVALID_ENTITY_GUID if the origin is not a valid entity. Asserts if its type is not the one the build type requires.
entity_guid world_spawn_builder(match_world_state& world, const world_entity_builders::single& start_state);
void world_destroy_builder(match_world_state& world, entity_guid builder);

// Allocates the builders' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_builders(match_world_state& world, mem_arena& memory, ui32 count);

#endif // WORLD_SIM_BUILDER_INCLUDED
