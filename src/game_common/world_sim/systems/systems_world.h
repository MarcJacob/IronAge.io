// General world systems that belong to no theme above: territory and entity movement.

#ifndef WORLD_SYSTEMS_WORLD_INCLUDED
#define WORLD_SYSTEMS_WORLD_INCLUDED

#include "game_common/match/world/world.h"

void world_system_settlement_territory_proximity_recalc(match_world_state& world);
void world_system_settlement_territory(match_world_state& world);

void world_system_settlement_territory_update_pressure(match_world_state& world);

// Movement decision AI for entities. @TODO: Too centralized. Have an equivalent decision function for each entity type.
void world_system_entity_movement_decision(match_world_state& world);

// Movement application for entities that can move.
void world_system_entity_movement(match_world_state& world);

#endif // WORLD_SYSTEMS_WORLD_INCLUDED
