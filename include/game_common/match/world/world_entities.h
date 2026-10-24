
#ifndef WORLD_ENTITIES_INCLUDED
#define WORLD_ENTITIES_INCLUDED

#include "world.h"
#include "game_common/game_data_object.h"

// @TODO(Marc): Move to parameters file.
// Minimum distance (in tiles) between a new settlement and the closest other settlement.
constexpr ui32 SETTLEMENT_MIN_DISTANCE = 16;

// Possible types for an entity.
// A type identifies the exact set of properties an entity possesses, and some of the behaviors that apply to it.
enum ENTITY_TYPE : ui8
{
	INVALID,
	MACRO_TYPES_BEGIN,
	SETTLEMENT = MACRO_TYPES_BEGIN,
	// ...

	MICRO_TYPES_START,
	ARMY = MICRO_TYPES_START,
	WORKER,
	BATTLE,
    CARAVAN,
	// ...
};

/*	
	Unique identifier for an entity on the map.
	What exactly it breaks down to depends on if the entity is MACRO (relatively low in number, not often created and destroyed, low total amount over the game)
	or MICRO (more common, shorter-lived, high total amount over the game).

	An Entity is a unique-identified dynamic element on the map tied to a set of constituant properties and behaviors.
	Most entities are owned / controlled by at least one player. They can cover relatively static elements like settlements to active elements like caravans or armies.

	Note: The ID 0 is considered "empty" or "invalid".
*/
/*
	Bit layout of the single ui32:
	- MACRO: bits 0-7 type, bits 8-19 index (up to 4096 active entities per type), bits 20-31 extra.
	- MICRO: bits 0-7 type, bits 8-23 index (up to 65536 active entities per type), bits 24-31 extra.
	Extra is based on something unpredictable like time of creation.
*/
struct entity_guid
{
	ui32 _value;

	inline bool is_valid() const { return _value != 0; }

	inline ENTITY_TYPE get_type() const { return (ENTITY_TYPE)(_value & 0xFF); }
	inline ui16 get_index() const { return is_macro() ? (_value >> 8) & 0xFFF : (_value >> 8) & 0xFFFF; }
	inline ui16 get_extra() const { return is_macro() ? (_value >> 20) & 0xFFF : (_value >> 24) & 0xFF; }

	inline bool is_macro() const { return get_type() < ENTITY_TYPE::MICRO_TYPES_START; }
};
static_assert(sizeof(entity_guid) == 4, "Entity GUID structure layout broken, it must be exactly 4 bytes.");

// Invalid / non-existent entity value. Note that non-zero values may still reference an entity that is not valid, in which case that is because it doesn't exist anymore.
constexpr entity_guid INVALID_ENTITY_GUID = { 0 };

// Builds a entity GUID from its parts.
static inline entity_guid entity_guid_new(ENTITY_TYPE type, ui16 index, ui16 extra)
{
	if (type < ENTITY_TYPE::MICRO_TYPES_START)
	{
		ASSERT(index <= 0xFFFF);
		return { (ui32)type | ((ui32)(index & 0xFFF) << 8) | ((ui32)(extra & 0xFFF) << 20) };
	}
	ASSERT(index <= 0xFFF);
	return { (ui32)type | ((ui32)index << 8) | ((ui32)(extra & 0xFF) << 24) };
}

static inline bool operator==(const entity_guid& guid_a, const entity_guid& guid_b)
{
	return guid_a._value == guid_b._value; // Whole value, so _extra is compared too.
}

struct match_world_state;


// Optional (nullptr = nothing happens) type-specific behavior. Implemented in world_sim_entity_settlement.cpp.
struct settlement_building_events
{
	void (*on_added)(match_world_state& world, entity_guid settlement) = nullptr; // After a unit of the type was added to the settlement.
	void (*on_removed)(match_world_state& world, entity_guid settlement) = nullptr; // After a unit of the type was removed from the settlement.
	void (*on_daily_tick)(match_world_state& world, entity_guid settlement) = nullptr; // Once per day, for settlements having at least one unit of the type.
};


// Building type events.

void settlement_building_barracks_on_added(match_world_state& world, entity_guid settlement);
void settlement_building_barracks_on_removed(match_world_state& world, entity_guid settlement);

void settlement_building_tradepost_on_added(match_world_state& world, entity_guid settlement);
void settlement_building_tradepost_on_removed(match_world_state& world, entity_guid settlement);

