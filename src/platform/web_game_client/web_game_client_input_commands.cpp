// Implementation for building match commands by property name from the JS frontend, through a Game Data Object (see game_data_object.h).
// JS calls client_command_create with a command name, then client_command_set_property per field, then client_command_send or client_command_query_valid.
// Strings (command and property names) are passed as NUL-terminated bytes written in INPUT_EVENT_BUFFER (web_game_client_input.cpp).
// Only one command GDO exists at a time: creating one overwrites the one in progress.

#include "game_client/game_client_backend.h"
#include "game_common/match/commands.h"
#include "web_game_client.h"

static constexpr ui32 GDO_BUFFER_SIZE = 128; // Must fit the largest command GDO.
static ui8 GDO_BUFFER[GDO_BUFFER_SIZE]; // All zero (no GDO) until client_command_create succeeds.
static MATCH_COMMAND_TYPE GDO_COMMAND_TYPE; // Type of the command the GDO in the buffer was created for.

// Results of client_command_create. Mirrored in backend.ts.
enum COMMAND_CREATE_RESULT : ui32
{
	COMMAND_CREATE_OK,
	COMMAND_CREATE_UNKNOWN_NAME, // No command has that name.
	COMMAND_CREATE_NO_GDO, // The command exists but has no GDO definition (yet).
	COMMAND_CREATE_TOO_LARGE, // The GDO does not fit in GDO_BUFFER.
};

// Copies a fresh GDO of the given type in the buffer.
template<typename GdoType>
static COMMAND_CREATE_RESULT gdo_buffer_place(MATCH_COMMAND_TYPE command_type)
{
	if (sizeof(GdoType) > GDO_BUFFER_SIZE) return COMMAND_CREATE_TOO_LARGE;

	// Built on the stack then copied: the buffer is plain bytes on purpose, see the global constructor note in web_game_client.h.
	GdoType gdo;
	ia_memcpy(GDO_BUFFER, &gdo, sizeof(GdoType));
	GDO_COMMAND_TYPE = command_type;
	return COMMAND_CREATE_OK;
}

// The only place mapping a command type to its GDO definition. Add one case per converted command.
static COMMAND_CREATE_RESULT gdo_buffer_create(MATCH_COMMAND_TYPE command_type)
{
	switch (command_type)
	{
	case MATCH_COMMAND_TYPE::SET_ENTITY_MOVE_TARGET: return gdo_buffer_place<gdo_command_data_set_entity_move_target>(command_type);
	case MATCH_COMMAND_TYPE::SET_ENTITY_ATTACK_TARGET: return gdo_buffer_place<gdo_command_data_set_entity_attack_target>(command_type);
	case MATCH_COMMAND_TYPE::FOUND_SETTLEMENT: return gdo_buffer_place<gdo_command_data_found_settlement>(command_type);
	case MATCH_COMMAND_TYPE::RAISE_ARMY: return gdo_buffer_place<gdo_command_data_raise_army>(command_type);
	case MATCH_COMMAND_TYPE::BUILD_ROAD: return gdo_buffer_place<gdo_command_data_build_road>(command_type);
	case MATCH_COMMAND_TYPE::SET_SETTLEMENT_TROOP_CEILING: return gdo_buffer_place<gdo_command_data_set_settlement_troop_ceiling>(command_type);
	case MATCH_COMMAND_TYPE::SET_ARMY_RAISE_TARGETS: return gdo_buffer_place<gdo_command_data_set_army_raise_targets>(command_type);
	case MATCH_COMMAND_TYPE::STOP_RAISING_ARMY: return gdo_buffer_place<gdo_command_data_stop_raising_army>(command_type);
	case MATCH_COMMAND_TYPE::SET_SETTLEMENT_TAX_RATE: return gdo_buffer_place<gdo_command_data_set_settlement_tax_rate>(command_type);
	case MATCH_COMMAND_TYPE::BUILD_SETTLEMENT_BUILDING: return gdo_buffer_place<gdo_command_data_build_settlement_building>(command_type);
	case MATCH_COMMAND_TYPE::DISBAND_ARMY: return gdo_buffer_place<gdo_command_data_disband_army>(command_type);
	case MATCH_COMMAND_TYPE::SET_EXPORT_RATIO: return gdo_buffer_place<gdo_command_data_set_export_ratio>(command_type);
		case MATCH_COMMAND_TYPE::SET_ALLOW_FOREIGN_EXPORT: return gdo_buffer_place<gdo_command_data_set_allow_foreign_export>(command_type);
	case MATCH_COMMAND_TYPE::BEGIN_ASSAULT: return gdo_buffer_place<gdo_command_data_begin_assault>(command_type);
	default: return COMMAND_CREATE_NO_GDO;
	}
}

// Returns the name JS wrote in INPUT_EVENT_BUFFER, guaranteed NUL-terminated.
static const char* gdo_read_name_buffer()
{
	INPUT_EVENT_BUFFER[INPUT_EVENT_MAX_PAYLOAD_SIZE - 1] = 0;
	return (const char*)INPUT_EVENT_BUFFER;
}

// The GDO in the buffer, or nullptr if there is none.
static game_data_object* gdo_get_current()
{
	game_data_object* gdo = (game_data_object*)GDO_BUFFER;
	return gdo->_totalSize != 0 ? gdo : nullptr;
}

