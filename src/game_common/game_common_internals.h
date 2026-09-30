// Internal symbols for use by the Game Common code.

#ifndef GAME_COMMON_INTERNALS_INCLUDED
#define GAME_COMMON_INTERNALS_INCLUDED

#include "game_common/match/match.h"

// Applies all passed tick commands over the match.
// Assumes the commands have been pre-validated.
void match_command_apply_all(game_match& match, const match_tick_commands& tick_commands);

#endif // GAME_COMMON_INTERNALS_INCLUDED