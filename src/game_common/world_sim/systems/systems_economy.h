// Economy systems: settlement economy, buildings, trade caravans, army supplies.

#ifndef WORLD_SYSTEMS_ECONOMY_INCLUDED
#define WORLD_SYSTEMS_ECONOMY_INCLUDED

#include "game_common/match/world/world.h"

// Daily, per settlement: output -> wealth, tax, consumption, population growth. See world_system_settlement_economy.cpp.
void world_system_economy_daily_tick(match_world_state& world);

void world_system_economy_troop_growth(match_world_state& world);

// Daily: runs the on_daily_tick event of every building type a settlement has at least one unit of.
void world_system_settlement_buildings_daily_tick(match_world_state& world);

// Daily: settlements roll for each missing caravan slot, investing part of their export pool in caravans towards a selected destination.
void world_system_caravan_spawn(match_world_state& world);

// Caravans that reached their destination settlement trade and disappear; those whose destination is gone are cancelled.
void world_system_caravan_trade(match_world_state& world);

// Daily: armies inside their owner's territory refill their supplies from the treasury (down to 0, no debt, adds to the player's expenses),
// then consume them per troop. Unfunded consumption makes troops desert back home.
void world_system_armies_upkeep(match_world_state& world);

// Daily: soldiers of the settlements' local pools get pay from the owner's treasury into the settlement's pending soldier pay (adds to the player's expenses).
void world_system_local_troops_upkeep(match_world_state& world);

#endif // WORLD_SYSTEMS_ECONOMY_INCLUDED
