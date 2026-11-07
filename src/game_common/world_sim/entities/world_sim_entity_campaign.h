// Campaign symbols / helpers shared between world simulation themes.

#ifndef WORLD_SIM_CAMPAIGN_INCLUDED
#define WORLD_SIM_CAMPAIGN_INCLUDED

#include "game_common/match/world/world_state.h"

// Spawns a new campaign owned by the player at the location (a settlement's), with no objective, and returns its GUID. ASSERTs if there is no free slot.
// It starts WAITING: it stays there until armies join it.
entity_guid world_spawn_campaign(match_world_state& world, match_player_id owner, world_location location);

// Destroys a campaign: its armies and settlements stop pointing at it.
void world_destroy_campaign(match_world_state& world, entity_guid campaign);

// Ends a campaign: its member armies are given their campaign-over order (standing armies GO_HOME, retinues STATION where they stand) and it is destroyed. Used when its objective is accomplished, or when its owner disbands it.
void world_campaign_end_and_send_home(match_world_state& world, entity_guid campaign);

// To call after an army left a campaign (was destroyed...): ends the campaign if no army is a member anymore. Does nothing for invalid guids.
void world_campaign_army_left(match_world_state& world, entity_guid campaign);

// Tiles around a campaign's location within which armies are clustered around it and joining armies get taken over: campaign_join_base_tiles + campaign_join_per_member_tiles per member army.
ui32 campaign_join_radius(const match_world_state& world, entity_guid campaign);

// Allocates the campaigns' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_campaigns(match_world_state& world, mem_arena& memory, ui32 count);

#endif // WORLD_SIM_CAMPAIGN_INCLUDED
