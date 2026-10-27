// Implementation for the query bridging code between JS frontend and Client backend: entity inspection, game parameters, world state values and paths.

#include "game_client/game_client_backend.h"
#include "game_common/game_data_object.h"
#include "game_common/match/game_parameters.h"
#include "game_common/match/world/world_state.h"
#include "web_game_client.h"

static entity_inspection_context ENTITY_INSPECTION; // Inspection context for the currently selected entity.

// Extra data of an inspected settlement, a second GDO written right after the settlement's own GDO: its ongoing building projects, one entry per project
// (parallel arrays, the first construction_project_count entries are used). Built from the world's workers by this layer only, see inspect_settlement_extra.
struct entity_inspection_extra_settlement
{
    static constexpr ui8 MAX_PROJECTS = 32; // Projects beyond this are not reported.

    ui16 construction_project_count;
    ui8 construction_project_building_type[MAX_PROJECTS]; // Row of SETTLEMENT_BUILDING_TABLE.
    ui8 construction_project_progress[MAX_PROJECTS]; // Overall progress towards completion, normalized: 0 = just started, 255 = done.
};

GAME_DATA_OBJECT_DEF_BEGIN(entity_inspection_extra_settlement)
    GAME_DATA_PROPERTY(construction_project_count, GAME_DATA_PROP_TYPE::UI16)
    GAME_DATA_PROPERTY_PATH_ARRAY(construction_project_building_type, construction_project_building_type, GAME_DATA_PROP_TYPE::UI8, entity_inspection_extra_settlement::MAX_PROJECTS)
    GAME_DATA_PROPERTY_PATH_ARRAY(construction_project_progress, construction_project_progress, GAME_DATA_PROP_TYPE::UI8, entity_inspection_extra_settlement::MAX_PROJECTS)
GAME_DATA_OBJECT_DEF_END();

// Every inspection (entity GDO + extra data if any) must fit the inspection buffer.
static_assert(sizeof(gdo_world_entity_settlement) + sizeof(gdo_entity_inspection_extra_settlement) <= entity_inspection_context::BUFFER_SIZE, "Settlement inspection data does not fit the inspection buffer.");
static_assert(sizeof(gdo_world_entity_army) <= entity_inspection_context::BUFFER_SIZE, "Army GDO does not fit the inspection buffer.");
static_assert(sizeof(gdo_world_entity_worker) <= entity_inspection_context::BUFFER_SIZE, "Worker GDO does not fit the inspection buffer.");
static_assert(sizeof(gdo_world_entity_battle) <= entity_inspection_context::BUFFER_SIZE, "Battle GDO does not fit the inspection buffer.");

// Writes the extra GDO of a settlement at out_buffer (buffer_size bytes free): its ongoing BUILD_SETTLEMENT_BUILDING projects.
// The web client is a unity build with the client backend, so the local world is read directly. Returns the byte size written, 0 if nothing was written.
static ui32 inspect_settlement_extra(entity_guid settlement, ui8* out_buffer, ui32 buffer_size)
{
    if (sizeof(gdo_entity_inspection_extra_settlement) > buffer_size) return 0;
    if (WEB_CLIENT.backend->local_match == nullptr) return 0;

    const world_entity_workers& workers = WEB_CLIENT.backend->local_match->world->entities->workers;

    entity_inspection_extra_settlement extra = {};
    for (ui16 workerIndex = 0; workerIndex < workers.max_count && extra.construction_project_count < entity_inspection_extra_settlement::MAX_PROJECTS; workerIndex++)
    {
        if (!workers.guids[workerIndex].is_valid()) continue;
        if ((WORK_TYPE)workers.work_types[workerIndex] != WORK_TYPE::BUILD_SETTLEMENT_BUILDING) continue;
        if (workers.origin_entity[workerIndex]._value != settlement._value) continue;

        const build_settlement_building_data& data = *(const build_settlement_building_data*)workers.action_data[workerIndex].bytes;
        if (data.building_type >= SETTLEMENT_BUILDING_TYPE_COUNT) continue; // Unknown type: the worker system finishes it.

        // Days financed over the build time, scaled to 0..255.
        ui32 buildDays = SETTLEMENT_BUILDING_TABLE[data.building_type].build_days;
        ui32 progress = buildDays == 0 ? 255 : (ui32)data.progress_days * 255 / buildDays;

        ui16 at = extra.construction_project_count++;
        extra.construction_project_building_type[at] = data.building_type;
        extra.construction_project_progress[at] = (ui8)(progress > 255 ? 255 : progress);
    }

    gdo_entity_inspection_extra_settlement gdo; // Built on the stack then copied, as for the other GDOs.
    ia_memcpy(&gdo._data, &extra, sizeof(extra));
    ia_memcpy(out_buffer, &gdo, sizeof(gdo));
    return sizeof(gdo);
}

