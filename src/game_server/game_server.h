#ifndef GAME_SERVER_INCLUDED
#define GAME_SERVER_INCLUDED

#include "core.h"
#include "game_common/game_match.h"

// Main symbols file for the game server implementation.
// Defines the actual game server structure and internals.

struct game_match;
struct http_server;

// States a match slot can be in.
// Lifecycle goes Uninitialized -> Waiting -> In Lobby -> Match Ongoing -> Match Ended -> Awaiting Cleanup -> Waiting -> [...]
enum class MATCH_SLOT_STATE : ui8
{
	UNINITIALIZED,		// Slot was just created and requires initialization.
	WAITING,			// Slot is ready to accept a new match.
	IN_LOBBY,			// Match has not started yet and is accepting new player connections.
	MATCH_ONGOING,		// Match is actively being played / ticked.
	MATCH_ENDED,		// Match has ended and is exposing its analytics data for other systems before cleanup.
	AWAITING_CLEANUP,	// Match has ended, and the slot can be re-used after cleanup.
};

// Data associated to a match slot when it is currently waiting or in lobby, accepting players joining the game and
// changes to the match parameters before it is started.
struct match_slot_lobby
{
	// ... hold player identifiers, associated to a connected client.
	ui8 to_implement;
};

// Wraps memory and a match structure that can be in multiple states.
// Allows the use and re-use of a same span of memory for a match's entire lifecycle: building the lobby, starting the match, ticking it...
struct match_slot
{
	MATCH_SLOT_STATE state;

	mem_arena slot_memory; // Memory assigned to this slot.

	game_match_start_params match_params; // Parameters for the current or next match (valid when in lobby or in a match).

	union
	{
		match_slot_lobby* lobby;	// Valid when the slot state is non MATCH_*

		struct
		{
			game_match* match_ptr;
			ui64 last_tick_time;

		} match;					// Valid when the slot state is MATCH_*
	};
};

struct game_server_init_params
{
	ui8 match_slot_count;

	// Test mode parameters.
	bool run_test_scenario; // If set to true, the server will start, run a match scenario on its first tick, dump it to a specific file then shutdown.
	const char* test_scenario_dump_filename; // If set to run test scenario, this indicates what file to dump the match data into once done.

	const char* web_root; // Folder holding the web client bundle to serve over HTTP, relative to the platform resources folder.
	const char* const* web_files; // Names of the files to serve over HTTP, relative to web_root.
	ui32 web_file_count;
};

struct game_server
{
	game_server_platform* platform; // Host platform functionality this server is running on.

	game_server_init_params init_params;

	bool shutdown_triggered; // Should the server shutdown as soon as possible ?
	ui64 tick_count; // How many ticks this server has gone through in total.

	time_ms time_ms; // Last recorded time from tick.

	mem_arena main_memory; // Main memory allocator for the server.
	match_slot* match_slots; // Match slots management structures.

	http_server* http; // Serves the web client bundle to connections.

	// LOGGING
	// Redirects to the platform, prepending "GAME SERVER (<component>): " to the message, or just "GAME SERVER: " if component is empty.
	// The component name must not contain '%' as it becomes part of the format string for logf. Messages beyond LOG_BUFF_SIZE are truncated.
	// NOTE(Marc): This is a little beefier than what I envisionned originally. I don't want the server structure to become more monolithic than it needs to be...
	// but typing "server.log" is just so convenient. Maaaaaaaaybe I'll replace it with an external game_server_log function or something.

	static constexpr ui32 LOG_BUFF_SIZE = 1024;

	inline void log(const char* component, LOG_TYPE type, const char* msg)
	{
		char buff[LOG_BUFF_SIZE];
		ui32 count = 0;
		ia_str_prefix(buff, LOG_BUFF_SIZE - 1, count, "GAME SERVER", component);
		ia_str_append(buff, LOG_BUFF_SIZE - 1, count, msg);
		buff[count] = '\0';

		platform->log(type, buff);
	}
	inline void log(const char* component, const char* msg) { log(component, LOG_NORMAL, msg); }
	// No component name.
	inline void log(LOG_TYPE type, const char* msg) { log("", type, msg); }
	inline void log(const char* msg) { log("", LOG_NORMAL, msg); }

	template<typename... args_types>
	inline void logf(const char* component, LOG_TYPE type, const char* format, args_types... args)
	{
		char buff[LOG_BUFF_SIZE];
		ui32 count = 0;
		ia_str_prefix(buff, LOG_BUFF_SIZE - 1, count, "GAME SERVER", component);
		ia_str_append(buff, LOG_BUFF_SIZE - 1, count, format);
		buff[count] = '\0';

		platform->logf(type, buff, args...);
	}
	template<typename... args_types>
	inline void logf(const char* component, const char* format, args_types... args) { logf(component, LOG_NORMAL, format, args...); }
	// No component name.
	// NOTE: logf("format", "string arg") is indistinguishable from logf("component", "format"), and the component overload wins.
	// If the first format argument is a string, use the explicit "" component (or a type) instead.
	template<typename... args_types>
	inline void logf(LOG_TYPE type, const char* format, args_types... args) { logf("", type, format, args...); }
	template<typename... args_types>
	inline void logf(const char* format, args_types... args) { logf("", LOG_NORMAL, format, args...); }
};

#endif // GAME_SERVER_INCLUDED
