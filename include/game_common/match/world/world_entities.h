
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
	BUILDER,
	BATTLE,
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

// Population accounting of a settlement.
// population_total = peasants + all soldiers (home and raised). It is stored: whatever changes the sum (births, deaths, spawn) must update it.
struct settlement_population
{
	ui32 population_total;
	ui32 peasants; // Everyone at home who is not a soldier.
	army_troop_count home; // Soldiers at home: the local troop pool. Levies have no home pool: home.levies is always 0.
	army_troop_count raised; // Soldiers in armies raised from the settlement. Levies in the field count here.

    i32 population_change;
	i32 growth_per_mille; // Growth rate applied to the population at home, in per mille. Negative = shrinking.

	inline ui32 soldiers_home() const { return (ui32)home.levies + home.men_at_arms + home.horsemen + home.knights; }
	inline ui32 at_home() const { return peasants + soldiers_home(); }
};

// Last day's economy figures of a settlement, written by the daily economy tick every day. Zero is a valid (empty) value.
struct settlement_finances
{
	ui32 effective_output; // Land output actually worked by the peasants.
	ui32 income; // Wealth produced from the effective output, added to local wealth.
	ui32 tax_paid; // Wealth moved from local wealth to the owner's treasury.
	ui32 consumption_required; // Wealth the people at home needed.
	ui32 consumption_paid; // Wealth actually consumed.
	ui32 excess; // Wealth available after tax that was not consumed (stays as local wealth).
};

// Property arrays for a world's settlements.
struct world_entity_settlements : public world_entity_container
{
	match_player_id*	owners;
	world_location*		locations;

    // Base economic factors.
	settlement_population* populations;
    ui32*               base_land_outputs; // Peasants that can work the land at full efficiency.

	ui32*               local_wealth; // Wealth held by the settlement. Never negative.
	i8*                 tax_rates; // -100..100, % of local wealth moved to the owner's treasury each day. Negative (future investment) is not applied yet. Reset to 0 on change of owner.
	settlement_finances* finances; // Last day's figures.

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

		settlement_population population; // Input of the spawn function (population_total is computed by it) and output of the query.
        ui32 base_land_output;
        ui32 local_wealth;
        i8 tax_rate;
        settlement_finances finances;
        territory territory;

		army_troop_count troop_ceilings;
		army_troop_count troop_max;
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

	GAME_DATA_PROPERTY(base_land_output, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(local_wealth, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(tax_rate, GAME_DATA_PROP_TYPE::I8)

	GAME_DATA_PROPERTY_PATH(finance_effective_output, finances.effective_output, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(finance_income, finances.income, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(finance_tax_paid, finances.tax_paid, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(finance_consumption_required, finances.consumption_required, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(finance_consumption_paid, finances.consumption_paid, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(finance_excess, finances.excess, GAME_DATA_PROP_TYPE::UI32)

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
	GAME_DATA_PROPERTY_PATH(max_men_at_arms, troop_max.men_at_arms, GAME_DATA_PROP_TYPE::UI16)

	GAME_DATA_PROPERTY_PATH(troops_horsemen, population.home.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(ceiling_horsemen, troop_effective_ceilings.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(max_horsemen, troop_max.horsemen, GAME_DATA_PROP_TYPE::UI16)

	GAME_DATA_PROPERTY_PATH(troops_knights, population.home.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(ceiling_knights, troop_effective_ceilings.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(max_knights, troop_max.knights, GAME_DATA_PROP_TYPE::UI16)

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
GAME_DATA_OBJECT_DEF_END()

// Small byte buffer for the data a build type keeps about its project. Its layout is entirely defined by the build type's logic.
static constexpr ui8 WORLD_BUILD_ACTION_DATA_SIZE = 16;
struct world_build_action_data
{
	alignas(4) ui8 bytes[WORLD_BUILD_ACTION_DATA_SIZE]; // Aligned so build types can overlay structures on it.
};

// Builders are the entities carrying out construction projects (roads, buildings...). Each is tied to an origin entity, and destroyed when it goes away.
struct world_entity_builders : public world_entity_container
{
	entity_guid* origin_entity; // Entity the project is tied to, whose type is dictated by the build type. Locates the builder.
	ui16* build_types; // What is being built (BUILD_TYPE, see world_sim.h).
	ui32* build_times; // Ticks since the project started. How the build type uses it is up to it.
	world_build_action_data* action_data;

	// Consolidated representation of a single builder for viewing purposes.
	struct single
	{
		entity_guid origin_entity;
		ui16 build_type;
		ui32 build_time;
		world_build_action_data action_data;
	};
};

using world_entity_builder = world_entity_builders::single;

// action_data is left out: it is a byte buffer, which has no property type yet.
GAME_DATA_OBJECT_DEF_BEGIN(world_entity_builder)
	GAME_DATA_PROPERTY(origin_entity, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(build_type, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY(build_time, GAME_DATA_PROP_TYPE::UI32)
GAME_DATA_OBJECT_DEF_END()

// A battle between two sides of armies. A battle is never targeted: armies join by targeting an army (or a besieged settlement). Fixed size: it doesn't know which armies are in it, armies point to it instead.
struct world_entity_battles : public world_entity_container
{
	static constexpr ui8 UNIT_TYPES = 4; // Army unit types, in comp order: levies, men at arms, horsemen, knights.

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

// Holds all entity data for a world state.
// Currently is all pre-allocated for pre-estimated max possible usage.
// TODO(Marc): We could have a single big "entities" arena (or maybe a macro vs micro one) that grows as needed block by block.
// We don't need ALL entities of a given type to be next to one another in memory.
struct world_entities
{
    world_entity_settlements settlements;
    world_entity_armies armies;
    world_entity_builders builders;
    world_entity_battles battles;
};

// QUERY FUNCTIONS

bool world_entity_is_valid(const match_world_state& world, entity_guid entity);

world_location world_entity_get_location(const match_world_state& world, entity_guid entity);

bool world_entity_query_settlement(const match_world_state& world, entity_guid entity, world_entity_settlements::single& out_state);

bool world_entity_query_army(const match_world_state& world, entity_guid entity, world_entity_armies::single& out_state);

bool world_entity_query_builder(const match_world_state& world, entity_guid entity, world_entity_builders::single& out_state);

bool world_entity_query_battle(const match_world_state& world, entity_guid entity, world_entity_battles::single& out_state);


// Returns whether the location is a valid place for the player to build a settlement: buildable terrain, tile unclaimed or owned by the player,
// and at least SETTLEMENT_MIN_DISTANCE tiles away from every settlement.
bool world_is_valid_settlement_location(const match_world_state& world, world_location loc, match_player_id player);

#endif // WORLD_ENTITIES_INCLUDED