WASM_EXPORT ui8* client_get_inspected_entity_gdo()
{
	return ENTITY_INSPECTION.inspection_buffer;
}

WASM_EXPORT ui8* client_get_inspected_entity_extra_data()
{
    return ENTITY_INSPECTION.inspection_buffer + ENTITY_INSPECTION.extra_data_offset;
}

// Inspects the entity: its GDO is written at the start of the inspection buffer, followed by extra data (a second GDO) for the entity types that have some
// (settlements: ongoing construction projects). The buffer is overwritten by every call: JS lifts out what it needs right away.
// Returns the total byte size written (GDO + extra data), or 0 if the entity doesn't exist (anymore), its type has no GDO yet, or its GDO doesn't fit the buffer.
// The extra data is at client_get_inspected_entity_extra_data: it exists if the returned size is above the size of the GDO (first ui32 of the GDO header).
WASM_EXPORT ui32 client_inspect_entity(ui32 guid)
{
	ASSERT(WEB_CLIENT.backend != nullptr);

    entity_guid inspectedGUID = entity_guid { guid };

    // Load GDO in first part of the inspection buffer.
	ui32 gdoSize = game_client_query_entity_gdo(*WEB_CLIENT.backend, inspectedGUID, ENTITY_INSPECTION.inspection_buffer, entity_inspection_context::BUFFER_SIZE);
    if (gdoSize == 0)
    {
        ENTITY_INSPECTION.extra_data_offset = 0;
        return 0;
    }

    ENTITY_INSPECTION.inspected_entity_guid = inspectedGUID;
    ENTITY_INSPECTION.extra_data_offset = gdoSize;

    // Entity-type-specific extra data, which the entity's GDO doesn't provide. Other entity types have none.
    ui32 extraDataSize = 0;
    switch (inspectedGUID.get_type())
    {
    case ENTITY_TYPE::SETTLEMENT:
        extraDataSize = inspect_settlement_extra(inspectedGUID, ENTITY_INSPECTION.inspection_buffer + gdoSize, entity_inspection_context::BUFFER_SIZE - gdoSize);
        break;
    default:
        break;
    }

    return gdoSize + extraDataSize;
}

static ui8 BUILDING_DEFINITION_GDO_BUFFER[sizeof(gdo_settlement_building_def)]; // Plain bytes on purpose, see the global constructor note in web_game_client.h.

// Fills the buffer with the GDO of the given building type (row of SETTLEMENT_BUILDING_TABLE, GDO defined under it) and returns its offset, null if the type doesn't exist.
// Constant data: JS reads each type once.
WASM_EXPORT ui8* client_get_building_definition_gdo(ui32 type)
{
	if (type >= SETTLEMENT_BUILDING_TYPE_COUNT) return nullptr;

	gdo_settlement_building_def gdo; // Built on the stack then copied, as for the game parameters.
	ia_memcpy(&gdo._data, &SETTLEMENT_BUILDING_TABLE[type], sizeof(settlement_building_def));
	ia_memcpy(BUILDING_DEFINITION_GDO_BUFFER, &gdo, sizeof(gdo_settlement_building_def));
	return BUILDING_DEFINITION_GDO_BUFFER;
}