void settlement_building_tradedepot_on_added(match_world_state& world, entity_guid settlement);
void settlement_building_tradedepot_on_removed(match_world_state& world, entity_guid settlement);

struct settlement_building_def
{
	static constexpr ui8 NAME_MAX = 32;
	static constexpr ui16 DESCRIPTION_MAX = 256;

	char name[NAME_MAX]; // Null terminated. Fixed size so a GDO can cover it (array of I8).
	char description[DESCRIPTION_MAX]; // Null terminated. Generic text for the UI.
	ui16 build_days; // Construction time.
	ui32 cost_per_day; // Wealth per day while under construction.
	ui8 workers; // Workers taken from the peasants while under construction.
	settlement_building_events events;
};

// SETTLEMENT BUILDING TYPES TABLE
// Settlements keep a count per row of this table.
static constexpr settlement_building_def SETTLEMENT_BUILDING_TABLE[] =
{
	{ "Barracks", "Increases Men at Arms Max and trains 1 per day.",
        10, 100, 10, { .on_added = settlement_building_barracks_on_added, .on_removed = settlement_building_barracks_on_removed } },
    { "Caravanserai", "Increases maximum amount of active caravans. Small increase to trade export and import pool sizes.", 
        10, 100, 25, { .on_added = settlement_building_tradepost_on_added, .on_removed = settlement_building_tradepost_on_removed } },
    { "Trade Depot", "Large increase to trade export and import pools.",
        20, 150, 50, { .on_added = settlement_building_tradedepot_on_added, .on_removed = settlement_building_tradedepot_on_removed } },
};

constexpr ui8 SETTLEMENT_BUILDING_TYPE_COUNT = sizeof(SETTLEMENT_BUILDING_TABLE) / sizeof(SETTLEMENT_BUILDING_TABLE[0]);

