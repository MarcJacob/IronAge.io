// Battle symbols / helpers shared between world simulation themes.

#ifndef WORLD_SIM_BATTLE_INCLUDED
#define WORLD_SIM_BATTLE_INCLUDED

#include "game_common/match/world/world_state.h"

// Armies in a battle move towards the battle location until they are this many tiles from it, then stay pinned.
constexpr i32 BATTLE_ARMY_DISTANCE = 2;

// Spawns a new battle in the world with the provided start state and returns its GUID.
// Returns a deterministic new ID according to match state, if there's room.
entity_guid world_spawn_battle(match_world_state& world, const world_entity_battles::state& start_state);
void world_destroy_battle(match_world_state& world, entity_guid battle);

// Allocates the battles' property arrays from the given memory. Returns false if there isn't enough.
bool world_alloc_battles(match_world_state& world, mem_arena& memory, ui32 count);

// Makes an army fight in a battle on the given side: its men are added to the side and it stops its orders (the movement system walks it to the battle).
void battle_join(match_world_state& world, entity_guid battle, entity_guid army, ui8 side);

#endif // WORLD_SIM_BATTLE_INCLUDED