// Global game parameters: the constant GAME_PARAMETERS as a Game Data Object, in its own buffer. JS calls client_get_game_parameters_gdo_offset (which fills it)
// and reads the properties by name. Plain bytes on purpose, see the global constructor note in web_game_client.h.
static ui8 GAME_PARAMETERS_GDO_BUFFER[sizeof(gdo_game_parameters)];

WASM_EXPORT ui8* client_get_game_parameters_gdo_offset()
{
	gdo_game_parameters gdo; // Built on the stack then copied, as for the command GDO.
	ia_memcpy(&gdo._data, &GAME_PARAMETERS, sizeof(game_parameters));
	ia_memcpy(GAME_PARAMETERS_GDO_BUFFER, &gdo, sizeof(gdo_game_parameters));
	return GAME_PARAMETERS_GDO_BUFFER;
}

// World age in ticks as a double (a wasm i64 would reach JS as a BigInt): exact up to 2^53 ticks.
WASM_EXPORT double client_query_world_age()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	return (double)game_client_query_world_age(*WEB_CLIENT.backend);
}

// State of the controlled player (world_player_state) as a Game Data Object, in its own buffer. JS calls client_get_player_state_gdo_offset (which fills it:
// the values change, so read it again every time) and reads the properties by name. Zeroed without a match. Plain bytes on purpose, see the global constructor note in web_game_client.h.
static ui8 PLAYER_STATE_GDO_BUFFER[sizeof(gdo_world_player_state)];

WASM_EXPORT ui8* client_get_player_state_gdo_offset()
{
	ASSERT(WEB_CLIENT.backend != nullptr);
	world_player_state state = game_client_query_controlled_player_state(*WEB_CLIENT.backend);
	gdo_world_player_state gdo; // Built on the stack then copied, as for the game parameters.
	ia_memcpy(&gdo._data, &state, sizeof(world_player_state));
	ia_memcpy(PLAYER_STATE_GDO_BUFFER, &gdo, sizeof(gdo_world_player_state));
	return PLAYER_STATE_GDO_BUFFER;
}

// Path query: JS calls client_query_path, which returns how many waypoints it wrote, then reads that many (x, y) ui16 pairs
// at the offset given by client_get_path_buffer_offset. The start tile is not part of the waypoints, see struct path.
// A plain ui16 array rather than world_location on purpose: see the global constructor note in web_game_client.h.
static constexpr ui16 PATH_BUFFER_CAPACITY = 1024; // In waypoints.
static ui16 PATH_BUFFER[PATH_BUFFER_CAPACITY * 2];
static_assert(sizeof(world_location) == 4, "PATH_BUFFER is read as (x, y) ui16 pairs, matching world_location.");

WASM_EXPORT ui8* client_get_path_buffer_offset()
{
	return (ui8*)PATH_BUFFER;
}

// Returns the number of waypoints written to the path buffer (0 if there is no path to follow).
// territory_guid: a settlement's guid to limit the path to its territory, or 0 for no limit.
// tile_mode: 0 writes the waypoints (straight lines), non-zero writes the tiles of the path's segments instead (see game_client_query_path).
WASM_EXPORT ui32 client_query_path(ui32 start_x, ui32 start_y, ui32 end_x, ui32 end_y, ui32 territory_guid, ui32 tile_mode)
{
	ASSERT(WEB_CLIENT.backend != nullptr);

	path result = {};
	result.points = (world_location*)PATH_BUFFER;
	result.points_capacity = PATH_BUFFER_CAPACITY;

	game_client_query_path(*WEB_CLIENT.backend, { (ui16)start_x, (ui16)start_y }, { (ui16)end_x, (ui16)end_y }, result, entity_guid{ territory_guid }, tile_mode != 0);
	return result.count;
}
