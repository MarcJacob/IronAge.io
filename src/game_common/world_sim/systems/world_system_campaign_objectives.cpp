// Campaign objectives: spawning and despawning of the objectives on the board (each type has its own rules and period), their scoring, the ranking campaigns use
// to choose their decision, and the logic of each type. Integer math only, everything in slot order. The entity is in world_entity_campaign_objectives.h.
// Adding a type: an enum value, its spawn / despawn functions called from world_system_campaign_objectives, a row in CAMPAIGN_OBJECTIVE_TYPE_DEFS (range, score, logic).
// A campaign picks the best score among WAIT and every objective that is valid for it (within its scan radius); the trip to an objective is scored by path cost (one path search per candidate, hence the radius filter first).

#include "game_common/match/game_parameters.h"
#include "game_common/match/world/entities/world_entity_armies.h"
#include "game_common/match/world/entities/world_entity_campaign_objectives.h"
#include "game_common/match/world/entities/world_entity_campaigns.h"
#include "game_common/match/world/entities/world_entity_common.h"
#include "game_common/match/world/world_state.h"

#include "systems_war.h"
#include "../world_sim.h"

#include "../entities/world_sim_entity_army.h"
#include "../entities/world_sim_entity_campaign_objective.h"
#include "../entities/world_sim_entity_engagement.h"

constexpr i32 CAMPAIGN_SCORE_INVALID = -2147483647 - 1; // The campaign can never take the objective.
constexpr ui32 CAMPAIGN_SUPPLY_DAYS_X100_MAX = 10000; // Cap of the supply days (hundredths): no consumption, or a lot of supplies.

// What a score needs to know about the campaign, computed once per ranking.
struct campaign_score_context
{
	match_player_id owner;
	world_location location;
	ui32 troops;
	ui32 supply_days_x100; // Supplies of all the member armies together / their daily consumption, in hundredths of days.
	ui32 supply_capacity_days_x100; // The same with their carry capacity instead of their supplies: the most waiting can ever bring. Supply thresholds are never above it.
	ui32 joiners; // Armies still on their way to join.

	entity_guid current_objective;

	bool has_goal; // The campaign was assigned a goal (campaign_goal) with a place: what travelling objectives help progress towards.
	world_location goal_location;
};

struct campaign_objective_type_def
{
	const char* name;
	ui32 range_tiles; // Within this many tiles of its location, the campaign runs the logic instead of travelling.
	// Not exact: an upper bound of the exact score (no path searches, and INVALID whenever the exact one is). Exact: the real score. Not const: path requests use the world's path store.
	i32 (*score)(match_world_state& world, const campaign_score_context& ctx, ui16 objective_index, bool exact);
	void (*logic)(match_world_state& world, entity_guid campaign, entity_guid objective);
};

// HELPERS

static ui32 campaign_ai_tile_distance(world_location a, world_location b)
{
	return ia_sqrt_i(vec2_dist_squared_i<ui64>(a, b));
}

static bool campaign_ai_period_elapsed(const match_world_state& world, ui32 period)
{
	return period <= 1 || world.age % period == 0;
}

// Penalty of a trip of the given path cost: per day of the trip, and more per day the supplies do not cover. The path cost is converted to time:
// a tile step takes 1000 * terrain cost / speed ticks, and the path cost is the terrain cost * TRAVEL_DIST_WEIGHT_ORTHOGONAL per tile. Never decreases with the cost.
// Returns false (not an option) when the trip is longer than the armies' supplies could ever last, even full (carry capacity): they would never have enough.
static bool campaign_ai_trip_penalty_from_cost(const campaign_score_context& ctx, ui32 path_cost, i64& out_penalty)
{
	const campaign_ai_parameters& params = GAME_PARAMETERS.campaign_ai;

	ui64 speed = GAME_PARAMETERS.army_travel_speed > 0 ? GAME_PARAMETERS.army_travel_speed : 1;
	ui64 ticksPerDay = GAME_PARAMETERS.ticks_per_day > 0 ? GAME_PARAMETERS.ticks_per_day : 1;
	ui64 ticks = (ui64)path_cost * (1000 / TRAVEL_DIST_WEIGHT_ORTHOGONAL) / speed;
	ui32 tripDays = (ui32)ia_min(ticks * 100 / ticksPerDay, (ui64)CAMPAIGN_SUPPLY_DAYS_X100_MAX);

	if (tripDays > ctx.supply_capacity_days_x100) return false;

	out_penalty = (i64)tripDays * params.time_penalty_per_day / 100;
	if (tripDays > ctx.supply_days_x100) out_penalty += (i64)(tripDays - ctx.supply_days_x100) * params.low_supply_penalty_per_day / 100;
	return true;
}

