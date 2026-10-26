// Public symbols of the match AI component (server side only). See src/game_common/match_ai/ for the implementation and its imprint.
// The host asks each AI player to update every tick; the AI decides on its own staggered ticks, reading the world directly (never writing it)
// and emitting commands through the validated command path.

#ifndef MATCH_AI_INCLUDED
#define MATCH_AI_INCLUDED

#include "core.h"

#include "game_common/match/match.h"
#include "game_common/match/commands.h"
#include "game_common/match/world/world_state.h"

// Opinions are tracked for the first players only; others are ignored by the AI's opinion / target logic.
constexpr ui8 AI_MAX_TRACKED_PLAYERS = 32;

// Strategic modes of an AI player. Zero (EXPAND) is the default.
enum AI_MODE : ui8
{
	AI_MODE_EXPAND, // Colonize free land, grow the economy.
	AI_MODE_BUILD_UP, // Build barracks, train and raise armies.
	AI_MODE_ATTACK, // Gather the armies and conquer the target settlement of the target player.
};

// State of one AI player. Zero is valid (EXPAND, neutral opinions, no target). Only controlled_player is set by the host.
struct ai_player_state
{
	match_player_id controlled_player;

	ui8 mode; // AI_MODE.
	ui8 expand_failures; // Consecutive site searches that found nothing.
	ui8 has_target; // 1 if target_player is set.
	match_player_id target_player;
	ui32 mode_start_day;
	ui32 decision_index; // Count of decisions taken: drives the per-layer sub-cadences.
	ui16 settlement_cursor; // Round-robin position in the owned settlements for per-settlement tactics.
	entity_guid target_settlement; // Settlement the campaign aims at.
	entity_guid rally_settlement; // Own settlement the armies gather at before attacking.

	i16 opinions[AI_MAX_TRACKED_PLAYERS]; // Per other player. Zero = neutral, negative = hostile (preferred target), positive = friendly / feared.
	ui32 rng_state; // 0 = seeded on first use.
};

// Lets the AI player act on this tick, if it is its decision tick. Reads the match's world, and pushes a validated command sequence for the AI's player
// into the next tick's commands. Call once per AI per tick, before match_tick (the world is then the result of the previous tick).
// Returns false if the commands could not be pushed (builder memory full).
bool ai_update(const game_match& match, ai_player_state& ai_state, match_tick_commands_builder& commands_builder);

#endif // MATCH_AI_INCLUDED
