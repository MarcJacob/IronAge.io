// Campaign objective symbols / helpers shared between world simulation themes.

#ifndef WORLD_SIM_CAMPAIGN_OBJECTIVE_INCLUDED
#define WORLD_SIM_CAMPAIGN_OBJECTIVE_INCLUDED

#include "game_common/match/world/world_state.h"

// Spawns a campaign objective of the type about the target at the location, and returns its GUID. INVALID_ENTITY_GUID if there is no free slot (nothing is created).
// Does not check for an existing objective of the same type + target: spawners call world_find_campaign_objective first.
entity_guid world_spawn_campaign_objective(match_world_state& world, CAMPAIGN_OBJECTIVE_TYPE type, entity_guid target, world_location location);

// Destroys a campaign objective. Campaigns that still point at it notice on their next decision (the GUID is no longer valid).
void world_destroy_campaign_objective(match_world_state& world, entity_guid objective);

// The campaign objective of the type about the target, INVALID_ENTITY_GUID if there is none.
entity_guid world_find_campaign_objective(const match_world_state& world, CAMPAIGN_OBJECTIVE_TYPE type, entity_guid target);

// Allocates the campaign objectives' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_campaign_objectives(match_world_state& world, mem_arena& memory, ui32 count);

#endif // WORLD_SIM_CAMPAIGN_OBJECTIVE_INCLUDED