// Penalty of walking from the campaign to the place. Exact: the path armies would take (same crossable terrain as the movement system), one path search; returns false if there is
// no complete path, or if the trip is longer than the supplies could ever last. Not exact: a lower bound of it, from the chamfer distance at the cheapest tile cost (1, a road) which
// no path can beat: no search, and it only fails when the exact one would too.
static bool campaign_ai_trip_penalty(match_world_state& world, const campaign_score_context& ctx, world_location to, bool exact, i64& out_penalty)
{
	if (!exact)
	{
		ui32 dx = ia_abs((i32)to.x - (i32)ctx.location.x);
		ui32 dy = ia_abs((i32)to.y - (i32)ctx.location.y);
		ui32 diagonal = dx < dy ? dx : dy;
		return campaign_ai_trip_penalty_from_cost(ctx, TRAVEL_DIST_WEIGHT_ORTHOGONAL * (dx + dy - 2 * diagonal) + TRAVEL_DIST_WEIGHT_DIAGONAL * diagonal, out_penalty);
	}

	path_params pathParams = {};
	pathParams.passable_terrain = (ui8)(PATH_TERRAIN_LAND | (GAME_PARAMETERS.armies_workers_cross_water != 0 ? PATH_TERRAIN_WATER : 0));
	pathParams.straight_segments = true;

	path_result path;
	world_request_path(world, ctx.location, to, pathParams, path);
	if (path.partial || (path.count == 0 && !(ctx.location == to))) return false;

	return campaign_ai_trip_penalty_from_cost(ctx, path.cost, out_penalty);
}

static i32 campaign_ai_clamp_score(i64 score)
{
	return (i32)ia_clamped(score, (i64)(CAMPAIGN_SCORE_INVALID + 1), (i64)0x7FFFFFFF);
}

// A despawn rule common to all types: an objective nobody is committed to, and that nothing renewed for idle_checks runs in a row, is abandoned.
static bool campaign_objective_is_abandoned(world_entity_campaign_objectives& objectives, ui16 index, ui32 idle_checks)
{
	if (objectives.committed[index] > 0)
	{
		objectives.idle_intervals[index] = 0;
		return false;
	}

	if (objectives.idle_intervals[index] < 0xFFFF) objectives.idle_intervals[index]++;
	return objectives.idle_intervals[index] >= idle_checks;
}

// CONQUER_SETTLEMENT

// Whether the owner's campaigns are on the target's side. The one place that decides it: only the owner's own settlements for now (allies, vassals, overlords later).
// Evaluated whenever needed, never stored: an ownership flip changes how the objective is read at the next reconsideration.
static bool campaign_ai_is_friendly(const match_world_state& world, match_player_id owner, entity_guid target)
{
	return world_entity_is_friendly(world, target, owner);
}

static bool objective_conquer_target_is_settlement(const match_world_state& world, entity_guid target)
{
	return world_entity_is_valid(world, target) && target.get_type() == ENTITY_TYPE::SETTLEMENT;
}

// A settlement that is still hostile or neutral to the owner: something to conquer (a spawn rule). A friendly one is read as a defense objective instead.
static bool objective_conquer_target_is_conquerable(const match_world_state& world, match_player_id owner, entity_guid target)
{
	return objective_conquer_target_is_settlement(world, target) && !campaign_ai_is_friendly(world, owner, target);
}

