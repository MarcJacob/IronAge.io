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
    ui32*               land_sizes;
    ui32*               land_outputs; // Set by influence system according to terrain type, infrastructure...

    struct land_influence
    {
        world_location bounds_min, bounds_max; // Rectangle bounds of all currently claimed tiles.
        i32 pressure; // Population - land_output * 3. Positive = surplus wanting to expand.
    };
	land_influence*				influences; // Influence values used to claim / lose land.

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

		ui8 tier;

		ui8 trade_attaction_index;
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

// Minimum distance (in tiles) between a new settlement and the closest other settlement.
constexpr ui32 SETTLEMENT_MIN_DISTANCE = 16;

// Returns whether the location is a valid place for the player to build a settlement: buildable terrain, tile unclaimed or owned by the player,
// and at least SETTLEMENT_MIN_DISTANCE tiles away from every settlement.
bool world_is_valid_settlement_location(const match_world_state& world, world_location loc, match_player_id player);

#endif // MATCH_WORLD_INCLUDED
