// War systems: armies and engagements.

#ifndef WORLD_SYSTEMS_WAR_INCLUDED
#define WORLD_SYSTEMS_WAR_INCLUDED

#include "game_common/match/world/world.h"

// Resolves an attack of an army that reached its target entity: starts or joins a siege / engagement (or takes an undefended settlement).
void world_army_attack_entity(match_world_state& world, entity_guid attacker, entity_guid target);

// Standing orders (world_system_standing_orders.cpp). The first one runs before movement and sets what each army moves towards;
// the second runs after movement and handles arrivals (an attacking army close enough to its target resolves the attack).
void world_system_army_orders(match_world_state& world);

// Daily, before the settlement economy: resets the settlements' raided totals, then raiding armies loot the settlement they raid (gold, supplies, peasants killed).
void world_system_raid_loot(match_world_state& world);

// Raising armies standing on their destination (and not in a engagement) draw men from their home settlement.
void world_system_armies_raising(match_world_state& world);

// Every tick, after movement: free armies within the join radius of a joinable engagement join it ("aggro"), unless they could leave it right away
// (world_engagement_can_be_left): joining those is up to the army's order logic.
void world_system_engagement_auto_join(match_world_state& world);

void world_system_engagements(match_world_state& world);

// Every tick, after movement (world_system_campaigns.cpp): campaigns take over the armies with a JOIN_CAMPAIGN order that reached them, follow their member armies, work towards their objective (gather, march, on objective) and end when none is close (a WAITING one stays) or the objective is accomplished.
void world_system_campaigns(match_world_state& world);

#endif // WORLD_SYSTEMS_WAR_INCLUDED