// Recomputes the sides of every conquer objective from the campaigns pursuing it. A defender is a campaign friendly to the target, an attacker any other.
static void objective_conquer_refresh(match_world_state& world)
{
	world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;
	const world_entity_campaigns& campaigns = world.entities->campaigns;

	for (ui16 index = 0; index < objectives.max_count; index++)
	{
		if (objectives.guids[index].is_valid() && objectives.type[index] == CAMPAIGN_OBJECTIVE_TYPE::CONQUER_SETTLEMENT) objectives.data[index].conquer = {};
	}

	for (ui16 campaignIndex = 0; campaignIndex < campaigns.max_count; campaignIndex++)
	{
		entity_guid objective = campaigns.current_objective[campaignIndex];
		if (!campaigns.guids[campaignIndex].is_valid() || !world_entity_is_valid(world, objective)) continue;

		ui16 index = objective.get_index();
		if (objectives.type[index] != CAMPAIGN_OBJECTIVE_TYPE::CONQUER_SETTLEMENT || !objective_conquer_target_is_settlement(world, objectives.target[index])) continue;

		auto& data = objectives.data[index].conquer;
		if (campaign_ai_is_friendly(world, campaigns.owner[campaignIndex], objectives.target[index]))
		{
			data.defender_campaigns++;
			data.defender_troops += campaigns.troop_count[campaignIndex];
		}
		else
		{
			data.attacker_campaigns++;
			data.attacker_troops += campaigns.troop_count[campaignIndex];
		}
	}
}

// Every settlement hostile (or neutral) to a campaign within its scan radius has a conquest objective. An existing one is renewed.
static void objective_conquer_spawn(match_world_state& world)
{
	const world_entity_campaigns& campaigns = world.entities->campaigns;
	const world_entity_settlements& settlements = world.entities->settlements;
	world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;
	ui32 radius = GAME_PARAMETERS.campaign_ai.scan_radius_tiles;

	for (ui16 campaignIndex = 0; campaignIndex < campaigns.max_count; campaignIndex++)
	{
		if (!campaigns.guids[campaignIndex].is_valid()) continue;

		for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
		{
			entity_guid settlement = settlements.guids[settlementIndex];
			if (!settlement.is_valid() || campaign_ai_tile_distance(campaigns.location[campaignIndex], settlements.location[settlementIndex]) > radius) continue;
			if (!objective_conquer_target_is_conquerable(world, campaigns.owner[campaignIndex], settlement)) continue;

			entity_guid existing = world_find_campaign_objective(world, CAMPAIGN_OBJECTIVE_TYPE::CONQUER_SETTLEMENT, settlement);
			if (existing.is_valid()) objectives.idle_intervals[existing.get_index()] = 0;
			else world_spawn_campaign_objective(world, CAMPAIGN_OBJECTIVE_TYPE::CONQUER_SETTLEMENT, settlement, settlements.location[settlementIndex]); // Nothing happens if the board is full.
		}
	}
}

// Gone when its settlement is gone, or when nobody is committed to it or interested in it anymore.
static void objective_conquer_despawn(match_world_state& world)
{
	world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;

	for (ui16 index = 0; index < objectives.max_count; index++)
	{
		if (!objectives.guids[index].is_valid() || objectives.type[index] != CAMPAIGN_OBJECTIVE_TYPE::CONQUER_SETTLEMENT) continue;

		entity_guid target = objectives.target[index];
		bool targetGone = !world_entity_is_valid(world, target) || target.get_type() != ENTITY_TYPE::SETTLEMENT;
		if (targetGone || campaign_objective_is_abandoned(objectives, index, GAME_PARAMETERS.campaign_ai.conquer_despawn_idle_checks)) world_destroy_campaign_objective(world, objectives.guids[index]);
	}
}

