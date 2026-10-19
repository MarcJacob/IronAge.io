// Contains the world state elements of a match.

#ifndef MATCH_WORLD_INCLUDED
#define MATCH_WORLD_INCLUDED

#include "core.h"
#include "core/math/random.h"
#include "game_common/game_data_object.h"

using match_player_id = ui16; // Index / unique ID of a player in the game, a single atomic participant in the match able to own entities and emit commands.
using match_player_count = match_player_id; // Alias type for places where we need to count player.

static constexpr match_player_id INVALID_MATCH_PLAYER_ID = ~0; // TODO(Marc): Make this 0 so things have an invalid player ID by default.

using world_location = vec2<ui16>;
using world_dimensions = vec2<ui16>;

static constexpr world_location INVALID_WORLD_LOCATION = { ~0, ~0 };

// Possible types for an entity.
// A type identifies the exact set of properties an entity possesses, and some of the behaviors that apply to it.
enum ENTITY_TYPE : ui8
{
	INVALID,
	MACRO_TYPES_BEGIN,
	SETTLEMENT = MACRO_TYPES_BEGIN,
	// ...

	MICRO_TYPES_START,
	CARAVAN = MICRO_TYPES_START,
	ARMY,
	BUILDER,
	BATTLE,
	WARBAND,
	SIEGE,
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

// Types of terrain tiles.
enum class TERRAIN_TILE_TYPE : ui8
{
	LAND_PLAINS,
	LAND_HILLS,
	LAND_MOUNTAINS,

	WATER_COAST,
	WATER_SEA,
};

// Holds the state of a world's terrain / tiles.
struct world_terrain
{
	world_dimensions size_tiles; // Size of the world in tiles.
	world_dimensions size_regions; // Size of the world in regions.

	static constexpr ui8 REGION_SIZE = 128; // Square side size of a region. Total tile count per region = REGION_SIZE^2.
	static constexpr ui16 REGION_TILE_COUNT = REGION_SIZE * REGION_SIZE;

	// A Region is a square of REGION_SIZE * REGION_SIZE tiles. Each of the world's dimensions must be a multiplier of that size.
	// Tiles are stored square-by-square as to increase data locality in most cases where we'll want to apply area-based effects.
	struct region
	{
        // Geologic / static properties determined at terrain generation / authoring.
		TERRAIN_TILE_TYPE terrain_types[REGION_TILE_COUNT]; // Type of terrain of this tile. Influences all entities and mechanics.

        // Dynamic properties
        struct
        {
            ui8 border_count : 3; // How many neighbouring tiles are unowned or owned by a different settlements ? Tile is considered to be a border if above 0.
            bool is_road : 1;
            bool proximity_recalc : 1;
            // ...
        } status[REGION_TILE_COUNT]; // Various status values packed together in a byte.
        ui16 owners[REGION_TILE_COUNT]; // Tile owner if any, as a settlement slot index (see UNOWNED_TILE). Determines which settlement obtains the tile's production output.
        ui8 contestation_levels[REGION_TILE_COUNT]; // How contested the tile is between its owner settlement and neighbouring settlements. Decreases productivity.
        ui16 proximities[REGION_TILE_COUNT]; // Proximity to settlement value for claimed tiles.
    } *regions;

