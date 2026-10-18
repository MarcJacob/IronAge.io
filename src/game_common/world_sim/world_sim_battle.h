// Battle symbols shared between world simulation themes.

#ifndef WORLD_SIM_BATTLE_INCLUDED
#define WORLD_SIM_BATTLE_INCLUDED

#include "game_common/match/world.h"

// Makes an army fight in a battle on the given side: its men are added to the side and it stops its orders (the movement system walks it to the battle).
void battle_join(match_world_state& world, entity_guid battle, entity_guid army, ui8 side);

#endif // WORLD_SIM_BATTLE_INCLUDED