static i32 objective_conquer_score(match_world_state& world, const campaign_score_context& ctx, ui16 objective_index, bool exact)
{
	const campaign_ai_parameters& params = GAME_PARAMETERS.campaign_ai;
	const world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;

	entity_guid target = objectives.target[objective_index];
	if (!objective_conquer_target_is_settlement(world, target)) return CAMPAIGN_SCORE_INVALID;

	// Friendly target: a defense objective, only while others (not friendly to the target) are pursuing it.
	bool defending = campaign_ai_is_friendly(world, ctx.owner, target);
	const auto& data = objectives.data[objective_index].conquer;
	if (defending && data.attacker_campaigns == 0) return CAMPAIGN_SCORE_INVALID;

	ui32 distance = campaign_ai_tile_distance(ctx.location, objectives.location[objective_index]);
	if (distance > params.scan_radius_tiles) return CAMPAIGN_SCORE_INVALID;

	i64 tripPenalty;
	if (!campaign_ai_trip_penalty(world, ctx, objectives.location[objective_index], exact, tripPenalty)) return CAMPAIGN_SCORE_INVALID;

	// Other campaigns on the same side as this one (not counting this campaign if it is on the objective).
	ui32 others = defending ? data.defender_campaigns : data.attacker_campaigns;
	if (objectives.guids[objective_index] == ctx.current_objective && others > 0) others--;

	i64 score;
	if (defending)
	{
		score = (i64)params.defend_base + (i64)data.attacker_campaigns * params.defend_per_attacker_campaign - tripPenalty;
		if (world.entities->settlements.siege[target.get_index()].is_valid()) score += params.defend_siege_relief_bonus;
	}
	else
	{
		ui64 troops = ctx.troops > 0 ? ctx.troops : 1;
		ui64 defenders = world.entities->settlements.soldiers_home(target.get_index());

		score = (i64)params.conquer_base - tripPenalty;
		score -= (i64)(defenders * 100 / troops) * params.conquer_defender_ratio_penalty;
	}

	score -= (i64)others * params.conquer_per_other_campaign_penalty;
	return campaign_ai_clamp_score(score);
}

// Friendly target (defense): every member army defends the settlement (the DEFEND_TERRITORY order stands on it, fights intruders and opposes its siege), as long as attackers are pursuing the objective.
// Otherwise, every member army attacks the settlement (the siege logic of the standing orders does the rest). Once the settlement is under an idle siege that the campaign's owner commands
// and member armies take part in, the campaign begins the assault when world_siege_assault_advisable says so (the same function as the BEGIN_ASSAULT command).
// It cannot repeat: the siege leaves SIEGE_IDLE at once, and only returns to it after an outside battle (then the rule is evaluated again, once more).
static void objective_conquer_logic(match_world_state& world, entity_guid campaign, entity_guid objective)
{
	world_entity_armies& armies = world.entities->armies;
	entity_guid target = world.entities->campaign_objectives.target[objective.get_index()];
	if (!objective_conquer_target_is_settlement(world, target)) return;

	if (campaign_ai_is_friendly(world, world.entities->campaigns.owner[campaign.get_index()], target))
	{
		if (world.entities->campaign_objectives.data[objective.get_index()].conquer.attacker_campaigns == 0) return;

		for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
		{
			if (!(armies.campaign[armyIndex] == campaign)) continue;
			if (!(armies.order[armyIndex].type == ARMY_ORDER_DEFEND_TERRITORY && armies.order[armyIndex].target == target))
				army_order_defend_territory(armies, (ui16)armyIndex, target, world.entities->settlements.location[target.get_index()]);
		}
		return;
	}

	bool besiegedByMember = false;

	for (ui32 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!(armies.campaign[armyIndex] == campaign)) continue;
		if (!(armies.order[armyIndex].type == ARMY_ORDER_ATTACK && armies.order[armyIndex].target == target)) army_order_attack(armies, (ui16)armyIndex, target);

		entity_guid siege = world.entities->settlements.siege[target.get_index()];
		if (siege.is_valid() && armies.engagement[armyIndex] == siege && armies.engagement_side[armyIndex] == ENGAGEMENT_SIDE::ATTACKER) besiegedByMember = true;
	}

	if (!besiegedByMember) return;

	entity_guid siege = world.entities->settlements.siege[target.get_index()];
	if (world.entities->engagements.attacker_commander[siege.get_index()] != world.entities->campaigns.owner[campaign.get_index()]) return;
	if (world_siege_assault_advisable(world, siege)) world_engagement_siege_begin_assault(world, siege);
}

// TRAVEL_TO_SETTLEMENT

