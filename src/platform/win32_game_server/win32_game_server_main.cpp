// win32_main.cpp : Defines the entry point for the Game Server application on the Win32 platform.
#include "win32_game_server_platform.h"

// Unity-compile with the server code.
#include "../../game_server/game_server_main.cpp"

// Unity-compile the rest of the platform code.
#include "win32_game_server_net.cpp"

// Include standard library stuff.
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>

static constexpr ui64 GAME_SERVER_MEM_SIZE = GiB(4);

// Assertion functions.

// Format the assertion message in a fixed stack buffer and go through the Win32 assert function. 
void ASSERT_MSG_FUNC(const char* msg, const char* filename, ui32 line, ...)
{
	static const ui32 ASSERT_MSG_BUFF_COUNT = 1024;

	char assert_msg_buff[ASSERT_MSG_BUFF_COUNT];
	memset(assert_msg_buff, 0, sizeof(assert_msg_buff));

	va_list va;
	va_start(va, line);
	int charCount = vsprintf_s(assert_msg_buff, ASSERT_MSG_BUFF_COUNT, msg, va);
	va_end(va);

	win32_log("ASSERT", LOG_ERROR, assert_msg_buff);

	memset(assert_msg_buff, 0, sizeof(assert_msg_buff));
	sprintf_s(assert_msg_buff, ASSERT_MSG_BUFF_COUNT, "FILE: %s, LINE %d", filename, line);

	win32_log("ASSERT", LOG_ERROR, assert_msg_buff);

	__debugbreak();
	raise(SIGABRT);
}

// Break into the debugger right here if one is attached, then raise SIGABRT.
void ASSERT_EXIT_FUNC()
{
	__debugbreak();
	raise(SIGABRT);
}

void win32_shutdown(game_server_platform& platform, int code)
{
	auto& win32Platform = (win32_platform&)platform;
	win32Platform.app.exitRequested = true;
}

static constexpr ui32 WIN32_LOG_BUFF_SIZE = 1024;

// Console color for a log type. 0 = leave the console's current color.
static WORD win32_log_color(LOG_TYPE type)
{
	switch (type)
	{
	case LOG_SUCCESS:
		return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	case LOG_WARNING:
		return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	case LOG_ERROR:
		return FOREGROUND_RED | FOREGROUND_INTENSITY;
	default:
		return 0;
	}
}

// Prints the buffer and a newline to the output in the type's color, then restores whatever the console attributes were.
// The color calls fail harmlessly if the output is redirected.
static void win32_print_colored(FILE* output, HANDLE outHandle, LOG_TYPE type, const char* buffer)
{
	WORD color = win32_log_color(type);

	CONSOLE_SCREEN_BUFFER_INFO consoleInfo = {};
	bool colored = color != 0
		&& GetConsoleScreenBufferInfo(outHandle, &consoleInfo)
		&& SetConsoleTextAttribute(outHandle, color);

	fputs(buffer, output);
	fputc('\n', output);
	fflush(output);

	if (colored)
	{
		SetConsoleTextAttribute(outHandle, consoleInfo.wAttributes);
	}
}

CRITICAL_SECTION CS_WIN32_STDOUT;
void win32_stdout(LOG_TYPE type, const char* buffer)
{
	EnterCriticalSection(&CS_WIN32_STDOUT);
	win32_print_colored(stdout, GetStdHandle(STD_OUTPUT_HANDLE), type, buffer);
	LeaveCriticalSection(&CS_WIN32_STDOUT);
}

CRITICAL_SECTION CS_WIN32_STDERR;
void win32_stderr(LOG_TYPE type, const char* buffer)
{
	EnterCriticalSection(&CS_WIN32_STDERR);
	win32_print_colored(stderr, GetStdHandle(STD_ERROR_HANDLE), type, buffer);
	LeaveCriticalSection(&CS_WIN32_STDERR);
}

// Sends a finished buffer to the end point matching its type.
static void win32_route_log(LOG_TYPE type, const char* buffer)
{
	if (type == LOG_ERROR)
	{
		win32_stderr(type, buffer);
	}
	else
	{
		win32_stdout(type, buffer);
	}
}

void win32_log(const char* component, LOG_TYPE type, const char* msg)
{
	char logBuff[WIN32_LOG_BUFF_SIZE + 64];

	if (component != nullptr && component[0] != '\0')
	{
		_snprintf_s(logBuff, sizeof(logBuff), _TRUNCATE, "WIN32 (%s): %s", component, msg);
	}
	else
	{
		_snprintf_s(logBuff, sizeof(logBuff), _TRUNCATE, "WIN32: %s", msg);
	}

	win32_route_log(type, logBuff);
}

void win32_logf(const char* component, LOG_TYPE type, const char* format, ...)
{
	char msgBuff[WIN32_LOG_BUFF_SIZE];

	va_list va;
	va_start(va, format);
	_vsnprintf_s(msgBuff, sizeof(msgBuff), _TRUNCATE, format, va);
	va_end(va);

	win32_log(component, type, msgBuff);
}