    static constexpr ui16 UNOWNED_TILE = ~0; // Owner value of a tile with no owner (influence 0).
};

// Returns region index & tile regional index of tile at given world location.
// Asserts if the location is invalid !
// @NOTE(Marc): Do not use this in performance-critical systems if possible. Instead, apply logic on every tile region-by-region.
static inline void world_get_tile(const world_terrain& terrain, world_location loc, ui16& out_region_index, ui16& out_tile_index)
{
    ASSERT(loc.x >= 0 && loc.x < terrain.size_tiles.x);
    ASSERT(loc.y >= 0 && loc.y < terrain.size_tiles.y);

    ui16 regionX = loc.x / world_terrain::REGION_SIZE;
    ui16 regionY = loc.y / world_terrain::REGION_SIZE;

    ui16 tileX = loc.x - regionX * world_terrain::REGION_SIZE;
    ui16 tileY = loc.y - regionY * world_terrain::REGION_SIZE;

    out_region_index = regionY * terrain.size_regions.x + regionX;
    out_tile_index = tileY * world_terrain::REGION_SIZE + tileX;
}

static inline world_location world_get_tile_location(const world_terrain& terrain, ui16 region_index, ui16 tile_index)
{
    ASSERT(region_index < (terrain.size_regions.x * terrain.size_regions.y));
    ASSERT(tile_index < world_terrain::REGION_TILE_COUNT);

    return { (region_index % terrain.size_regions.x * world_terrain::REGION_SIZE) + (tile_index % world_terrain::REGION_SIZE),
            (region_index / terrain.size_regions.x * world_terrain::REGION_SIZE) + (tile_index / world_terrain::REGION_SIZE)};
}

// Reused entity sub-components

// Identifies a path kept by the world's path store. Placeholder: always invalid until the store can keep several paths.
static constexpr ui32 INVALID_PATH_HANDLE = 0;

struct world_entity_movement
{
	ui8 travel_speed; // Speed in tenth of tile per tick.
	vec2<i8> fractional_loc; // "In-tile location" of the entity. Affects movement.
	world_location destination; // Tile the entity wants to reach. Set by orders and decisions.
	world_location move_target; // Next tile of the path the entity is stepping to. Only set by the movement system.
	ui32 path_handle; // Path the entity is following, INVALID_PATH_HANDLE if none.
};

struct world_entity_container
{
	ui16 max_count;
	entity_guid* guids; // Computed GUIDs for active entities. Set to 0 for inactive slots.
	ui16 active_count; // How many entities in the container are known to be active.
};

// Counts of the troop types a settlement can hold locally (levies are drawn from population instead).
struct settlement_troops
{
	ui16 men_at_arms;
	ui16 horsemen;
	ui16 knights;
};

// Property arrays for a world's settlements.
struct world_entity_settlements : public world_entity_container
{
	match_player_id*	owners;
	world_location*		locations;

    // Base economic factors
	ui32*				populations;
	ui32*				local_wealths;
    ui32*               land_sizes;
    ui32*               land_outputs; // Set by influence system according to terrain type, infrastructure...

    struct land_influence
    {
        world_location bounds_min, bounds_max; // Rectangle bounds of all currently claimed tiles.
        i32 pressure; // Population - land_output * 3. Positive = surplus wanting to expand.
    };
	land_influence*				influences; // Influence values used to claim / lose land.

	// Local troop pool. Ceilings are the RAW player-set values, never clamped in storage: 0xFFFF = saturated (effective ceiling = max).
	settlement_troops*	local_troops;
	settlement_troops*	troop_ceilings;

    // Infrastructure / building factors
	ui8*				tiers;

    // Intermediate values
	ui8*				trade_attraction_indices;

	// Consolidated representation of a single settlement for viewing purposes.
	struct single
	{
		match_player_id owner;
		world_location location;

		ui32 population;
		ui32 local_wealth;
        ui32 land_output;
        land_influence influence;

		settlement_troops local_troops;
		settlement_troops troop_ceilings; // Raw. Ignored by spawn (new settlements start saturated).
		// Derived from population, only filled by query_entity_state_settlement.
		settlement_troops troop_max;
		settlement_troops troop_effective_ceilings;

		// Armies raised from this settlement (see world_settlement_raised), only filled by query_entity_state_settlement.
		// Their men still count in population and local troops: raised_percent = raised_men / population.
		ui8 raised_armies;
		ui8 max_raised_armies;
		ui32 raised_men;
		ui32 raised_percent;

		entity_guid siege; // Siege of this settlement, if any. Only filled by query_entity_state_settlement.

		ui8 tier;

