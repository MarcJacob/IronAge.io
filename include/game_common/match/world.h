// Contains the world state elements of a match.

#ifndef MATCH_WORLD_INCLUDED
#define MATCH_WORLD_INCLUDED

#include "core.h"

using world_location = vec2<ui16>;
using world_dimensions = vec2<ui16>;

static constexpr world_location INVALID_WORLD_LOCATION = { ~0, ~0 };

// Types of terrain tiles.
enum TERRAIN_TILE_TYPE : ui8
{
	LAND_PLAINS,
	LAND_HILLS,
	LAND_MOUNTAINS,

	WATER_COAST,
	WATER_SEA,
};

enum TERRAIN_TILE_FLAGS : ui8
{
	IS_ROAD = 1 << 0,
	// ...
};

// Holds information about a world's terrain and other static information.
struct world_terrain
{
	world_dimensions size_tiles; // Size of the world in tiles.
	world_dimensions size_regions; // Size of the world in regions.

	static constexpr ui8 REGION_SIZE = 128; // Square side size of a region. Total tile count per region = REGION_SIZE^2.
	static constexpr ui16 REGION_TILE_COUNT = REGION_SIZE * REGION_SIZE;

	// A Region is a square of REGION_SIZE * REGION_SIZE tiles. Each of the world's dimensions must be a multiplier of that size.
	// Tiles are stored square-by-square as to increase data locality in most cases where we'll want to apply area-based effects.
	// Holds static data about each tile in the region. Dynamic tile-related elements like settlement influences or modifiers are stored in other ways.
	struct region
	{
		TERRAIN_TILE_TYPE terrain_types[REGION_TILE_COUNT];
		TERRAIN_TILE_FLAGS flags[REGION_TILE_COUNT];
		ui8 fertilities [REGION_TILE_COUNT];
	} *regions;
};

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
		ASSERT(extra <= 0xFF);
		return { (ui32)type | ((ui32)(index & 0xFFF) << 8) | ((ui32)(extra & 0xFFF) << 20) };
	}
	ASSERT(index <= 0xFFF);
	ASSERT(extra <= 0xFFF);
	return { (ui32)type | ((ui32)index << 8) | ((ui32)(extra & 0xFF) << 24) };
}

bool operator==(const entity_guid& guid_a, const entity_guid& guid_b)
{
	return guid_a._value == guid_b._value; // Whole value, so _extra is compared too.
}

// Reused entity sub-components

struct world_entity_movement
{
	ui8 travel_speed; // Speed in tiles per second.
	ui8 fractional_loc; // Normalized distance from logical location tile center as the entity travels. At 0, dead center. At 1, at tile edge.
	world_location move_target; // Location the entity is moving towards.
};

struct world_entity_container
{
	ui16 max_count;
	entity_guid* guids; // Computed GUIDs for active entities. Set to 0 for inactive slots.
	ui16 active_count; // How many entities in the container are known to be active.
};

// Property arrays for a world's settlements.
struct world_entity_settlements : public world_entity_container
{
	match_player_id*	owners;
	world_location*		locations;

	ui32*				populations;
	ui32*				local_wealth;
	ui8*				tier;

	ui8*				trade_attractivity;
	ui8*				area_influence;

	// Consolidated representation of a single settlement for viewing purposes.
	struct single
	{
		match_player_id owner;
		world_location location;

		ui32 population;
		ui32 local_wealth;
		ui8 tier;

		ui8 trade_attractivity;
		ui8 area_influence;
	};
};

// Property arrays for a world's armies.
struct world_entity_armies : public world_entity_container
{
	match_player_id* owners;
	world_location*	locations;

	world_entity_movement* movements;

	struct comp
	{
		ui16 levies;
		ui16 archers;
		ui32 men_at_arms;
		ui16 horsemen;
		ui16 knights;
	} *compositions;

	entity_guid* action_targets; // What entity this army is currently engaging or following, if any.
	entity_guid* arrows_targets; // What entity this army last shot at, if any.

	// Consolidated representation of a single army for viewing purposes.
	struct single
	{
		match_player_id owner;
		world_location location;
		world_entity_movement movement;
		comp composition;
		entity_guid action_target;
		entity_guid arrows_target;
	};
};

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

// Contains the match's world state, namely its active entities, and static elements such as tiles.
// Overall this represents the "observable state" of the match that can be interacted with indirectly by sending input commands into the next tick.
struct match_world_state
{
	world_terrain terrain;

	// Holds all entity data for this world state.
	// Currently is all pre-allocated for pre-estimated max possible usage.
	// TODO(Marc): We could have a single big "entities" arena (or maybe a macro vs micro one) that grows as needed block by block.
	// We don't need ALL entities of a given type to be next to one another in memory.
	struct entities_store
	{
		world_entity_settlements settlements;
		world_entity_armies armies;
		world_entity_caravans caravans;
	} entities;
};

// Initializes a full world state according to the passed parameters.
// Memory is allocated for the full terrain tile info, and pre-allocated for a reasonable number of each entity type given the size of the world.
// TODO(Marc): Profile system so worlds can be exactly configured (image-based terrain generation, exact max entity counts...).
match_world_state* world_state_init(mem_arena& memory, world_dimensions size_regions);

struct game_match;

// Returns true if the GUID is non-zero, its index is in range, and the GUID stored at that slot is identical to it.
// If the entity was destroyed and its slot reused, the stored GUID will (almost always) differ in its _extra field, and this returns false.
bool match_entity_is_valid(const game_match& match, entity_guid entity);

// Per-type queries: if entity is a valid entity of the matching type, fills out_state with its current state and returns true.
// Otherwise returns false and leaves out_state untouched.
bool query_entity_state_settlement(const game_match& match, entity_guid entity, world_entity_settlements::single& out_state);
bool query_entity_state_caravan(const game_match& match, entity_guid entity, world_entity_caravans::single& out_state);
bool query_entity_state_army(const game_match& match, entity_guid entity, world_entity_armies::single& out_state);

#endif // MATCH_WORLD_INCLUDED