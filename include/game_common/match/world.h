// Contains the world state elements of a match.

#ifndef MATCH_WORLD_INCLUDED
#define MATCH_WORLD_INCLUDED

#include "core.h"

using world_location = vec2<ui16>;
using world_dimensions = vec2<ui16>;

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
union entity_guid
{
	ui32 guid;
	struct
	{
		ENTITY_TYPE _type;
		union
		{
			struct
			{
				ui16 _index : 12; // Up to 4098 active macro entities at a time, per type.
				ui16 _extra : 12; // Based on something unpredictable like time of creation.
			} _static;
			struct
			{
				ui16 _index; // Up to 65635 active micro entities at a time per type.
				ui8 _extra; // Based on something unpredictable like time of creation.
			} _dynamic;
		};
	};
};

// Invalid / non-existent entity value. Note that non-zero values may still reference an entity that is not valid, in which case that is because it doesn't exist anymore.
constexpr entity_guid INVALID_ENTITY_GUID = { 0 };

// Reused entity sub-components

struct world_entity_movement
{
	bool* _activeFlags;

	ui8 travel_speed; // Speed in tiles per second.
	ui8 fractional_loc; // Normalized distance from logical location tile center as the entity travels. At 0, dead center. At 1, at tile edge.
	world_location move_target; // Location the entity is moving towards.
};

struct world_entity_container
{
	ui16 max_count;
	bool* _activeFlags; // Whether a specific entry is being used by an actual entity instance or not.
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

#endif // MATCH_WORLD_INCLUDED