// Every settlement within a campaign's scan radius, whoever owns it, can be travelled to. An existing objective is renewed.
static void objective_travel_spawn(match_world_state& world)
{
	const world_entity_campaigns& campaigns = world.entities->campaigns;
	const world_entity_settlements& settlements = world.entities->settlements;
	world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;
	ui32 radius = GAME_PARAMETERS.campaign_ai.scan_radius_tiles;

	for (ui16 campaignIndex = 0; campaignIndex < campaigns.max_count; campaignIndex++)
	{
		if (!campaigns.guids[campaignIndex].is_valid()) continue;

		for (ui16 settlementIndex = 0; settlementIndex < settlements.max_count; settlementIndex++)
		{
			entity_guid settlement = settlements.guids[settlementIndex];
			if (!settlement.is_valid() || campaign_ai_tile_distance(campaigns.location[campaignIndex], settlements.location[settlementIndex]) > radius) continue;

			entity_guid existing = world_find_campaign_objective(world, CAMPAIGN_OBJECTIVE_TYPE::TRAVEL_TO_SETTLEMENT, settlement);
			if (existing.is_valid()) objectives.idle_intervals[existing.get_index()] = 0;
			else world_spawn_campaign_objective(world, CAMPAIGN_OBJECTIVE_TYPE::TRAVEL_TO_SETTLEMENT, settlement, settlements.location[settlementIndex]); // Nothing happens if the board is full.
		}
	}
}

static void objective_travel_despawn(match_world_state& world)
{
	world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;

	for (ui16 index = 0; index < objectives.max_count; index++)
	{
		if (!objectives.guids[index].is_valid() || objectives.type[index] != CAMPAIGN_OBJECTIVE_TYPE::TRAVEL_TO_SETTLEMENT) continue;

		entity_guid target = objectives.target[index];
		bool targetGone = !world_entity_is_valid(world, target) || target.get_type() != ENTITY_TYPE::SETTLEMENT;
		if (targetGone || campaign_objective_is_abandoned(objectives, index, GAME_PARAMETERS.campaign_ai.travel_despawn_idle_checks)) world_destroy_campaign_objective(world, objectives.guids[index]);
	}
}

// Worth it if the settlement is closer to the campaign's goal than the campaign is (and the campaign is not already there): the more it gains, the better,
// minus the trip penalties. Not worth anything without a goal to progress towards.
static i32 objective_travel_score(match_world_state& world, const campaign_score_context& ctx, ui16 objective_index, bool exact)
{
	const campaign_ai_parameters& params = GAME_PARAMETERS.campaign_ai;
	const world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;

	entity_guid target = objectives.target[objective_index];
	if (!ctx.has_goal || !world_entity_is_valid(world, target) || target.get_type() != ENTITY_TYPE::SETTLEMENT) return CAMPAIGN_SCORE_INVALID;

	world_location location = objectives.location[objective_index];
	ui32 distance = campaign_ai_tile_distance(ctx.location, location);
	if (distance <= GAME_PARAMETERS.campaign_assault_distance_tiles || distance > params.scan_radius_tiles) return CAMPAIGN_SCORE_INVALID; // Already there / too far to consider.

	ui32 before = campaign_ai_tile_distance(ctx.location, ctx.goal_location);
	ui32 after = campaign_ai_tile_distance(location, ctx.goal_location);
	if (after >= before) return CAMPAIGN_SCORE_INVALID;

	i64 tripPenalty;
	if (!campaign_ai_trip_penalty(world, ctx, location, exact, tripPenalty)) return CAMPAIGN_SCORE_INVALID;

	return campaign_ai_clamp_score((i64)(before - after) * params.travel_progress_weight - tripPenalty);
}

// Nothing to do there: arriving is all that was asked.
static void objective_travel_logic(match_world_state& world, entity_guid campaign, entity_guid objective)
{
}

// TYPES

