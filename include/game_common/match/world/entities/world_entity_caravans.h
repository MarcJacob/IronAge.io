// Symbol declarations for the Caravan world entities.

#ifndef WORLD_ENTITY_CARAVANS_INCLUDED
#define WORLD_ENTITY_CARAVANS_INCLUDED

#include "core.h"
#include "world_entity_common.h"

// Flags for Caravan entities.
enum class WORLD_ENTITY_CARAVAN_FLAGS : ui8
{
    IS_SHIP = 1 << 0, // "Ships" are water caravans, as we all know.
    RETURNING_HOME = 1 << 1, // Found no destination: walking back to the origin to refund its value there.
};

struct world_entity_caravans : public world_entity_container
{
    world_location* location;
    world_entity_movement* movement;

    WORLD_ENTITY_CARAVAN_FLAGS* flags;
    entity_guid* origin_settlement;
    entity_guid* destination; // Settlement or army.

    ui16* value;

    struct view;

    struct single
    {
        world_location location;
        world_entity_movement movement;

        WORLD_ENTITY_CARAVAN_FLAGS flags;
        entity_guid origin_settlement;
        entity_guid destination;

        ui16 value;

        match_player_id owner; // Derived: owner of the origin settlement (world_entity_get_owner). Not stored per caravan.

        void copy_from(const view& source);
        void copy_from(const world_entity_caravans& collection, ui16 index);
    };

    // Reference view of a caravan slot. The owner is derived by the query and is not stored in the collection.
    struct view
    {
        world_location& location;
        world_entity_movement& movement;
        WORLD_ENTITY_CARAVAN_FLAGS& flags;
        entity_guid& origin_settlement;
        entity_guid& destination;
        ui16& value;

        void copy_from(const single& source)
        {
            location = source.location;
            movement = source.movement;
            flags = source.flags;
            origin_settlement = source.origin_settlement;
            destination = source.destination;
            value = source.value;
        }

        view(world_entity_caravans& collection, ui16 index)
            : location(collection.location[index])
            , movement(collection.movement[index])
            , flags(collection.flags[index])
            , origin_settlement(collection.origin_settlement[index])
            , destination(collection.destination[index])
            , value(collection.value[index])
        {}
    };
};

inline void world_entity_caravans::single::copy_from(const world_entity_caravans::view& source)
{
    location = source.location;
    movement = source.movement;
    flags = source.flags;
    origin_settlement = source.origin_settlement;
    destination = source.destination;
    value = source.value;
    owner = INVALID_MATCH_PLAYER_ID;
}

inline void world_entity_caravans::single::copy_from(const world_entity_caravans& collection, ui16 index)
{
    location = collection.location[index];
    movement = collection.movement[index];
    flags = collection.flags[index];
    origin_settlement = collection.origin_settlement[index];
    destination = collection.destination[index];
    value = collection.value[index];
    owner = INVALID_MATCH_PLAYER_ID;
}

using world_entity_caravan = world_entity_caravans::single;
using world_entity_caravan_view = world_entity_caravans::view;

GAME_DATA_OBJECT_DEF_BEGIN(world_entity_caravan)
GAME_DATA_PROPERTY(location, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
GAME_DATA_PROPERTY(flags, GAME_DATA_PROP_TYPE::UI8)

GAME_DATA_PROPERTY_PATH(movement_destination, movement.destination, GAME_DATA_PROP_TYPE::WORLD_LOCATION)
GAME_DATA_PROPERTY_PATH(movement_fractional_loc, movement.fractional_loc, GAME_DATA_PROP_TYPE::VEC2_UI16)
GAME_DATA_PROPERTY_PATH(movement_travel_speed, movement.travel_speed, GAME_DATA_PROP_TYPE::UI16)

GAME_DATA_PROPERTY(origin_settlement, GAME_DATA_PROP_TYPE::ENTITY_GUID)
GAME_DATA_PROPERTY_PATH(destination_settlement, destination, GAME_DATA_PROP_TYPE::ENTITY_GUID) // Name kept for the client; the guid may be a settlement or an army.
GAME_DATA_PROPERTY(value, GAME_DATA_PROP_TYPE::UI16)
GAME_DATA_PROPERTY(owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
GAME_DATA_OBJECT_DEF_END();

bool world_entity_query_caravan(const match_world_state& world, entity_guid entity, world_entity_caravan& out_state);

#endif // WORLD_ENTITY_CARAVANS_INCLUDED
