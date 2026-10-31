// Engagement symbols / helpers shared between world simulation themes.

#ifndef WORLD_SIM_ENGAGEMENT_INCLUDED
#define WORLD_SIM_ENGAGEMENT_INCLUDED

#include "game_common/match/world/world_state.h"

// Armies in a engagement move towards the engagement location until they are this many tiles from it, then stay pinned.
constexpr i32 ENGAGEMENT_ARMY_DISTANCE = 2;

// Spawns a new engagement in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_engagement(match_world_state& world, const world_entity_engagements::state& start_state);
void world_destroy_engagement(match_world_state& world, entity_guid engagement);

// Ends a engagement: releases every army in it (clears their engagement link), then destroys it (a siege also clears its settlement's siege).
void world_engagement_end(match_world_state& world, entity_guid engagement);

// Recomputes the commanders of a engagement: per side, the owner with the most men commands, the incumbent keeps command on a tie.
// A siege's defender commander (its settlement's owner) is left alone. Call when an army joins or leaves, and each round.
void world_engagement_update_commanders(match_world_state& world, entity_guid engagement);

// To call after an army left a engagement (withdrew, was destroyed): ends a siege left without attackers (see below), otherwise updates the commanders.
void world_engagement_army_left(match_world_state& world, entity_guid engagement);

// Ends the engagement if it is a siege with no army left on the attacker side. Call after an army left a engagement or was destroyed. Does nothing for other engagements or invalid guids.
void world_siege_check_attackers(match_world_state& world, entity_guid engagement);

// Diplomatic status of an engagement for a player (the only place it is computed: world_entity_get_diplomatic_status and army_engagement_try_join use it).
// Each side is rated by the player's status towards its commander (friendly > neutral > hostile). The player would join the better side, and the status says which:
// FRIENDLY = the attackers are the better side (join the attackers), HOSTILE = the defenders are (join the defenders), NEUTRAL = equal sides, not joinable.
PLAYER_DIPLOMATIC_STATUS engagement_diplomatic_status(const match_world_state& world, entity_guid engagement, match_player_id player);

// Tiles around an engagement's location within which a free army joins it (see engagement_join_* in the game parameters): base + 1 per power of 2 of the troops
// involved above the threshold. Troops: both sides' men; for a siege, minus the garrison counted in the defender side (the men inside the settlement).
ui32 engagement_join_radius(const match_world_state& world, entity_guid engagement);

// Allocates the engagements' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_engagements(match_world_state& world, mem_arena& memory, ui32 count);

// Makes an army fight in a engagement on the given side: its men are added to the side and it stops its orders (the movement system walks it to the engagement).
void engagement_join(match_world_state& world, entity_guid engagement, entity_guid army, ui8 side);

#endif // WORLD_SIM_ENGAGEMENT_INCLUDED