// One row per CAMPAIGN_OBJECTIVE_TYPE, in order. NONE is never on the board.
static const campaign_objective_type_def CAMPAIGN_OBJECTIVE_TYPE_DEFS[(ui8)CAMPAIGN_OBJECTIVE_TYPE::COUNT] =
{
	{ "NONE", 0, nullptr, nullptr },
	{ "CONQUER_SETTLEMENT", GAME_PARAMETERS.campaign_assault_distance_tiles, objective_conquer_score, objective_conquer_logic },
	{ "TRAVEL_TO_SETTLEMENT", GAME_PARAMETERS.campaign_assault_distance_tiles, objective_travel_score, objective_travel_logic },
};

static const campaign_objective_type_def* campaign_objective_get_def(const match_world_state& world, entity_guid objective)
{
	if (!world_entity_is_valid(world, objective) || objective.get_type() != ENTITY_TYPE::CAMPAIGN_OBJECTIVE) return nullptr;

	ui8 type = (ui8)world.entities->campaign_objectives.type[objective.get_index()];
	return type < (ui8)CAMPAIGN_OBJECTIVE_TYPE::COUNT && CAMPAIGN_OBJECTIVE_TYPE_DEFS[type].score != nullptr ? &CAMPAIGN_OBJECTIVE_TYPE_DEFS[type] : nullptr;
}

ui32 campaign_objective_range_tiles(const match_world_state& world, entity_guid objective)
{
	const campaign_objective_type_def* def = campaign_objective_get_def(world, objective);
	return def != nullptr ? def->range_tiles : 0;
}

void campaign_objective_run_logic(match_world_state& world, entity_guid campaign, entity_guid objective)
{
	const campaign_objective_type_def* def = campaign_objective_get_def(world, objective);
	if (def != nullptr && def->logic != nullptr) def->logic(world, campaign, objective);
}

// SYSTEM

void world_system_campaign_objectives(match_world_state& world)
{
	const campaign_ai_parameters& params = GAME_PARAMETERS.campaign_ai;

	objective_conquer_refresh(world); // Every tick: the scorers and logic read the sides.

	if (campaign_ai_period_elapsed(world, params.conquer_spawn_period_ticks)) objective_conquer_spawn(world);
	if (campaign_ai_period_elapsed(world, params.conquer_despawn_period_ticks)) objective_conquer_despawn(world);

	if (campaign_ai_period_elapsed(world, params.travel_spawn_period_ticks)) objective_travel_spawn(world);
	if (campaign_ai_period_elapsed(world, params.travel_despawn_period_ticks)) objective_travel_despawn(world);
}

// RANKING

static campaign_score_context campaign_build_score_context(const match_world_state& world, entity_guid campaign, entity_guid current_objective)
{
	const world_entity_campaigns& campaigns = world.entities->campaigns;
	const world_entity_armies& armies = world.entities->armies;
	ui16 campaignIndex = campaign.get_index();

	campaign_score_context ctx = {};
	ctx.owner = campaigns.owner[campaignIndex];
	ctx.location = campaigns.location[campaignIndex];
	ctx.troops = campaigns.troop_count[campaignIndex];
	ctx.current_objective = current_objective;

	// The goal's place: the entity it targets, if any.
	const campaign_goal& goal = campaigns.objective[campaignIndex];
	ctx.has_goal = goal.type != CAMPAIGN_GOAL_NONE && world_entity_is_valid(world, goal.target);
	if (ctx.has_goal) ctx.goal_location = world_entity_get_location(world, goal.target);

	// All the member armies together.
	ui64 supplies = 0;
	ui64 capacity = 0;
	ui64 consumption = 0;
	for (ui16 armyIndex = 0; armyIndex < armies.max_count; armyIndex++)
	{
		if (!armies.guids[armyIndex].is_valid()) continue;

		if (armies.campaign[armyIndex] == campaign)
		{
			supplies += armies.supplies[armyIndex];
			capacity += army_supply_capacity(world, armyIndex);
			consumption += army_daily_consumption(army_supply_basis(world, armyIndex));
		}
		else if (!armies.campaign[armyIndex].is_valid() && armies.order[armyIndex].type == ARMY_ORDER_JOIN_CAMPAIGN && armies.order[armyIndex].target == campaign) ctx.joiners++;
	}
	ctx.supply_days_x100 = consumption == 0 ? CAMPAIGN_SUPPLY_DAYS_X100_MAX : (ui32)ia_min(supplies * 100 / consumption, (ui64)CAMPAIGN_SUPPLY_DAYS_X100_MAX);
	ctx.supply_capacity_days_x100 = consumption == 0 ? CAMPAIGN_SUPPLY_DAYS_X100_MAX : (ui32)ia_min(capacity * 100 / consumption, (ui64)CAMPAIGN_SUPPLY_DAYS_X100_MAX);

	return ctx;
}