// A row of the table as a Game Data Object, for the UI. Only what the UI needs: the row's function pointers are not exposed.
GAME_DATA_OBJECT_DEF_BEGIN(settlement_building_def)
	GAME_DATA_PROPERTY_PATH_ARRAY(name, name, GAME_DATA_PROP_TYPE::I8, settlement_building_def::NAME_MAX) // Null terminated text.
	GAME_DATA_PROPERTY_PATH_ARRAY(description, description, GAME_DATA_PROP_TYPE::I8, settlement_building_def::DESCRIPTION_MAX) // Null terminated text.
	GAME_DATA_PROPERTY(build_days, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY(cost_per_day, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(workers, GAME_DATA_PROP_TYPE::UI8)
GAME_DATA_OBJECT_DEF_END();

struct world_entity_container
{
	ui16 max_count;
	entity_guid* guids; // Computed GUIDs for active entities. Set to 0 for inactive slots.
	ui16 active_count; // How many entities in the container are known to be active.
};

// Reused entity sub-components

struct world_entity_movement
{
	ui8 travel_speed; // Speed in twentieths of a tile per tick, along an axis (a tile spans 200 fractional units, each tick adds 10 per speed point). Diagonal moves split it between the axes.
	vec2<i8> fractional_loc; // "In-tile location" of the entity. Affects movement.
	world_location destination; // Tile the entity wants to reach. Set by orders and decisions.
	world_location move_target; // Next tile of the path the entity is stepping to. Only set by the movement system.
	ui32 path_handle; // Path the entity is following, INVALID_PATH_HANDLE if none.
};

struct army_troop_count
{
    ui16 levies;
    ui16 men_at_arms;
    ui16 horsemen;
    ui16 knights;
};

// Training of one troop type in a settlement: every duration_days days, up to amount peasants become soldiers of the type.
struct settlement_troop_training
{
	ui16 amount; // Soldiers per training. 0 = no training.
	ui16 duration_days; // Days between trainings. 0 is treated as 1.
	ui16 progress_days; // Days since the last training.
};

// Population accounting of a settlement.
// population_total = peasants + workers + all soldiers (home and raised). It is stored: whatever changes the sum (births, deaths, spawn) must update it.
// @NOTE: the soldiers stuff (home, raised, troop_max, troop_training) could maybe move to a settlement_military struct.
struct settlement_population
{
	ui32 population_total;
	ui32 peasants; // Free peasants: at home, not a soldier, not a worker. The only ones working the land.
	ui32 workers; // At home, assigned to construction projects (see BUILD_SETTLEMENT_BUILDING). Consume and grow like peasants, but don't work the land.
	army_troop_count home; // Soldiers at home: the local troop pool. Levies have no home pool: home.levies is always 0.
	army_troop_count raised; // Soldiers in armies raised from the settlement. Levies in the field count here.

	// Set by the settlement's buildings (see their events). Levies are neither capped nor trained: their fields are unused.
	army_troop_count troop_max; // Max of each soldier type, home + raised.
	settlement_troop_training troop_training[4]; // Indexed by troop type (TROOP_TYPE_*).

    i32 population_change;
	i32 growth_per_mille; // Growth rate applied to the population at home, in per mille. Negative = shrinking.

	inline ui32 soldiers_home() const { return (ui32)home.levies + home.men_at_arms + home.horsemen + home.knights; }
	inline ui32 soldiers_raised() const { return (ui32)raised.levies + raised.men_at_arms + raised.horsemen + raised.knights; }
	inline ui32 soldiers_total() const { return soldiers_home() + soldiers_raised(); } // Everyone who is a soldier, at home or raised.
	inline ui32 at_home() const { return peasants + workers + soldiers_home(); }
};

// Last day's economy figures of a settlement, written by the daily economy tick every day. Zero is a valid (empty) value.
// Nothing is carried over to the next day: the consumption budget is used up by the population at home.
struct settlement_economy
{
	ui32 raw_output; // Land output actually worked by the peasants (work units, before conversion to wealth).
	ui32 production; // Wealth produced from the raw output.
	ui32 worker_pay; // Wealth received from the owner's treasury for construction workers (roads, buildings) since the previous daily tick.
	ui32 soldier_pay; // Wealth received from the owner's treasury for the soldiers of the local pool since the previous daily tick.
	ui32 trade_release; // Taken from the import pool today (import_release_percent of its contents).
	ui32 trade_overflow; // Pending trade income since the previous daily tick: arrived revenue that did not fit the import pool, export refunds that did not fit the export pool.
	ui32 trade_income; // trade_release + trade_overflow.
	ui32 income; // production + worker_pay + soldier_pay + trade_income.
	ui32 tax; // Part of the income moved to the owner's treasury.
	ui32 trade_set_aside; // Part of the income moved to the export pool.
	ui32 consumption_budget; // What the population at home lives on: income - tax - trade_set_aside.
	ui32 consumption_required; // Wealth the people at home needed.
};

// External income received by a settlement between two daily ticks (adds saturate). The daily economy tick turns it into that day's income and zeroes it.
struct settlement_pending_income
{
	ui32 worker_pay; // From the owner's treasury, for construction workers.
	ui32 soldier_pay; // From the owner's treasury, for the soldiers of the local pool.
	ui32 trade; // Caravan revenue that did not fit the import pool, and export refunds that did not fit the export pool.
};

// Buildings-related data for a settlement.
struct settlement_buildings
{
    ui16 building_counts[SETTLEMENT_BUILDING_TYPE_COUNT]; // Building count for each type.
};

// Trading-related data for a settlement.
struct settlement_trading
{
    ui8 caravan_count; // Current number of active caravans originating from this settlement.
    ui8 caravan_count_max; // Maximum number of active caravans a settlement can have.
    // Caps are kept by the Caravanserai's added / removed events (constants there). Adds stop at a cap, a pool above it is kept.
    ui32 export_pool; // Wealth set aside from the daily income for trade. Caravans are funded from it. Zero by default.
    ui32 export_pool_max;
    ui32 import_pool; // Revenue of arrived caravans, released into the daily income a share at a time. Zero by default.
    ui32 import_pool_max;
};

// Property arrays for a world's settlements.
struct world_entity_settlements : public world_entity_container
{
	match_player_id*	owners;
	world_location*		locations;
    ui64*                founding_ticks;

    // Base economic factors.
	settlement_population* populations;
    ui32*               base_land_outputs; // Peasants that can work the land at full efficiency.

	settlement_pending_income* pending_incomes; // External income received since the last daily tick.
	i8*                 tax_rates; // -100..100, % of the daily income moved to the owner's treasury. Negative (future investment) is not applied yet. Reset to 0 on change of owner.
	settlement_economy* economies; // Last day's figures.

    settlement_buildings* buildings;

    settlement_trading* trade;

    struct territory
    {
        world_location bounds_min, bounds_max; // Rectangle bounds of all currently claimed tiles.
        i32 pressure; // How much this settlement desires to expand.
        ui16 tile_count; // Total number of claimed tiles.
    };

	territory* territories; // Influence values used to claim / lose land.

	// Desired counts of the local troop pool (populations[].home).
	army_troop_count*	troop_ceilings;

	// Consolidated representation of a single settlement for viewing purposes.
	struct single
	{
		match_player_id owner;
		world_location location;
        ui64 founding_tick;

		settlement_population population;
        
        settlement_pending_income pending_income;
        i8 tax_rate;
        settlement_economy economy;

        territory territory;
        ui32 base_land_output;

        settlement_buildings buildings;

        settlement_trading trade;

		army_troop_count troop_ceilings;
		army_troop_count troop_effective_ceilings;
	};
};

using world_entity_settlement = world_entity_settlements::single;

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_settlement)
	GAME_DATA_PROPERTY(owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(population, population.population_total, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(population_change, population.population_change, GAME_DATA_PROP_TYPE::I32)
	GAME_DATA_PROPERTY_PATH(population_growth_rate, population.growth_per_mille, GAME_DATA_PROP_TYPE::I32)

	GAME_DATA_PROPERTY_PATH(peasants, population.peasants, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(workers, population.workers, GAME_DATA_PROP_TYPE::UI32)

	GAME_DATA_PROPERTY(base_land_output, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(tax_rate, GAME_DATA_PROP_TYPE::I8)

    GAME_DATA_PROPERTY_PATH_ARRAY(building_counts, buildings.building_counts, GAME_DATA_PROP_TYPE::UI16, SETTLEMENT_BUILDING_TYPE_COUNT);

	GAME_DATA_PROPERTY_PATH(economy_raw_output, economy.raw_output, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_production, economy.production, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_worker_pay, economy.worker_pay, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_soldier_pay, economy.soldier_pay, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_trade_release, economy.trade_release, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_trade_overflow, economy.trade_overflow, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_trade_income, economy.trade_income, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_income, economy.income, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_tax, economy.tax, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_trade_set_aside, economy.trade_set_aside, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_consumption_budget, economy.consumption_budget, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(economy_consumption_required, economy.consumption_required, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trading_caravans_active, trade.caravan_count, GAME_DATA_PROP_TYPE::UI8)
    GAME_DATA_PROPERTY_PATH(trading_caravans_max, trade.caravan_count_max, GAME_DATA_PROP_TYPE::UI8)
    GAME_DATA_PROPERTY_PATH(trading_export_pool, trade.export_pool, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trading_export_pool_max, trade.export_pool_max, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trading_import_pool, trade.import_pool, GAME_DATA_PROP_TYPE::UI32)
    GAME_DATA_PROPERTY_PATH(trading_import_pool_max, trade.import_pool_max, GAME_DATA_PROP_TYPE::UI32)

	GAME_DATA_PROPERTY_PATH(territory_bounds_min, territory.bounds_min, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(territory_bounds_max, territory.bounds_max, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(territory_pressure, territory.pressure, GAME_DATA_PROP_TYPE::I32)
    GAME_DATA_PROPERTY_PATH(territory_size, territory.tile_count, GAME_DATA_PROP_TYPE::UI16)

	GAME_DATA_PROPERTY_PATH(raised_levies, population.raised.levies, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_men_at_arms, population.raised.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_horsemen, population.raised.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_knights, population.raised.knights, GAME_DATA_PROP_TYPE::UI16)

	GAME_DATA_PROPERTY_PATH(troops_men_at_arms,population.home.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(ceiling_men_at_arms, troop_effective_ceilings.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(max_men_at_arms, population.troop_max.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(training_men_at_arms_amount, population.troop_training[1].amount, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(training_men_at_arms_days, population.troop_training[1].duration_days, GAME_DATA_PROP_TYPE::UI16)

	GAME_DATA_PROPERTY_PATH(troops_horsemen, population.home.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(ceiling_horsemen, troop_effective_ceilings.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(max_horsemen, population.troop_max.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(training_horsemen_amount, population.troop_training[2].amount, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(training_horsemen_days, population.troop_training[2].duration_days, GAME_DATA_PROP_TYPE::UI16)

	GAME_DATA_PROPERTY_PATH(troops_knights, population.home.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(ceiling_knights, troop_effective_ceilings.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(max_knights, population.troop_max.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(training_knights_amount, population.troop_training[3].amount, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(training_knights_days, population.troop_training[3].duration_days, GAME_DATA_PROP_TYPE::UI16)

GAME_DATA_OBJECT_DEF_END()

// Property arrays for a world's armies.
struct world_entity_armies : public world_entity_container
{
	match_player_id* owners;
	world_location*	locations;

	world_entity_movement* movements;


    army_troop_count *compositions;

	entity_guid* battles; // Battle this army is fighting in, if any.
	ui8* battle_sides; // Side (0 = Attackers or 1 = Defenders) of the battle the army is on.

	army_troop_count* raised_troops; // Raise target when raising, "Original" / "Max" counts after that.
	ui8* raising; // 1 while the army is raising.

	entity_guid* home_settlements; // Settlement this army was raised from.
	entity_guid* action_targets; // What entity this army is currently engaging or following, if any.

	ui32* supplies; // Wealth the army carries. Consumed daily by its soldiers, refilled from the owner's treasury inside the owner's territory. See world_system_armies_upkeep.
	ui16* desertions; // Men who deserted at the last daily tick (reset by every daily tick).

	// Consolidated representation of a single army for viewing purposes.
	struct single
	{
		match_player_id owner;
		world_location location;
		world_entity_movement movement;

		army_troop_count composition;
		army_troop_count raised_troops;
		ui8 raising;

		entity_guid home_settlement;
		entity_guid action_target;
		entity_guid battle;
        ui8 battle_side;

		ui32 supplies;
		ui32 supplies_consumption; // Computed by the query: what the army's soldiers consume per day. Not stored.
		ui32 supplies_capacity; // Computed by the query: what the army refills its supplies up to (army_supply_capacity). Not stored.
		ui16 desertions;
	};
};

using world_entity_army = world_entity_armies::single;

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_army)
	GAME_DATA_PROPERTY(owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)

	GAME_DATA_PROPERTY_PATH(movement_travel_speed, movement.travel_speed,       GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY_PATH(movement_fractional_loc, movement.fractional_loc,   GAME_DATA_PROP_TYPE::VEC2_I8)
	GAME_DATA_PROPERTY_PATH(movement_destination, movement.destination,         GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(movement_move_target, movement.move_target,         GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(movement_path_handle, movement.path_handle,         GAME_DATA_PROP_TYPE::UI32)

	GAME_DATA_PROPERTY_PATH(composition_levies, composition.levies,             GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(composition_men_at_arms, composition.men_at_arms,   GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(composition_horsemen, composition.horsemen,         GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(composition_knights, composition.knights,           GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_levies, raised_troops.levies,                GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_men_at_arms, raised_troops.men_at_arms,      GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_horsemen, raised_troops.horsemen,            GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raised_knights, raised_troops.knights,              GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY(raising,                                                 GAME_DATA_PROP_TYPE::UI8)

	GAME_DATA_PROPERTY(home_settlement,                                         GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(action_target,                                           GAME_DATA_PROP_TYPE::ENTITY_GUID)

	GAME_DATA_PROPERTY(battle,                                                  GAME_DATA_PROP_TYPE::ENTITY_GUID)
    GAME_DATA_PROPERTY(battle_side,                                             GAME_DATA_PROP_TYPE::UI8)

	GAME_DATA_PROPERTY(supplies,                                                GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(supplies_consumption,                                    GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(supplies_capacity,                                       GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(desertions,                                              GAME_DATA_PROP_TYPE::UI16)
GAME_DATA_OBJECT_DEF_END()

// Small byte buffer for the data a work type keeps about its project. Its layout is entirely defined by the work type's logic.
static constexpr ui8 WORLD_WORKER_ACTION_DATA_SIZE = 16;
struct world_worker_action_data
{
	alignas(4) ui8 bytes[WORLD_WORKER_ACTION_DATA_SIZE]; // Aligned so work types can overlay structures on it.
};

// Every archetype of work project a worker can be assigned to.
enum class WORK_TYPE : ui16
{
    FOUND_SETTLEMENT, // Worker is on the map and goes to found a settlement at specific.
    BUILD_ROAD, // Worker is on the map and builds a road from origin to destination, tile by tile.
    BUILD_SETTLEMENT_BUILDING, // Worker is inside the settlement and builds a building / upgrade.

    WORK_TYPE_COUNT,
};

// Layout of the action data of a road project.
struct build_road_data
{
    world_location start; // Tile the road starts from.
    world_location head; // Last tile built, which is where the road continues from.
    world_location destination; // Tile the road is going towards.
    ui8 funded; // 1 if the current day is paid for. The crew only walks / builds while it is.
    ui16 ticks_left; // Ticks left to build the tile the crew is on. 0 = not started on the next tile.
};
static_assert(sizeof(build_road_data) <= WORLD_WORKER_ACTION_DATA_SIZE, "Road action data doesn't fit the worker action data buffer.");

// Layout of the action data of a settlement founding project. The worker walks to the target carrying the colonists (taken out of the origin settlement).
struct found_settlement_data
{
    world_location target; // Tile the settlement is founded at.
    ui32 colonists; // Peasants carried. Start population of the new settlement, or given back to the origin if the worker has to return.
    ui8 returning; // 0 = going to the target, 1 = going back to the origin settlement.
};
static_assert(sizeof(found_settlement_data) <= WORLD_WORKER_ACTION_DATA_SIZE, "Found settlement action data doesn't fit the worker action data buffer.");

// Layout of the action data of a settlement building project. Zero is the state of a project that hasn't started.
struct build_settlement_building_data
{
    ui8 building_type; // Row of SETTLEMENT_BUILDING_TABLE.
    ui8 held_workers; // Workers currently taken from the settlement's peasants for the project (0 or the row's workers).
    ui16 progress_days; // Days financed so far.
};
static_assert(sizeof(build_settlement_building_data) <= WORLD_WORKER_ACTION_DATA_SIZE, "Settlement building action data doesn't fit the worker action data buffer.");

// Workers are the entities carrying out work projects (roads & building construction, colonization...). 
// Each is tied to an origin entity, and destroyed when it goes away / is done.
struct world_entity_workers : public world_entity_container
{
	entity_guid* origin_entity; // Entity the project is tied to, whose type is dictated by the work type. Locates the worker.
	WORK_TYPE* work_types; // What is being built.
	ui32* work_times; // Ticks since the project started. How the work type uses it is up to it.

    ui8* work_progress_ratios; // Progress indicator in range [0, 255] freely settable by worker logic.

	world_worker_action_data* action_data;
	world_location* locations; // Where the worker is on the map. Starts at the origin entity's location; only moving work types change it.
	world_entity_movement* movements; // Movement state, used by work types that travel (like armies').

	// Consolidated representation of a single worker for viewing purposes.
	struct single
	{
		entity_guid origin_entity;
		WORK_TYPE work_type;
		ui32 work_time;

        ui8 work_progress_ratio;

		world_worker_action_data action_data;
		world_location position;
		world_entity_movement movement;
	};
};

using world_entity_worker = world_entity_workers::single;

// action_data is left out: it is a byte buffer, which has no property type yet.
GAME_DATA_OBJECT_DEF_BEGIN(world_entity_worker)
	GAME_DATA_PROPERTY(origin_entity, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(work_type, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY(work_time, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(location, position, GAME_DATA_PROP_TYPE::WORLD_LOCATION) // Named like the other entities' so generic inspection shows it.
GAME_DATA_OBJECT_DEF_END()

// A battle between two sides of armies. A battle is never targeted: armies join by targeting an army (or a besieged settlement). Fixed size: it doesn't know which armies are in it, armies point to it instead.
struct world_entity_battles : public world_entity_container
{
	static constexpr ui8 UNIT_TYPES = 4; // Army unit types, in comp order: levies, men at arms, horsemen, knights.

    // Single state structure should work since everything gets read battle-by-battle anyway.
	struct state
	{
		world_location location;
		ui32 men[2][UNIT_TYPES]; // Men still fighting, per side and unit type.
		ui32 damage[2]; // Men killed this tick BY each side.
		ui32 duration; // Rounds since the battle started (one round every BATTLE_TICK_INTERVAL ticks).
                       
		entity_guid siege_settlement; // Settlement besieged by side 0, INVALID if this isn't a siege. Side 0 winning takes it.
		match_player_id siege_owner; // Owner of the army that started the siege: new owner of the settlement if side 0 wins.
		entity_guid siege_defenders; // Levy army raised by the besieged settlement. Disbanded back into it if side 1 wins.

	} *states;

	using single = state; // Consolidated representation of a single battle for viewing purposes.
};

using world_entity_battle = world_entity_battles::single;

// Men per side and unit type are flattened: men_side<side>_<unit type>, unit types in the order of army comp.
GAME_DATA_OBJECT_DEF_BEGIN(world_entity_battle)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(men_side0_levies, men[0][0], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side0_men_at_arms, men[0][1], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side0_horsemen, men[0][2], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side0_knights, men[0][3], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side1_levies, men[1][0], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side1_men_at_arms, men[1][1], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side1_horsemen, men[1][2], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(men_side1_knights, men[1][3], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(damage_side0, damage[0], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(damage_side1, damage[1], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(duration, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(siege_settlement, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(siege_owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY(siege_defenders, GAME_DATA_PROP_TYPE::ENTITY_GUID)
GAME_DATA_OBJECT_DEF_END()

// Flags for Caravan entities.
enum class WORLD_ENTITY_CARAVAN_FLAGS : ui8
{
    IS_SHIP = 1 << 0, // "Ships" are water caravans, as we all know.
    RETURNING_HOME = 1 << 1, // Found no destination: walking back to the origin to refund its value there.
};

struct world_entity_caravans : public world_entity_container
{
    world_location* locations;
    world_entity_movement* movements;

    WORLD_ENTITY_CARAVAN_FLAGS* flags;
    entity_guid* origin_settlements;
    entity_guid* destination_settlements;

    ui16* values;

    struct single
    {
        world_location location;
        world_entity_movement movement;

        WORLD_ENTITY_CARAVAN_FLAGS flags;
        entity_guid origin_settlement;
        entity_guid destination_settlement;

        ui16 value;
    };
};

using world_entity_caravan = world_entity_caravans::single;

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_caravan)
GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
GAME_DATA_PROPERTY(flags, GAME_DATA_PROP_TYPE::UI8)

GAME_DATA_PROPERTY_PATH(movement_destination, movement.destination, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
GAME_DATA_PROPERTY_PATH(movement_move_target, movement.move_target, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
GAME_DATA_PROPERTY_PATH(movement_fractional_loc, movement.fractional_loc, GAME_DATA_PROP_TYPE::VEC2_UI8)
GAME_DATA_PROPERTY_PATH(movement_travel_speed, movement.travel_speed, GAME_DATA_PROP_TYPE::UI8)

GAME_DATA_PROPERTY(origin_settlement, GAME_DATA_PROP_TYPE::ENTITY_GUID)
GAME_DATA_PROPERTY(destination_settlement, GAME_DATA_PROP_TYPE::ENTITY_GUID)
GAME_DATA_PROPERTY(value, GAME_DATA_PROP_TYPE::UI16)
GAME_DATA_OBJECT_DEF_END();

// Holds all entity data for a world state.
// Currently is all pre-allocated for pre-estimated max possible usage.
// TODO(Marc): We could have a single big "entities" arena (or maybe a macro vs micro one) that grows as needed block by block.
// We don't need ALL entities of a given type to be next to one another in memory.
struct world_entities
{
    world_entity_settlements settlements;
    world_entity_caravans caravans;
    world_entity_armies armies;
    world_entity_workers workers;
    world_entity_battles battles;
};

// QUERY FUNCTIONS

bool world_entity_is_valid(const match_world_state& world, entity_guid entity);

world_location world_entity_get_location(const match_world_state& world, entity_guid entity);

bool world_entity_query_settlement(const match_world_state& world, entity_guid entity, world_entity_settlement& out_state);

bool world_entity_query_caravan(const match_world_state& world, entity_guid entity, world_entity_caravan& out_state);

bool world_entity_query_army(const match_world_state& world, entity_guid entity, world_entity_armies& out_state);

bool world_entity_query_worker(const match_world_state& world, entity_guid entity, world_entity_workers& out_state);

bool world_entity_query_battle(const match_world_state& world, entity_guid entity, world_entity_battles& out_state);


// Returns whether the location is a valid place for the player to build a settlement: buildable terrain, tile unclaimed or owned by the player,
// and at least SETTLEMENT_MIN_DISTANCE tiles away from every settlement.
bool world_is_valid_settlement_location(const match_world_state& world, world_location loc, match_player_id player);

#endif // WORLD_ENTITIES_INCLUDED