		ui8 trade_attaction_index;
	};
};

using world_entity_settlement = world_entity_settlements::single;

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_settlement)
	GAME_DATA_PROPERTY(owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY(population, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(local_wealth, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(land_output, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(influence_bounds_min, influence.bounds_min, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(influence_bounds_max, influence.bounds_max, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(influence_pressure, influence.pressure, GAME_DATA_PROP_TYPE::I32)
	GAME_DATA_PROPERTY_PATH(troops_men_at_arms, local_troops.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(troops_horsemen, local_troops.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(troops_knights, local_troops.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(ceiling_men_at_arms, troop_effective_ceilings.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(ceiling_horsemen, troop_effective_ceilings.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(ceiling_knights, troop_effective_ceilings.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(max_men_at_arms, troop_max.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(max_horsemen, troop_max.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(max_knights, troop_max.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY(raised_armies, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(max_raised_armies, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(raised_men, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(raised_percent, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(siege, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(tier, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(trade_attaction_index, GAME_DATA_PROP_TYPE::UI8)
GAME_DATA_OBJECT_DEF_END()

// Property arrays for a world's warbands: men raised from a settlement. A warband is always part of exactly one army,
// which moves, takes orders and fights for it: the warband itself only holds its men and its tie to its home settlement.
struct world_entity_warbands : public world_entity_container
{
	match_player_id* owners; // Always the owner of its army.

	struct comp
	{
		ui16 levies;
		ui16 men_at_arms;
		ui16 horsemen;
		ui16 knights;
	} *compositions;

	// Raising state. A warband is raised empty inside its home settlement and draws men from it until each count reaches its target.
	// Edit targets = overwrite raise_targets. Stop raising = clear raising (targets are kept).
	// The men of a warband stay counted in its home settlement's population / local troops for as long as the warband exists (they are not taken out):
	// men the warband loses are removed from the settlement (world_warband_remove_men_from_home).
	comp* raise_targets; // Wanted counts per type while raising. Kept after raising ends: remembers what the warband was raised to.
	entity_guid* home_settlements; // Settlement the warband was raised from. Its men count in it while it is valid and has the warband's owner.
	ui8* raising; // 1 while the warband is raising: its army takes orders but stays in place until all its warbands are raised. Cleared by the raising system.

	entity_guid* armies; // Army the warband is part of.

	// Consolidated representation of a single warband for viewing purposes.
	struct single
	{
		match_player_id owner;
		comp composition;
		comp raise_target;
		entity_guid home_settlement;
		ui8 raising;
		entity_guid army; // Ignored when spawning (see world_spawn_warband).
	};
};

using world_entity_warband = world_entity_warbands::single;

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_warband)
	GAME_DATA_PROPERTY(owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY_PATH(composition_levies, composition.levies, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(composition_men_at_arms, composition.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(composition_horsemen, composition.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(composition_knights, composition.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raise_target_levies, raise_target.levies, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raise_target_men_at_arms, raise_target.men_at_arms, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raise_target_horsemen, raise_target.horsemen, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY_PATH(raise_target_knights, raise_target.knights, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY(home_settlement, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(raising, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(army, GAME_DATA_PROP_TYPE::ENTITY_GUID)
GAME_DATA_OBJECT_DEF_END()

// Max warbands in one army.
static constexpr ui8 ARMY_MAX_WARBANDS = 64;

// What an army is doing, for viewing purposes (army single state). Highest priority first: in battle, besieging, raising, merging, following, attacking, moving, idle.
enum ARMY_STATE : ui8
{
	ARMY_STATE_IDLE,
	ARMY_STATE_MOVING,
	ARMY_STATE_ATTACKING,
	ARMY_STATE_FOLLOWING, // Accompanies an own army: keeps close to it and joins its battles.
	ARMY_STATE_MERGING, // Heads for an own army to merge into it.
	ARMY_STATE_RAISING, // One of its warbands is raising: waits in place.
	ARMY_STATE_IN_BATTLE,
	ARMY_STATE_BESIEGING, // In a siege: pinned at the besieged settlement.
};

// Property arrays for a world's armies: a group of warbands of one owner (at least one), all standing at the army's location.
// Movement, orders and battles are the army's; men and their home settlements are the warbands'.
struct world_entity_armies : public world_entity_container
{
	using comp = world_entity_warbands::comp;

	match_player_id* owners;
	world_location*	locations;

	world_entity_movement* movements;

	entity_guid* action_targets; // Entity this army is attacking, or own army it follows (see merging), if any.
	entity_guid* arrows_targets; // What entity this army last shot at, if any.
	ui8* merging; // With an own army as action target: 1 = merge into it once within reach, 0 = accompany it.

	entity_guid* battles; // Battle this army is fighting in, if any. An army in a battle is pinned in place.
	ui8* battle_sides; // Side (0 or 1) of the battle the army is on. Only relevant while in a battle.

	entity_guid* sieges; // Siege this army is in, if any (it is then in the siege's army list). An army in a siege is pinned in place.

	// Warbands of the army, as slot indices in the warband container, in joining order.
	struct warband_list
	{
		ui8 count;
		ui16 indices[ARMY_MAX_WARBANDS];
	} *warbands;

	// Consolidated representation of a single army for viewing purposes.
	struct single
	{
		match_player_id owner;
		world_location location;
		world_entity_movement movement;
		entity_guid action_target;
		entity_guid arrows_target;

		// Query-only below: ignored when spawning (an army starts with no warbands, see world_spawn_warband).
		ui8 merging;
		entity_guid battle; // Battle the army is in, if any.
		entity_guid siege; // Siege the army is in, if any.
		ui8 state; // ARMY_STATE.
		ui8 raising; // 1 while any of its warbands is raising.
		entity_guid followed_army; // Own army it follows (accompanies or merges into), if any.
		struct men_totals
		{
			ui32 levies;
			ui32 men_at_arms;
			ui32 horsemen;
			ui32 knights;
		} composition; // Men of all its warbands (ui32: 64 full warbands overflow a ui16).
		ui8 warband_count;
		entity_guid warbands[ARMY_MAX_WARBANDS]; // Its warbands (see the warband GDO for each one's men and home), warband_count first ones valid.
	};
};

using world_entity_army = world_entity_armies::single;

// One property per warband slot: warband_0 ... warband_63.
#define ARMY_WARBAND_PROPERTY(Slot) GAME_DATA_PROPERTY_PATH(warband_##Slot, warbands[Slot], GAME_DATA_PROP_TYPE::ENTITY_GUID)
#define ARMY_WARBAND_PROPERTIES_8(S0, S1, S2, S3, S4, S5, S6, S7) \
	ARMY_WARBAND_PROPERTY(S0) ARMY_WARBAND_PROPERTY(S1) ARMY_WARBAND_PROPERTY(S2) ARMY_WARBAND_PROPERTY(S3) \
	ARMY_WARBAND_PROPERTY(S4) ARMY_WARBAND_PROPERTY(S5) ARMY_WARBAND_PROPERTY(S6) ARMY_WARBAND_PROPERTY(S7)

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_army)
	GAME_DATA_PROPERTY(owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(movement_travel_speed, movement.travel_speed, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY_PATH(movement_fractional_loc, movement.fractional_loc, GAME_DATA_PROP_TYPE::VEC2_I8)
	GAME_DATA_PROPERTY_PATH(movement_destination, movement.destination, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(movement_move_target, movement.move_target, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(movement_path_handle, movement.path_handle, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(action_target, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(arrows_target, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(merging, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(battle, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(siege, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(state, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(raising, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY(followed_army, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY_PATH(composition_levies, composition.levies, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(composition_men_at_arms, composition.men_at_arms, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(composition_horsemen, composition.horsemen, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(composition_knights, composition.knights, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(warband_count, GAME_DATA_PROP_TYPE::UI8)
	ARMY_WARBAND_PROPERTIES_8(0, 1, 2, 3, 4, 5, 6, 7)
	ARMY_WARBAND_PROPERTIES_8(8, 9, 10, 11, 12, 13, 14, 15)
	ARMY_WARBAND_PROPERTIES_8(16, 17, 18, 19, 20, 21, 22, 23)
	ARMY_WARBAND_PROPERTIES_8(24, 25, 26, 27, 28, 29, 30, 31)
	ARMY_WARBAND_PROPERTIES_8(32, 33, 34, 35, 36, 37, 38, 39)
	ARMY_WARBAND_PROPERTIES_8(40, 41, 42, 43, 44, 45, 46, 47)
	ARMY_WARBAND_PROPERTIES_8(48, 49, 50, 51, 52, 53, 54, 55)
	ARMY_WARBAND_PROPERTIES_8(56, 57, 58, 59, 60, 61, 62, 63)
GAME_DATA_OBJECT_DEF_END()

static_assert(ARMY_MAX_WARBANDS == 64, "The army GDO lists one warband_<slot> property per slot: update it with ARMY_MAX_WARBANDS.");

#undef ARMY_WARBAND_PROPERTIES_8
#undef ARMY_WARBAND_PROPERTY

struct world_entity_caravans : public world_entity_container
{
	world_location* locations;

	world_entity_movement* movements;

	entity_guid* origin_settlements; // Origin settlement if any. If none / invalid, then the caravan must be destroyed. Ties it to its owner.
	entity_guid* dest_settlements; // Destination settlement if any. If none / invalid, find a new one.

	// Consolidated representation of a single caravan for viewing purposes.
	struct single
	{
		world_location location;
		world_entity_movement movement;
		entity_guid origin_settlement;
		entity_guid dest_settlement;
	};
};

using world_entity_caravan = world_entity_caravans::single;

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_caravan)
	GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(movement_travel_speed, movement.travel_speed, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY_PATH(movement_fractional_loc, movement.fractional_loc, GAME_DATA_PROP_TYPE::VEC2_I8)
	GAME_DATA_PROPERTY_PATH(movement_destination, movement.destination, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(movement_move_target, movement.move_target, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(movement_path_handle, movement.path_handle, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(origin_settlement, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(dest_settlement, GAME_DATA_PROP_TYPE::ENTITY_GUID)
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
		ui32 duration; // Ticks since the battle started.
		entity_guid siege_settlement; // Settlement besieged by side 0, INVALID if this isn't a siege. Side 0 winning takes it.
		match_player_id siege_owner; // Owner of the army that started the siege: new owner of the settlement if side 0 wins.
		entity_guid siege_defenders; // Levy army raised by the besieged settlement. Disbanded back into it if side 1 wins.
		ui8 defense_bonus; // 1 = side 1 defends with doubled defense (siege assault): side 0's damage is halved.
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
	GAME_DATA_PROPERTY(defense_bonus, GAME_DATA_PROP_TYPE::UI8)
GAME_DATA_OBJECT_DEF_END()

// Max armies besieging one settlement.
static constexpr ui8 SIEGE_MAX_ARMIES = 16;

// A siege of a settlement: started by an army attacking an enemy settlement, joined by the next armies attacking it (one siege per settlement).
// Besieging armies are pinned until they leave it, and the settlement loses population and local troops every growth tick.
// The commander can assault, the defender can sally out: both are a battle of the besiegers against the settlement's defenders (see world_spawn_settlement_defenders).
// Ends with that battle, when the settlement is gone or changes hands, when the commander lifts it, or when no army is left in it.
struct world_entity_sieges : public world_entity_container
{
	struct state
	{
		world_location location; // The settlement's.
		entity_guid settlement;
		match_player_id commander; // Owner of the army that started the siege: orders assaults and lifting, takes the settlement if its battle is won.
		match_player_id defender; // Owner of the settlement (the siege ends if it changes).
		entity_guid battle; // Assault or sally in progress, if any.
		ui8 army_count;
		entity_guid armies[SIEGE_MAX_ARMIES]; // Besieging armies in joining order, army_count first ones used.
	} *states;

	// Consolidated representation of a single siege for viewing purposes.
	struct single
	{
		state siege;
		// Query-only: men per unit type (army comp order) of the besiegers, and of the defenders the settlement would field right now.
		ui32 besieger_men[world_entity_battles::UNIT_TYPES];
		ui32 defender_men[world_entity_battles::UNIT_TYPES];
	};
};

using world_entity_siege = world_entity_sieges::single;

// One property per army slot: army_0 ... army_15.
#define SIEGE_ARMY_PROPERTY(Slot) GAME_DATA_PROPERTY_PATH(army_##Slot, siege.armies[Slot], GAME_DATA_PROP_TYPE::ENTITY_GUID)

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_siege)
	GAME_DATA_PROPERTY_PATH(location, siege.location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
	GAME_DATA_PROPERTY_PATH(settlement, siege.settlement, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY_PATH(commander, siege.commander, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY_PATH(defender, siege.defender, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY_PATH(battle, siege.battle, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY_PATH(army_count, siege.army_count, GAME_DATA_PROP_TYPE::UI8)
	GAME_DATA_PROPERTY_PATH(besieger_levies, besieger_men[0], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(besieger_men_at_arms, besieger_men[1], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(besieger_horsemen, besieger_men[2], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(besieger_knights, besieger_men[3], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(defender_levies, defender_men[0], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(defender_men_at_arms, defender_men[1], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(defender_horsemen, defender_men[2], GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY_PATH(defender_knights, defender_men[3], GAME_DATA_PROP_TYPE::UI32)
	SIEGE_ARMY_PROPERTY(0) SIEGE_ARMY_PROPERTY(1) SIEGE_ARMY_PROPERTY(2) SIEGE_ARMY_PROPERTY(3)
	SIEGE_ARMY_PROPERTY(4) SIEGE_ARMY_PROPERTY(5) SIEGE_ARMY_PROPERTY(6) SIEGE_ARMY_PROPERTY(7)
	SIEGE_ARMY_PROPERTY(8) SIEGE_ARMY_PROPERTY(9) SIEGE_ARMY_PROPERTY(10) SIEGE_ARMY_PROPERTY(11)
	SIEGE_ARMY_PROPERTY(12) SIEGE_ARMY_PROPERTY(13) SIEGE_ARMY_PROPERTY(14) SIEGE_ARMY_PROPERTY(15)
GAME_DATA_OBJECT_DEF_END()

static_assert(SIEGE_MAX_ARMIES == 16, "The siege GDO lists one army_<slot> property per slot: update it with SIEGE_MAX_ARMIES.");

#undef SIEGE_ARMY_PROPERTY

// @TEMP(Marc): Static memory size / layout properties.
constexpr ui32 SETTLEMENTS_PER_REGION = 10;
constexpr ui32 MICRO_ENTITIES_PER_TYPE_PER_REGION = 32;

struct world_path_store; // Opaque outside the world simulation, see world_sim.h.

// Contains the match's world state, namely its active entities, and static elements such as tiles.
// Overall this represents the "observable state" of the match that can be interacted with indirectly by sending input commands into the next tick.
struct match_world_state
{
    ui64 age; // Age of the world in ticks.

    rand_generator_32 rand_gen; // Main random generator used by this world's systems.

	world_terrain terrain; // World Terrain data.

	world_path_store* path_store; // Paths calculated for entity movement.

	// Holds all entity data for this world state.
	// Currently is all pre-allocated for pre-estimated max possible usage.
	// TODO(Marc): We could have a single big "entities" arena (or maybe a macro vs micro one) that grows as needed block by block.
	// We don't need ALL entities of a given type to be next to one another in memory.
	struct entities_store
	{
		world_entity_settlements settlements;
		world_entity_armies armies;
		world_entity_caravans caravans;
		world_entity_builders builders;
		world_entity_battles battles;
		world_entity_warbands warbands;
		world_entity_sieges sieges;
	} entities;
};

// World query functions.

bool world_entity_is_valid(const match_world_state& world, entity_guid entity);

world_location world_get_entity_location(const match_world_state& world, entity_guid entity);

bool query_entity_state_settlement(const match_world_state& world, entity_guid entity, world_entity_settlements::single& out_state);

bool query_entity_state_caravan(const match_world_state& world, entity_guid entity, world_entity_caravans::single& out_state);

bool query_entity_state_army(const match_world_state& world, entity_guid entity, world_entity_armies::single& out_state);

bool query_entity_state_warband(const match_world_state& world, entity_guid entity, world_entity_warbands::single& out_state);

bool query_entity_state_builder(const match_world_state& world, entity_guid entity, world_entity_builders::single& out_state);

bool query_entity_state_battle(const match_world_state& world, entity_guid entity, world_entity_battles::single& out_state);

bool query_entity_state_siege(const match_world_state& world, entity_guid entity, world_entity_sieges::single& out_state);

bool world_caravan_is_owned_by(const match_world_state& world, entity_guid caravan, match_player_id player);

// Minimum distance (in tiles) between a new settlement and the closest other settlement.
constexpr ui32 SETTLEMENT_MIN_DISTANCE = 16;

// Returns whether the location is a valid place for the player to build a settlement: buildable terrain, tile unclaimed or owned by the player,
// and at least SETTLEMENT_MIN_DISTANCE tiles away from every settlement.
bool world_is_valid_settlement_location(const match_world_state& world, world_location loc, match_player_id player);

#endif // MATCH_WORLD_INCLUDED
