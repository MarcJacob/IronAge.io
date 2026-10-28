// Symbols for Worker type world entities.

#ifndef WORLD_ENTITY_WORKERS_INCLUDED
#define WORLD_ENTITY_WORKERS_INCLUDED

#include "core.h"
#include "world_entity_common.h"

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
    ui32 goods; // Market Goods taken from the origin with the colonists (see settlement_colonist_goods). The new settlement's pool, or given back to the origin if the worker has to return.
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

	ui32* daily_costs; // Paid from the origin owner's treasury (spent) for every day the project advances. 0 = free.

	world_worker_action_data* action_data;
	world_location* locations; // Where the worker is on the map. Starts at the origin entity's location; only moving work types change it.
	world_entity_movement* movements; // Movement state, used by work types that travel (like armies').

	struct view;

	// Consolidated representation of a single worker for viewing purposes.
	struct single
	{
		entity_guid origin_entity;
		WORK_TYPE work_type;
		ui32 work_time;

        ui8 work_progress_ratio;

		ui32 daily_cost;

		world_worker_action_data action_data;
		world_location position;
		world_entity_movement movement;

		match_player_id owner; // Derived: owner of the origin entity (world_entity_get_owner). Not stored per worker.

		void copy_from(const view& source);
		void copy_from(const world_entity_workers& collection, ui16 index);
	};

	// Reference view of a worker slot. The owner is derived by the query and is not stored in the collection.
	struct view
	{
		entity_guid& origin_entity;
		WORK_TYPE& work_type;
		ui32& work_time;
		ui8& work_progress_ratio;
		ui32& daily_cost;
		world_worker_action_data& action_data;
		world_location& position;
		world_entity_movement& movement;

		void copy_from(const single& source)
		{
			origin_entity = source.origin_entity;
			work_type = source.work_type;
			work_time = source.work_time;
			work_progress_ratio = source.work_progress_ratio;
			daily_cost = source.daily_cost;
			action_data = source.action_data;
			position = source.position;
			movement = source.movement;
		}

		view(world_entity_workers& collection, ui16 index)
			: origin_entity(collection.origin_entity[index])
			, work_type(collection.work_types[index])
			, work_time(collection.work_times[index])
			, work_progress_ratio(collection.work_progress_ratios[index])
			, daily_cost(collection.daily_costs[index])
			, action_data(collection.action_data[index])
			, position(collection.locations[index])
			, movement(collection.movements[index])
		{}
	};
};

inline void world_entity_workers::single::copy_from(const world_entity_workers::view& source)
{
	origin_entity = source.origin_entity;
	work_type = source.work_type;
	work_time = source.work_time;
	work_progress_ratio = source.work_progress_ratio;
	daily_cost = source.daily_cost;
	action_data = source.action_data;
	position = source.position;
	movement = source.movement;
	owner = INVALID_MATCH_PLAYER_ID;
}

inline void world_entity_workers::single::copy_from(const world_entity_workers& collection, ui16 index)
{
	origin_entity = collection.origin_entity[index];
	work_type = collection.work_types[index];
	work_time = collection.work_times[index];
	work_progress_ratio = collection.work_progress_ratios[index];
	daily_cost = collection.daily_costs[index];
	action_data = collection.action_data[index];
	position = collection.locations[index];
	movement = collection.movements[index];
	owner = INVALID_MATCH_PLAYER_ID;
}

using world_entity_worker = world_entity_workers::single;
using world_entity_worker_view = world_entity_workers::view;

// action_data is left out: it is a byte buffer, which has no property type yet.
GAME_DATA_OBJECT_DEF_BEGIN(world_entity_worker)
	GAME_DATA_PROPERTY(origin_entity, GAME_DATA_PROP_TYPE::ENTITY_GUID)
	GAME_DATA_PROPERTY(work_type, GAME_DATA_PROP_TYPE::UI16)
	GAME_DATA_PROPERTY(work_time, GAME_DATA_PROP_TYPE::UI32)
	GAME_DATA_PROPERTY(owner, GAME_DATA_PROP_TYPE::PLAYER_ID)
	GAME_DATA_PROPERTY_PATH(location, position, GAME_DATA_PROP_TYPE::WORLD_LOCATION) // Named like the other entities' so generic inspection shows it.
GAME_DATA_OBJECT_DEF_END()

bool world_entity_query_worker(const match_world_state& world, entity_guid entity, world_entity_worker& out_state);

#endif // WORLD_ENTITY_WORKERS_INCLUDED
