// Simple implementation file for an AI player logic.
// AI players, like human players, observe the match state and create desired input commands in response to it and to their internal state,
// which doesn't have to follow the same rules of determinism as the match tick code since they live outside of it.

#include "game_common/match/AI_logic.h"

// Pushes a new command sequence for the AI's controlled player containing their desired input for the next match tick.
// Returns whether the AI could push all its commands successfully.
// NOTE: The commands are NOT checked for validity !
bool AI_output_commands(const game_match& match, AI_player_state& ai_state, match_tick_commands_builder& commands_builder)
{
	// TODO(Marc): Rebuild ! Once there are some actual game mechanics...
	return true;
}