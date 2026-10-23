// Worker systems: one per work type (for now only construction).

#ifndef WORLD_SYSTEMS_WORKERS_INCLUDED
#define WORLD_SYSTEMS_WORKERS_INCLUDED

#include "game_common/match/world/world.h"

// Advances all construction projects (workers) by one tick, destroying the finished ones.
void world_system_worker_construction(match_world_state& world);

#endif // WORLD_SYSTEMS_WORKERS_INCLUDED
