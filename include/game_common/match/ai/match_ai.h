// Public symbols of the match AI component (server side only). See src/game_common/match_ai/ for the implementation and its imprint.
// The host observes the world after each tick into ONE shared observed state, then asks each AI player for its commands from it.

#ifndef MATCH_AI_INCLUDED
#define MATCH_AI_INCLUDED

#include "core.h"

#include "game_common/match/match.h"
#include "game_common/match/commands.h"
#include "game_common/match/world/world.h"
#include "game_common/match/world/world_entities.h"

// Capacities of the observed state. Entities beyond them are not observed.
constexpr ui16 AI_OBSERVED_MAX_PLAYERS = 256;
constexpr ui16 AI_OBSERVED_MAX_SETTLEMENTS = 256;
constexpr ui16 AI_OBSERVED_MAX_ARMIES = 256;

struct ai_observed_settlement
{
	entity_guid guid;
	match_player_id owner;
	world_location location;
	ui32 population;
	ui32 peasants;
	ui32 workers; // Peasants busy on a construction project: non-zero = something is being built.
	ui32 local_wealth;
	i8 tax_rate;
	army_troop_count home_troops; // Pooled soldiers at home.
	settlement_buildings buildings;
};

struct ai_observed_army
{
	entity_guid guid;
	match_player_id owner;
	world_location location;
	ui32 troops; // All troop types.
	ui8 idle; // 1 if not raising, not in a battle, not chasing a target and not walking anywhere.
};

// Copy of what the AI players know about the world, rebuilt after each world tick. Full information game: shared by all AI players.
// Owns all its data (no pointers into the world). Zero = empty world.
struct ai_observed_world_state
{
	ui64 tick; // World age.
	ui64 day;

	ui16 player_count;
	ui32 treasuries[AI_OBSERVED_MAX_PLAYERS]; // Indexed by player.

	ui16 settlement_count;
	ai_observed_settlement settlements[AI_OBSERVED_MAX_SETTLEMENTS];

	ui16 army_count;
	ai_observed_army armies[AI_OBSERVED_MAX_ARMIES];
};

// State of one AI player. Zero is valid. Only the controlled player is set by the host.
struct ai_player_state
{
	match_player_id controlled_player;
};

// Rebuilds the observed state from the world. Call after each match tick.
void ai_observe_world(const match_world_state& world, ai_observed_world_state& out_observed);

// Pushes a command sequence for the AI's player into the next tick's commands, if it decides to act this tick.
// Decisions use the observed state only; the match is only used to validate the commands (invalid ones are dropped).
// Returns false if the commands could not be pushed (builder memory full).
bool ai_output_commands(const ai_observed_world_state& observed, const game_match& match, ai_player_state& ai_state,
	match_tick_commands_builder& commands_builder);

#endif // MATCH_AI_INCLUDED
