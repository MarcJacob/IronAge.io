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

// Allocates the engagements' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_engagements(match_world_state& world, mem_arena& memory, ui32 count);

// Makes an army fight in a engagement on the given side: its men are added to the side and it stops its orders (the movement system walks it to the engagement).
void engagement_join(match_world_state& world, entity_guid engagement, entity_guid army, ui8 side);

#endif // WORLD_SIM_ENGAGEMENT_INCLUDED
