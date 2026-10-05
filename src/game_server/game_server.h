// Declarations for symbols used across the game server.

#ifndef GAME_SERVER_INCLUDED
#define GAME_SERVER_INCLUDED

#include "core.h"
#include "game_server/game_server_init_params.h"
#include "game_server/game_server_resources.h"

#include "game_server/game_server_platform.h" // @TODO(Marc): Remove as soon as possible ! The logging needs to move to external functions, as predicted in an earlier comment...

// Main symbols file for the game server implementation.
// Defines the actual game server structure and internals.

struct game_server_platform;

// Forward-declare game server components.
struct web_server;
struct match_slot;
struct game_server_clients_table;

struct game_server
{
	game_server_platform* platform; // Host platform functionality this server is running on.

	game_server_init_params init_params;
	mem_arena main_memory; // Main memory allocator for the server.

	game_server_resource_path* resource_files;
	ui16 resource_file_count;

	bool shutdown_triggered; // Should the server shutdown as soon as possible ?
	ui64 tick_count; // How many ticks this server has gone through in total.

	time_ms uptime_ms; // Last recorded time from tick.
	time_ms delta_ms; // Delta between current and previous uptime.

	game_server_clients_table* client_table;

	match_slot* match_slots; // Match slots management structures.

	web_server* web; // Serves the web client bundle over http, and manages websocket connections.

	// LOGGING
	// Redirects to the platform, prepending "GAME SERVER (<component>): " to the message, or just "GAME SERVER: " if component is empty.
	// The component name must not contain '%' as it becomes part of the format string for logf. Messages beyond LOG_BUFF_SIZE are truncated.
	// NOTE(Marc): This is a little beefier than what I envisionned originally. I don't want the server structure to become more monolithic than it needs to be...
	// but typing "server.log" is just so convenient. Maaaaaaaaybe I'll replace it with an external game_server_log function or something.

	static constexpr ui32 LOG_BUFF_SIZE = 1024;

	inline void log(const ia_string_view& component, LOG_TYPE type, const ia_string_view& msg)
	{
		static_mem_arena<LOG_BUFF_SIZE> logMem;
		ia_string_builder logBuilder(&logMem);

		logBuilder.push_back("GAME SERVER");
		if (!component.is_empty()) logBuilder.push_back_format(" (%s)", component);
		logBuilder.push_back(": ");
		logBuilder.push_back(msg);

		platform->log(type, logBuilder.string);
	}
	inline void log(const ia_string_view& component, const ia_string_view& msg) { log(component, LOG_NORMAL, msg); }
	// No component name.
	inline void log(LOG_TYPE type, const ia_string_view& msg) { log("", type, msg); }
	inline void log(const ia_string_view& msg) { log("", LOG_NORMAL, msg); }

	template<typename... args_types>
	inline void logf(const ia_string_view& component, LOG_TYPE type, const ia_string_view& format, args_types... args)
	{
		static_mem_arena<LOG_BUFF_SIZE> logMem;
		ia_string_builder logBuilder(&logMem);

		logBuilder.push_back("GAME SERVER");
		if (!component.is_empty()) logBuilder.push_back_format(" (%s)", component);
		logBuilder.push_back(": ");
		logBuilder.push_back_format(format, args...);

		platform->log(type, logBuilder.string);
	}

	template<typename... args_types>
	inline void logf(LOG_TYPE type, const ia_string_view& format, args_types... args) { logf("", type, format, args...); }
};

#endif // GAME_SERVER_INCLUDED
