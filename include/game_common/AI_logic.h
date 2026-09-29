// Temp declaration file for simple AI player system.

#ifndef PLAYER_AI_INCLUDED
#define PLAYER_AI_INCLUDED

#include "core.h"
#include "game_match.h"
#include "game_commands.h"

// Main AI state structure, holding its own decision-making data and high level parameters.
struct AI_player_state
{
	match_player_id controlled_player;

	entity_id controlled_entity;
};

bool AI_player_output_commands(const game_match& match, AI_player_state& ai_state, match_tick_commands_builder& commands_builder);

#endif // PLAYER_AI_INCLUDED