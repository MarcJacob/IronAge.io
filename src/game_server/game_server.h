// Declarations for symbols used across the game server.

#ifndef GAME_SERVER_INCLUDED
#define GAME_SERVER_INCLUDED

#include "core.h"
#include "game_common/game_match.h"

// Main symbols file for the game server implementation.
// Defines the actual game server structure and internals.

// Forward-declare game server components.
struct http_server;

struct match_slot;
struct game_server_clients_table;

struct game_server_init_params
{
	ui8 match_slot_count;
	ui16 max_client_count;

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

	game_server_clients_table* client_table;

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
