// Symbols shared by the files of the match AI component (not visible outside of it). All tuning constants live here.

#ifndef MATCH_AI_INTERNAL_INCLUDED
#define MATCH_AI_INTERNAL_INCLUDED

#include "game_common/match/ai/match_ai.h"
#include "game_common/match/game_parameters.h"
#include "core/math/random.h"

// CADENCE
// An AI decides every AI_DECISION_PERIOD_TICKS ticks, on the tick given by its player index (so work per tick is about players / period decisions).
// Inside a decision, layers run every N decisions (counted by ai_player_state::decision_index).
constexpr ui32 AI_DECISION_PERIOD_TICKS = 20;
constexpr ui32 AI_STRATEGY_EVERY = 5; // Strategy: opinions, target, mode. Slow.
constexpr ui32 AI_TACTICS_EVERY = 3; // Economy and expansion tactics, on a few settlements at a time.
constexpr ui32 AI_THREAT_EVERY = 2; // Threat scan and defense.
constexpr ui32 AI_SETTLEMENTS_PER_TACTICS = 6; // Settlements handled per tactics run (round-robin).
constexpr ui32 AI_SITE_SAMPLES = 24; // Candidate tiles tested per site search.
constexpr ui32 AI_MAX_ORDERS = 12; // Army orders per decision.

// OWNED ENTITIES LOOKED AT PER DECISION (more are ignored)
constexpr ui16 AI_MAX_OWNED_SETTLEMENTS = 48;
constexpr ui16 AI_MAX_OWNED_ARMIES = 96;

// STRATEGY
constexpr ui32 AI_GOAL_SETTLEMENTS_MAX = 24; // Settlement count the expansion goal grows to.
constexpr ui32 AI_GOAL_DAYS_PER_SETTLEMENT = 4; // Goal = 3 + day / this.
constexpr ui32 AI_CAMPAIGN_MAX_DISTANCE = 250; // Tiles: players further than this from all our settlements are not targets.
constexpr ui32 AI_ATTACK_MAX_DAYS = 80; // Days after which a campaign is abandoned.
constexpr ui32 AI_BUILD_UP_MIN_DAYS = 15; // Rest between two campaigns.
constexpr i32 AI_OPINION_LIMIT = 300;

// TACTICS
constexpr ui32 AI_COLONIST_RESERVE = 50; // Peasants a settlement keeps after sending colonists.
constexpr ui32 AI_MIN_SITE_DISTANCE_ENEMY = 26; // Tiles between a new site and any foreign settlement.
constexpr ui32 AI_THREAT_RADIUS = 24; // Foreign armies this close to one of our settlements are a threat.
constexpr ui32 AI_RALLY_RADIUS = 12;
constexpr ui32 AI_MIN_ARMY_STRENGTH = 150; // Army strength the build up aims for at least.
constexpr ui32 AI_MIN_RAISE_PEASANTS = 80;
constexpr ui32 AI_RAISE_PERCENT_OF_PEASANTS = 40;
constexpr ui32 AI_MIN_BUILDING_TREASURY = 150;
constexpr ui32 AI_MIN_ROAD_TREASURY = 250;
constexpr ui32 AI_MAX_BUILD_PROJECTS = 2; // Plus one per AI_TREASURY_PER_EXTRA_PROJECT in the treasury.
constexpr ui32 AI_TREASURY_PER_EXTRA_PROJECT = 1500;
constexpr ui32 AI_RICH_TREASURY = 2000; // Above this, taxes drop to 1 (the money is not being spent anyway).
constexpr ui32 AI_TINY_ARMY = 10; // Idle armies under this are disbanded.
constexpr ui32 AI_MAX_ROAD_LENGTH = 60; // Tiles.
constexpr i8 AI_TAX_BY_MODE[] = { 3, 6, 8 }; // Indexed by AI_MODE.

// VIEWS: copies of the few properties of an entity the AI reads. Filled only by the accessors of match_ai_world.cpp.

struct ai_settlement_view
{
	entity_guid guid;
	match_player_id owner;
	world_location location;
	ui32 population, peasants, workers, local_wealth;
	i32 growth_per_mille;
	i8 tax_rate;
	army_troop_count home_troops;
	ui16 barracks, caravanserai;
	ui16 maa_ceiling; // Raw men at arms ceiling.
};

struct ai_army_view
{
	entity_guid guid;
	entity_guid home;
	match_player_id owner;
	world_location location;
	ui32 troops;
	ui32 troops_target; // Raise targets while raising, else troops.
	ui8 raising;
	ui8 idle; // Not raising, not in a battle, not chasing and not walking anywhere.
};

// Per other player figures gathered by the strategy scan.
struct ai_player_scan
{
	ui32 troops[AI_MAX_TRACKED_PLAYERS]; // Armies and home pools.
	ui16 settlements[AI_MAX_TRACKED_PLAYERS];
	ui32 nearest_dist_sq[AI_MAX_TRACKED_PLAYERS]; // To the closest of our settlements. 0xFFFFFFFF = none.
};

