// Internal symbols for use by the Game Common code.

#ifndef GAME_COMMON_INTERNALS_INCLUDED
#define GAME_COMMON_INTERNALS_INCLUDED

#include "game_common/match/match.h"
#include "game_common/match/world.h"

// Spawns a new settlement with the provided start state and returns its GUID. There must be a free settlement slot.
entity_guid match_spawn_settlement(game_match& match, const world_entity_settlements::single& start_state);

// Destroys an army: frees its slot and clears its stored GUID, so the old GUID stops being valid.
// The entity must be a valid army. Other entities referencing it are NOT updated.
void match_destroy_army(game_match& match, entity_guid army);

// Spawns a new army with the provided start state and returns its GUID. There must be a free army slot.
entity_guid match_spawn_army(game_match& match, const world_entity_armies::single& start_state);

// Destroys a settlement: frees its slot and clears its stored GUID, so the old GUID stops being valid.
// The entity must be a valid settlement. Other entities referencing it are NOT updated.
void match_destroy_settlement(game_match& match, entity_guid settlement);

// Removes amount from a valid settlement's population (saturating at 0). The settlement is destroyed if it reaches 0.
void match_settlement_decrease_population(game_match& match, entity_guid settlement, ui32 amount);

// Applies all passed tick commands over the match.
// Assumes the commands have been pre-validated.
void match_command_apply_all(game_match& match, const match_tick_commands& tick_commands);

#endif // GAME_COMMON_INTERNALS_INCLUDED