void win32_logf(const char* component, const char* format, ...)
{
	char msgBuff[WIN32_LOG_BUFF_SIZE];

	va_list va;
	va_start(va, format);
	_vsnprintf_s(msgBuff, sizeof(msgBuff), _TRUNCATE, format, va);
	va_end(va);

	win32_log(component, LOG_NORMAL, msgBuff);
}

void win32_platform_log(game_server_platform& platform, LOG_TYPE type, const char* msg)
{
	win32_route_log(type, msg);
}

void win32_platform_logf(game_server_platform& platform, LOG_TYPE type, const char* msg, ...)
{
	char msgBuff[WIN32_LOG_BUFF_SIZE];

	va_list va;
	va_start(va, msg);
	_vsnprintf_s(msgBuff, sizeof(msgBuff), _TRUNCATE, msg, va);
	va_end(va);

	win32_route_log(type, msgBuff);
}

// The "server resources" folder is GAME_SERVER_RESOURCES_DIR, defined by the build (see CMakeLists.txt).
// Builds <resources dir>/<filename> in out_path. Returns false if it doesn't fit.
static bool win32_resource_path(const char* filename, char* out_path, size_t out_size)
{
	return sprintf_s(out_path, out_size, "%s/%s", GAME_SERVER_RESOURCES_DIR, filename) > 0;
}

ui64 win32_read_file(game_server_platform& platform, const char* filename, ui8* read_buff, ui64 buff_size)
{
	char path[MAX_PATH];
	if (!win32_resource_path(filename, path, sizeof(path)))
	{
		return 0;
	}

	FILE* file = nullptr;
	if (fopen_s(&file, path, "rb") != 0 || file == nullptr)
	{
		return 0;
	}

	_fseeki64(file, 0, SEEK_END);
	i64 fileSize = _ftelli64(file);
	_fseeki64(file, 0, SEEK_SET);

	// Empty / unreadable file, or a "dry run" asking only for the size, or a buffer that's too small.
	if (fileSize <= 0 || read_buff == nullptr || buff_size < (ui64)fileSize)
	{
		fclose(file);
		return read_buff == nullptr && fileSize > 0 ? (ui64)fileSize : 0;
	}

	size_t readCount = fread(read_buff, 1, (size_t)fileSize, file);
	fclose(file);

	return readCount == (size_t)fileSize ? (ui64)fileSize : 0;
}

bool win32_write_file(game_server_platform& platform, const char* filename, const ui8* data, ui64 size)
{
	char path[MAX_PATH];
	if (!win32_resource_path(filename, path, sizeof(path)))
	{
		return false;
	}

	FILE* file = nullptr;
	if (fopen_s(&file, path, "wb") != 0 || file == nullptr)
	{
		return false;
	}

	size_t written = fwrite(data, 1, size, file);
	fclose(file);

	return written == size;
}

// BEGIN PROGRAM ENTRY

static win32_platform WIN32_PLATFORM;  // Static memory storage of the main platform object.

// Program / Console signal handler.
static BOOL WINAPI win32_console_ctrl_handler(DWORD ctrl_type)
{
      switch (ctrl_type)
      {
      case CTRL_C_EVENT:        // Ctrl+C
      case CTRL_BREAK_EVENT:    // Ctrl+Break
      case CTRL_CLOSE_EVENT:    // Console window closed
      case CTRL_LOGOFF_EVENT:   // User logging off (services only, mostly)
      case CTRL_SHUTDOWN_EVENT: // System shutting down
		  
		  if (WIN32_PLATFORM.initialized)
		  {
			  win32_log("SIGNAL", LOG_WARNING, "!! SHUTDOWN SIGNAL RECEIVED !!");
			  WIN32_PLATFORM.shutdown(1);
			return TRUE;
		  }
      }
      return FALSE;
}

void win32_platform_init()
{
	ASSERT(WIN32_PLATFORM.initialized == false);

	WIN32_PLATFORM.shutdown_func = win32_shutdown,

	WIN32_PLATFORM.log_func = win32_platform_log;
	WIN32_PLATFORM.logf_func = win32_platform_logf;

	WIN32_PLATFORM.net_query_new_connections_func = win32_net_query_new_connections;
	WIN32_PLATFORM.net_query_closed_connections_func = win32_net_query_closed_connections;
	WIN32_PLATFORM.net_send_bytes_func = win32_net_send_bytes;
	WIN32_PLATFORM.net_receive_bytes_func = win32_net_receive_bytes;
	WIN32_PLATFORM.net_close_connection_func = win32_net_close_connection;

	WIN32_PLATFORM.read_resource_file_func = win32_read_file;
	WIN32_PLATFORM.write_resource_file_func = win32_write_file;

	WIN32_PLATFORM.net_component = win32_net_start(); // Start networking capabilities.
	ASSERT_MSG(WIN32_PLATFORM.net_component != nullptr, "Win32: Failed to start Net Component.");

	// ... TODO(Marc) Many more platform functions / properties to add !

	WIN32_PLATFORM.initialized = true;
}

