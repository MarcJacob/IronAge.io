// Contains the world state elements of a match.

#ifndef MATCH_WORLD_INCLUDED
#define MATCH_WORLD_INCLUDED

#include "core.h"
#include "core/math/random.h"

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
enum TERRAIN_TILE_TYPE : ui8
{
	LAND_PLAINS,
	LAND_HILLS,
	LAND_MOUNTAINS,

	WATER_COAST,
	WATER_SEA,
};

// Bit flags of a tile (world_terrain::region::tile_flags).
enum TILE_FLAGS : ui8
{
	TILE_FLAG_BORDER = 1 << 0, // Owned tile touching an unowned 8-neighbour, or a neighbour owned by a higher settlement slot (one-sided at contact lines).
	TILE_FLAG_TAKEOVER = 1 << 1, // Transient: set and cleared inside the takeover pass (tile is to change owner this tick). Never set between ticks.
	// Room for future flags (roads...).
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
        ui16 owners[REGION_TILE_COUNT]; // Tile owner if any, as a settlement slot index (see UNOWNED_TILE). Determines which settlement obtains the tile's production output.
        ui8 influence_levels[REGION_TILE_COUNT]; // How much influence the owning settlement has over the tile. 0 = unowned. Determines ability to hold it and exploit it.
        ui8 contestation_levels[REGION_TILE_COUNT]; // How contested the tile is between its owner settlement and neighbouring settlements. Decreases productivity.
        ui8 tile_flags[REGION_TILE_COUNT]; // TILE_FLAGS bits. BORDER is maintained incrementally by the influence system.

        // Scratch plane for the double-buffered influence growth (written during the pass, then copied back). Not state.
        // Avoids a directional bias in growth from the fixed scan order.
        // @TEMP(Marc): Per-region scratch buffers like this are pretty memory inefficient. Later we'll come up with some sort of
        // intermediate update memory scheme so less is used, by spreading the work over multiple passes / ticks.
        ui8 influence_next[REGION_TILE_COUNT];
        ui8 contestation_next[REGION_TILE_COUNT]; // Contestation sources, then the new contestation, before the copy back.
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

// Reused entity sub-components

struct world_entity_movement
{
	ui8 travel_speed; // Speed in tenth of tile per tick.
	vec2<i8> fractional_loc; // "In-tile location" of the entity. Affects movement.
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

    // Base economic factors
	ui32*				populations;
	ui32*				local_wealths;
    ui32*               land_outputs;

    // Infrastructure / building factors
	ui8*				tiers;

    // Intermediate values
	ui8*				trade_attraction_indices;
	ui32*				influence_capacities;

    // Per-slot values for the influence / land output systems. Derived, rewritten every tick.
    ui64*               maintenance_totals; // Sum of maintenance cost of owned tiles, fixed point (capacity * 16). Set by the land output pass, adjusted by claims / unclaims in between.
    ui16*               maintenance_overshoots; // How far over budget (1..256, in 1/256 of capacity), 0 = within budget. Set at the start of each influence tick.
    ui64*               land_output_sums; // Fixed-point (x255 x255) land output accumulator.

	// Consolidated representation of a single settlement for viewing purposes.
	struct single
	{
		match_player_id owner;
		world_location location;

		ui32 population;
		ui32 local_wealth;
        ui32 land_output;

		ui8 tier;

		ui8 trade_attaction_index;
        ui32 influence_capacity;
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

// @TEMP(Marc): Static memory size / layout properties.
constexpr ui32 SETTLEMENTS_PER_REGION = 10;
constexpr ui32 MICRO_ENTITIES_PER_TYPE_PER_REGION = 32;

// Contains the match's world state, namely its active entities, and static elements such as tiles.
// Overall this represents the "observable state" of the match that can be interacted with indirectly by sending input commands into the next tick.
struct match_world_state
{
    ui64 age; // Age of the world in ticks.

    rand_generator_32 rand_gen; // Main random generator used by this world's systems.

	world_terrain terrain; // World Terrain data.

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
match_world_state* world_state_init(mem_arena& memory, ui32 rand_seed);

// World query functions.

bool world_entity_is_valid(const match_world_state& world, entity_guid entity);

world_location world_get_entity_location(const match_world_state& world, entity_guid entity);

bool query_entity_state_settlement(const match_world_state& world, entity_guid entity, world_entity_settlements::single& out_state);

bool query_entity_state_caravan(const match_world_state& world, entity_guid entity, world_entity_caravans::single& out_state);

bool query_entity_state_army(const match_world_state& world, entity_guid entity, world_entity_armies::single& out_state);

bool world_caravan_is_owned_by(const match_world_state& world, entity_guid caravan, match_player_id player);

#endif // MATCH_WORLD_INCLUDED