// Writes the numeric arguments into a property according to its type. Returns false for types not supported (64 bit ones).
// Values are truncated to the property's size: the server validates the command anyway.
static bool gdo_set_numeric(game_data_object& gdo, const GAME_DATA_PROP_DEF& prop, i32 x, i32 y)
{
	switch (prop.type)
	{
	case GAME_DATA_PROP_TYPE::UI8: gdo.access_property_val<ui8>(prop) = (ui8)x; return true;
	case GAME_DATA_PROP_TYPE::I8: gdo.access_property_val<i8>(prop) = (i8)x; return true;
	case GAME_DATA_PROP_TYPE::UI16: gdo.access_property_val<ui16>(prop) = (ui16)x; return true;
	case GAME_DATA_PROP_TYPE::I16: gdo.access_property_val<i16>(prop) = (i16)x; return true;
	case GAME_DATA_PROP_TYPE::UI32: gdo.access_property_val<ui32>(prop) = (ui32)x; return true;
	case GAME_DATA_PROP_TYPE::I32: gdo.access_property_val<i32>(prop) = x; return true;
	case GAME_DATA_PROP_TYPE::PLAYER_ID: gdo.access_property_val<match_player_id>(prop) = (match_player_id)x; return true;
	case GAME_DATA_PROP_TYPE::ENTITY_GUID: gdo.access_property_val<entity_guid>(prop) = entity_guid{ (ui32)x }; return true;
	case GAME_DATA_PROP_TYPE::WORLD_LOCATION: gdo.access_property_val<world_location>(prop) = world_location{ (ui16)x, (ui16)y }; return true;
	case GAME_DATA_PROP_TYPE::VEC2_UI8: gdo.access_property_val<vec2<ui8>>(prop) = vec2<ui8>{ (ui8)x, (ui8)y }; return true;
	case GAME_DATA_PROP_TYPE::VEC2_I8: gdo.access_property_val<vec2<i8>>(prop) = vec2<i8>{ (i8)x, (i8)y }; return true;
	case GAME_DATA_PROP_TYPE::VEC2_UI16: gdo.access_property_val<vec2<ui16>>(prop) = vec2<ui16>{ (ui16)x, (ui16)y }; return true;
	case GAME_DATA_PROP_TYPE::VEC2_I16: gdo.access_property_val<vec2<i16>>(prop) = vec2<i16>{ (i16)x, (i16)y }; return true;
	case GAME_DATA_PROP_TYPE::VEC2_UI32: gdo.access_property_val<vec2<ui32>>(prop) = vec2<ui32>{ (ui32)x, (ui32)y }; return true;
	case GAME_DATA_PROP_TYPE::VEC2_I32: gdo.access_property_val<vec2<i32>>(prop) = vec2<i32>{ x, y }; return true;
	default: return false;
	}
}

// Replaces the GDO in progress by a fresh one for the command named in INPUT_EVENT_BUFFER. Returns a COMMAND_CREATE_RESULT.
// On any failure no GDO is left in progress.
WASM_EXPORT ui32 client_command_create()
{
	ia_memset(GDO_BUFFER, 0, GDO_BUFFER_SIZE);

	MATCH_COMMAND_TYPE type = match_command_type_from_name(gdo_read_name_buffer());
	if (type == MATCH_COMMAND_TYPE::TYPE_COUNT) return COMMAND_CREATE_UNKNOWN_NAME;

	COMMAND_CREATE_RESULT result = gdo_buffer_create(type);
	if (result != COMMAND_CREATE_OK) ia_memset(GDO_BUFFER, 0, GDO_BUFFER_SIZE);
	return result;
}

// Offset of the GDO buffer. Its header and property definitions are the schema (read by backend.ts, debug use).
WASM_EXPORT ui8* client_command_get_buffer_offset()
{
	return GDO_BUFFER;
}

// Sets the property named in INPUT_EVENT_BUFFER on the GDO in progress. Scalars, entity guids (as ui32 bit pattern) and player ids use x only,
// vec2 and world location types use x and y. Returns false if there is no GDO, no such property, or the property type is unsupported.
WASM_EXPORT bool client_command_set_property(i32 x, i32 y)
{
	game_data_object* gdo = gdo_get_current();
	if (gdo == nullptr) return false;

	const GAME_DATA_PROP_DEF* prop = gdo->get_property_def(ia_string_view(gdo_read_name_buffer()));
	if (prop == nullptr) return false;

	return gdo_set_numeric(*gdo, *prop, x, y);
}

// Queues the command of the GDO in progress, which stays in progress. Returns whether the backend accepted it.
WASM_EXPORT bool client_command_send()
{
	game_data_object* gdo = gdo_get_current();
	if (gdo == nullptr) return false;

	return game_client_input_push_match_command(*WEB_CLIENT.backend, (ui8)GDO_COMMAND_TYPE, gdo->get_data());
}

// Feedback of the last client_command_query_valid call, as a match_command_validity_feedback GDO (plain bytes, see the global constructor note in web_game_client.h).
static ui8 FEEDBACK_BUFFER[sizeof(gdo_match_command_validity_feedback)];

// Offset of the feedback GDO buffer. Its schema and values are read by JS (all zero before the first query).
WASM_EXPORT ui8* client_command_get_feedback_offset()
{
	return FEEDBACK_BUFFER;
}

// Asks whether the command of the GDO in progress would be valid right now. Unset fields are zero.
// Also fills the feedback GDO buffer with what the check found out.
WASM_EXPORT bool client_command_query_valid()
{
	gdo_match_command_validity_feedback feedbackGdo; // Built on the stack then copied, as for the command GDO.
	bool isValid = false;

	game_data_object* gdo = gdo_get_current();
	if (gdo == nullptr) feedbackGdo._data.error_flags = COMMAND_ERROR_UNSPECIFIED;
	else isValid = game_client_query_command_valid(*WEB_CLIENT.backend, (ui8)GDO_COMMAND_TYPE, gdo->get_data(), &feedbackGdo._data);

	ia_memcpy(FEEDBACK_BUFFER, &feedbackGdo, sizeof(gdo_match_command_validity_feedback));
	return isValid;
}
