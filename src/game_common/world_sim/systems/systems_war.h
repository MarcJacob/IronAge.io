// War systems: armies and battles.

#ifndef WORLD_SYSTEMS_WAR_INCLUDED
#define WORLD_SYSTEMS_WAR_INCLUDED

#include "game_common/match/world/world.h"

void world_system_armies_target_reached(match_world_state& world);

// Raising armies standing on their destination (and not in a battle) draw men from their home settlement.
void world_system_armies_raising(match_world_state& world);

void world_system_battles(match_world_state& world);

#endif // WORLD_SYSTEMS_WAR_INCLUDED