void win32_platform_shutdown()
{
	ASSERT(WIN32_PLATFORM.initialized);

	if (WIN32_PLATFORM.app.gameServer != nullptr)
	{
		win32_log("", "Shutting down Game Server.");
		game_server_stop(*WIN32_PLATFORM.app.gameServer);
	}

	win32_log("", "Platform shutting down...");

	win32_net_stop(*WIN32_PLATFORM.net_component);
	win32_net_free(*WIN32_PLATFORM.net_component);

	// Delete logging critical sections.
	DeleteCriticalSection(&CS_WIN32_STDOUT);
	DeleteCriticalSection(&CS_WIN32_STDERR);

	win32_log("", LOG_SUCCESS, "Platform shutdown complete.");

	WIN32_PLATFORM = {};
}

// Main entry point.
int main(int argc, char** argv)
{
	// Register console signal handling.
	if (!SetConsoleCtrlHandler(win32_console_ctrl_handler, TRUE))
	{
		win32_logf("", LOG_WARNING, "Failed to register console control handler. Error code = %d", GetLastError());
	}

	// Init logging critical sections.
	InitializeCriticalSection(&CS_WIN32_STDOUT);
	InitializeCriticalSection(&CS_WIN32_STDERR);

	win32_log("", "Initializing IronAge.io Game Server.\nPlatform = Win32 x64\n");

	// Initialize win32 platform structure.

	win32_logf("", "Allocating Game Server memory. Memory size = %llu bytes", GAME_SERVER_MEM_SIZE);

	win32_platform_init();
	ASSERT(WIN32_PLATFORM.initialized);

	ui8* game_server_mem = (ui8*)VirtualAlloc(NULL, GAME_SERVER_MEM_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	ASSERT_MSG(game_server_mem != nullptr, "Failed to allocate Game Server memory. Error code = %d", GetLastError());

	// Files of the web client bundle the game server will preload and serve over HTTP, relative to the web root.
	// TODO(Marc): Add a platform call to list available files in resources folder, so the server can just discover all available files.
	static const char* const WEB_FILES[] = {

		// Core JS
		"index.html",
		"style.css",
		"src/main.js",
		"src/backend.js",
		"src/render.js",
		"src/input.js",
		"src/websocket_tests.js",

		// Client WASM backend
		"IronAgeIO_WebClient.wasm",

		// Art
		"favicon.ico",
	};
	static constexpr ui8 WEB_FILE_COUNT = sizeof(WEB_FILES) / sizeof(WEB_FILES[0]);

	// TODO(Marc): Read command line / config file for those !
	game_server_init_params server_init_params = {

		.match_slot_count = 4,
		.max_client_count = 1024,
		.run_test_scenario = false,
		.test_scenario_dump_filename = "snapshot_native.bin",

		.web_root = "web_root",
		.web_files = WEB_FILES,
		.web_file_count = WEB_FILE_COUNT,
	};

	win32_logf("", "Initializing Game Server...\n");

	WIN32_PLATFORM.app.gameServer = game_server_init(WIN32_PLATFORM, server_init_params, game_server_mem, GAME_SERVER_MEM_SIZE);
	if (WIN32_PLATFORM.app.gameServer == nullptr)
	{
		win32_log("", LOG_TYPE::LOG_ERROR, "Failed to initialize Game Server. Aborting...");
		win32_platform_shutdown();
		return 1;
	}

	win32_log("", LOG_SUCCESS, "Game Server initialized.");

	win32_log("", "Starting main tick loop.\n");

	// Main loop: measure the time elapsed since the previous iteration and hand it to the server.
	LARGE_INTEGER counter_frequency;
	QueryPerformanceFrequency(&counter_frequency);

	LARGE_INTEGER current_counter;
	QueryPerformanceCounter(&current_counter);

	ui64 start_ms = (current_counter.QuadPart * 1000 / counter_frequency.QuadPart);

	while (!WIN32_PLATFORM.app.exitRequested)
	{
		// Run main update of Net component.
		if (WIN32_PLATFORM.net_component->active)
		{
			win32_net_update_connections(*WIN32_PLATFORM.net_component); // TEMP(Marc): For now we just do this on the main thread. Later we may want a "net master thread" that does this on its own.
		}
		else
		{
			win32_log("", LOG_ERROR, "Net Component has stopped unexpectedly. Shutting down.");
			win32_platform_shutdown();
			return 1;
		}

		// Query uptime and run game server main tick function.
		QueryPerformanceCounter(&current_counter);

		// Measure time since game server initialization in milliseconds.
		ui64 time_ms = (current_counter.QuadPart * 1000 / counter_frequency.QuadPart) - start_ms;

		game_server_tick(*WIN32_PLATFORM.app.gameServer, time_ms);
	}

	win32_platform_shutdown();
	return 0;
}