static i32 campaign_wait_score(const campaign_score_context& ctx)
{
	const campaign_ai_parameters& params = GAME_PARAMETERS.campaign_ai;
	ui32 comfort = ia_min(params.wait_supply_comfort_days * 100, ctx.supply_capacity_days_x100); // Never above what the armies can carry, or waiting could never be satisfied.

	i64 score = (i64)params.wait_base + (i64)ctx.joiners * params.wait_per_joiner;
	if (ctx.supply_days_x100 < comfort) score += (i64)(comfort - ctx.supply_days_x100) * params.wait_low_supply_per_day / 100;
	return campaign_ai_clamp_score(score);
}

// Score of the objective (or its upper bound, see campaign_objective_type_def), with the stickiness bonus of the current objective. INVALID if the type has no score for it.
static i32 campaign_objective_score(match_world_state& world, const campaign_score_context& ctx, ui16 index, bool exact)
{
	const campaign_objective_type_def* def = campaign_objective_get_def(world, world.entities->campaign_objectives.guids[index]);
	if (def == nullptr) return CAMPAIGN_SCORE_INVALID;

	i32 score = def->score(world, ctx, index, exact);
	if (score != CAMPAIGN_SCORE_INVALID && world.entities->campaign_objectives.guids[index] == ctx.current_objective) score = campaign_ai_clamp_score((i64)score + GAME_PARAMETERS.campaign_ai.stickiness);
	return score;
}

// Branch and bound: the objectives are visited from the best upper bound down (ties by slot), each getting its exact score (a path search), and the visit stops once the next bound
// cannot beat the best exact score so far. The result is the same as scoring every objective exactly (WAIT wins ties against objectives, objectives tie to the lowest slot).
campaign_decision world_campaign_rank_objectives(match_world_state& world, entity_guid campaign, entity_guid current_objective)
{
	const world_entity_campaign_objectives& objectives = world.entities->campaign_objectives;
	campaign_score_context ctx = campaign_build_score_context(world, campaign, current_objective);

	campaign_decision best = { INVALID_ENTITY_GUID, campaign_wait_score(ctx) };
	ui16 bestIndex = 0;

	bool first = true;
	i32 prevBound = 0;
	ui16 prevIndex = 0;

	for (;;)
	{
		// Next in (bound descending, slot ascending) order after the previous one. Bounds are cheap, so they are recomputed on each scan instead of stored.
		i32 nextBound = CAMPAIGN_SCORE_INVALID;
		i32 nextIndex = -1;
		for (ui16 index = 0; index < objectives.max_count; index++)
		{
			i32 bound = campaign_objective_score(world, ctx, index, false);
			if (bound == CAMPAIGN_SCORE_INVALID) continue;
			if (!first && (bound > prevBound || (bound == prevBound && index <= prevIndex))) continue;
			if (nextIndex < 0 || bound > nextBound)
			{
				nextBound = bound;
				nextIndex = index;
			}
		}

		if (nextIndex < 0) break;
		if (nextBound < best.score || (nextBound == best.score && !best.objective.is_valid())) break; // Nothing left that can win.

		first = false;
		prevBound = nextBound;
		prevIndex = (ui16)nextIndex;

		i32 score = campaign_objective_score(world, ctx, prevIndex, true);
		if (score == CAMPAIGN_SCORE_INVALID) continue;

		if (score > best.score || (score == best.score && best.objective.is_valid() && prevIndex < bestIndex))
		{
			best.objective = objectives.guids[prevIndex];
			best.score = score;
			bestIndex = prevIndex;
		}
	}

	return best;
}
