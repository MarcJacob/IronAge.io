// War systems: armies and engagements.

#ifndef WORLD_SYSTEMS_WAR_INCLUDED
#define WORLD_SYSTEMS_WAR_INCLUDED

#include "game_common/match/world/world.h"

// Resolves an attack of an army that reached its target entity: starts or joins a siege / engagement (or takes an undefended settlement).
void world_resolve_attack(match_world_state& world, entity_guid attacker, entity_guid target);

// Standing orders (world_system_standing_orders.cpp). The first one runs before movement and sets what each army moves towards;
// the second runs after movement and handles arrivals (an attacking army close enough to its target resolves the attack).
void world_system_army_orders(match_world_state& world);
void world_system_army_orders_arrival(match_world_state& world);

// Raising armies standing on their destination (and not in a engagement) draw men from their home settlement.
void world_system_armies_raising(match_world_state& world);

void world_system_engagements(match_world_state& world);

#endif // WORLD_SYSTEMS_WAR_INCLUDED
