// win32_main.cpp : Defines the entry point for the Game Server application on the Win32 platform.
// Unity-compile with the server code.
#include "../../game_server/game_server_main.cpp"

// Unity-compile the rest of the platform code.
#include "win32_game_server_net.cpp"

// Include standard library stuff.
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static const SIZE_T GAME_SERVER_MEM_SIZE = GiB(4);

// Shared global application state available in WIN32_APP_STATE.
struct win32_app_state
{
	// Handle to output console.
	HANDLE consoleHandle;

	// (Forward-declared) Pointer to game server structure.
	game_server* gameServer;

	// Set to true when the application wants to cleanly exit.
	bool exitRequested;
};

win32_app_state WIN32_APP_STATE = {};

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

	win32_log_stderr(assert_msg_buff);

	memset(assert_msg_buff, 0, sizeof(assert_msg_buff));
	sprintf_s(assert_msg_buff, ASSERT_MSG_BUFF_COUNT, "FILE: %s, LINE %d", filename, line);

	win32_log_stderr(assert_msg_buff);

	abort();
}

// Immediately call the standard abort() function so debuggers can break here.
void ASSERT_EXIT_FUNC()
{
	abort();
}

void win32_shutdown(int code)
{
	WIN32_APP_STATE.exitRequested = true;
}

void win32_log_stdout(const char* msg)
{
	fputs(msg, stdout);
	fputc('\n', stdout);
}

void win32_logf_stdout(const char* msg, ...)
{
	static const ui32 LOG_FORMAT_BUFF_SIZE = 1024;

	char log_msg_buff[LOG_FORMAT_BUFF_SIZE];
	memset(log_msg_buff, 0, sizeof(log_msg_buff));

	va_list va;
	va_start(va, msg);
	int charCount = vsprintf_s(log_msg_buff, LOG_FORMAT_BUFF_SIZE, msg, va);
	va_end(va);

	win32_log_stdout(log_msg_buff);
}

void win32_log_stderr(const char* msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
}

// The "server ressources" folder is currently just the working directory.
ui64 win32_read_file(const char* filename, ui8* read_buff, ui64 buff_size)
{
	// TO BE IMPLEMENTED.
	return 0;
}

// The "server resources" folder is currently the working directory.
bool win32_write_file(const char* filename, const ui8* data, ui64 size)
{
	FILE* file = nullptr;
	if (fopen_s(&file, filename, "wb") != 0 || file == nullptr)
	{
		return false;
	}

	size_t written = fwrite(data, 1, size, file);
	fclose(file);

	return written == size;
}

// Main entry point.
int main(int argc, char** argv)
{
	win32_log_stdout("Initializing IronAge.io Game Server.\nPlatform = Win32 x64\n");

	// Get handle to console for the logging functions.
	// TODO(Marc): Prepare for more complex, flexible logging to other outputs.

	// Initialize win32 platform & platform interface structure.

	win32_start_net(); // Start networking capabilities.

	game_server_platform win32_platform = {

		.shutdown = exit,

		.log_stdout = win32_log_stdout,
		.logf_stdout = win32_logf_stdout,
		.log_stderr = win32_log_stderr,

		.net_query_new_connections = win32_net_query_new_connections,
		.net_query_closed_connections = win32_net_query_closed_connections,
		.net_send_bytes = win32_net_send_bytes,
		.net_receive_bytes = win32_net_receive_bytes,
		.net_close_connection = win32_net_close_connection,

		.read_file = win32_read_file,
		.write_file = win32_write_file
	};

	// ... TODO(Marc) Many more platform functions / properties to add !
	
	win32_logf_stdout("Allocating Game Server memory. Memory size = %llu bytes", GAME_SERVER_MEM_SIZE);
	ui8* game_server_mem = (ui8*)VirtualAlloc(NULL, GAME_SERVER_MEM_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	if (game_server_mem == nullptr)
	{
		DWORD errCode = GetLastError();
		ASSERT_MSG(0, "Failed to allocate Game Server memory. Error code = %d", errCode);
	}

	// TODO(Marc): Read command line / config file for those !
	game_server_init_params server_init_params = {

		.match_slot_count = 4,
		.run_test_scenario = false,
		.test_scenario_dump_filename = "snapshot_native.bin"
	};

	win32_logf_stdout("Initializing Game Server...\n");

	WIN32_APP_STATE.gameServer = game_server_init(win32_platform, server_init_params, game_server_mem, GAME_SERVER_MEM_SIZE);
	ASSERT_MSG(WIN32_APP_STATE.gameServer != nullptr, "Failed to initialize Game Server.");

	win32_logf_stdout("\nGame Server initialized.");

	win32_logf_stdout("Starting main tick loop.\n");

	// Main loop: measure the time elapsed since the previous iteration and hand it to the server.
	LARGE_INTEGER counter_frequency;
	QueryPerformanceFrequency(&counter_frequency);

	LARGE_INTEGER current_counter;
	QueryPerformanceCounter(&current_counter);

	ui64 start_ms = (current_counter.QuadPart * 1000 / counter_frequency.QuadPart);

	while (!WIN32_APP_STATE.exitRequested)
	{
		QueryPerformanceCounter(&current_counter);

		// Measure time since game server initialization in milliseconds.
		ui64 uptime_ms = (current_counter.QuadPart * 1000 / counter_frequency.QuadPart) - start_ms;

		game_server_tick(*WIN32_APP_STATE.gameServer, uptime_ms);
	}

	win32_logf_stdout("Win32 platform shutting down...");

	win32_stop_net();

	return 0;
}
