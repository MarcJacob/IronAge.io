// Symbols used commonly across world entity types.

#ifndef WORLD_ENTITIES_INCLUDED
#define WORLD_ENTITIES_INCLUDED

#include "core.h"
#include "game_common/game_data_object.h"

#include "../world.h"

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

struct world_entity_container
{
	ui16 max_count;
	entity_guid* guids; // Computed GUIDs for active entities. Set to 0 for inactive slots.
	ui16 active_count; // How many entities in the container are known to be active.
};

// Reused entity sub-components

// Movement data assigned to all entities that can move over the map.
struct world_entity_movement
{
	ui16 travel_speed; // Speed in thousandths of a tile per tick (fractional units per tick), along an axis. Diagonal moves split it between the axes.
	vec2<i16> fractional_loc; // "In-tile location" of the entity, centered on 0, in thousandths of a tile (range [-500, 500]). Affects movement.
	world_location destination; // Tile the entity wants to reach. Set by orders and decisions.
	ui32 path_handle; // Path the entity is following, INVALID_PATH_HANDLE if none.
};

// Contains a count of all troop types.
struct army_troop_count
{
    ui16 levies;
    ui16 men_at_arms;
    ui16 horsemen;
    ui16 knights;
};

bool            world_entity_is_valid(const match_world_state& world, entity_guid entity);
world_location  world_entity_get_location(const match_world_state& world, entity_guid entity);
match_player_id world_entity_get_owner(const match_world_state& world, entity_guid entity);

#endif // WORLD_ENTITIES_INCLUDED