// Everything one decision works with.
struct ai_context
{
	const match_world_state& world;
	ai_player_state& state;
	command_sequence_builder& commands; // Unvalidated sequence of this decision. A push returning nullptr means the budget is spent.
	match_player_id player;
	ui32 day = 0;
	ui32 treasury = 0;

	ui16 settlement_count = 0;
	ai_settlement_view settlements[AI_MAX_OWNED_SETTLEMENTS] = {};
	ui16 army_count = 0;
	ai_army_view armies[AI_MAX_OWNED_ARMIES] = {};
	ui32 army_strength = 0; // Troops of the owned armies, raising ones at their target.
};

// Pushes a command into the decision's sequence with a zeroed payload (the arena memory is not). Returns nullptr when the budget is spent.
template<typename Payload>
static Payload* ai_push(ai_context& ctx, MATCH_COMMAND_TYPE type)
{
	Payload* payload = ctx.commands.push_command<Payload>(type);
	if (payload != nullptr) ia_memzero(payload, sizeof(Payload));
	return payload;
}

// WORLD ACCESS (match_ai_world.cpp): the only place reading the world.
static ui32 ai_world_player_count(const match_world_state& world);
static ui32 ai_world_day(const match_world_state& world);
static ui64 ai_world_age(const match_world_state& world);
static ui16 ai_world_settlement_slots(const match_world_state& world);
static ui16 ai_world_army_slots(const match_world_state& world);
static bool ai_world_has_room_to_found(const match_world_state& world); // Free settlement and worker slots.
static bool ai_view_settlement(const match_world_state& world, ui16 slot, ai_settlement_view& out); // False if the slot is empty.
static bool ai_view_settlement_by_guid(const match_world_state& world, entity_guid guid, ai_settlement_view& out); // False if gone.
static bool ai_view_army(const match_world_state& world, ui16 slot, ai_army_view& out);
static ui16 ai_count_projects(const match_world_state& world, WORK_TYPE type, match_player_id player, entity_guid settlement); // settlement invalid = any of the player's.
static bool ai_site_is_valid(const match_world_state& world, world_location loc, match_player_id player);
static i32 ai_site_land_score(const match_world_state& world, world_location loc); // 0 = unusable.
static bool ai_site_is_owned(const match_world_state& world, world_location loc);
static bool ai_has_road_next_to(const match_world_state& world, world_location loc);
static world_dimensions ai_world_size(const match_world_state& world);
static void ai_gather_own(ai_context& ctx);

// SMALL HELPERS
static inline ui32 ai_dist_sq(world_location a, world_location b)
{
	i32 dx = (i32)a.x - b.x, dy = (i32)a.y - b.y;
	return (ui32)(dx * dx + dy * dy);
}

static inline ui32 ai_troops_sum(const army_troop_count& troops)
{
	return (ui32)troops.levies + troops.men_at_arms + troops.horsemen + troops.knights;
}

// Troops a settlement can put against a siege: 20% of the population as levies (limited by the peasants) plus the local pool.
static inline ui32 ai_estimate_defense(const ai_settlement_view& settlement)
{
	return ia_min(settlement.population / 5, settlement.peasants) + ai_troops_sum(settlement.home_troops);
}

// Strength the campaign wants before attacking a settlement.
static inline ui32 ai_attack_need(const ai_settlement_view& target)
{
	return ai_estimate_defense(target) * 3 / 2 + 30;
}

static inline ui32 ai_random(ai_context& ctx, ui32 count) // [0, count)
{
	return count == 0 ? 0 : ia_xorshift_i32(ctx.state.rng_state) % count;
}

static inline bool ai_layer_due(const ai_context& ctx, ui32 every, ui32 offset)
{
	return ctx.state.decision_index % every == offset;
}

// LAYERS
static void ai_scan_players(const ai_context& ctx, ai_player_scan& out); // out must be zeroed.
static void ai_update_opinions(ai_context& ctx, const ai_player_scan& scan, ui32 own_troops, match_player_id threat_owner);
static void ai_strategy_update(ai_context& ctx);
static void ai_tactics_taxes(ai_context& ctx);
static void ai_tactics_buildings(ai_context& ctx);
static void ai_tactics_found_settlement(ai_context& ctx);
static void ai_tactics_roads(ai_context& ctx);
static void ai_tactics_raise_armies(ai_context& ctx);
static void ai_tactics_disband_remnants(ai_context& ctx);
static bool ai_find_threat(const ai_context& ctx, ai_army_view& out_threat); // Biggest foreign army near one of our settlements.
static void ai_tactics_defend(ai_context& ctx);
static void ai_tactics_campaign(ai_context& ctx);

#endif // MATCH_AI_INTERNAL_INCLUDED
