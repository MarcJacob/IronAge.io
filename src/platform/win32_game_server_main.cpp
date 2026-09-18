// win32_main.cpp : Defines the entry point for the Game Server application on the Win32 platform.

#include "core.h"

// Unity-compile with the server code.
#include "../game_server/game_server_main.cpp"

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static const SIZE_T GAME_SERVER_MEM_SIZE = GiB(4);

struct win32_app_state
{
	// Handle to output console.
	HANDLE consoleHandle;

	// (Undefined) Pointer to game server structure.
	game_server* gameServer;

	// Set to true when the application wants to cleanly exit.
	bool exitRequested;

} APP_STATE;

// Game Server platform functions.
void win32_log_stdout(const wchar_t* msg);
void win32_log_stderr(const wchar_t* msg);

// Assertion functions.

// Format the assertion message in a fixed stack buffer and go through the Win32 _wassert function. 
void ASSERT_MSG_FUNC(const wchar_t* msg, const wchar_t* filename, ui32 line, ...)
{
	static const ui32 ASSERT_MSG_BUFF_COUNT = 1024;

	wchar_t assert_msg_buff[ASSERT_MSG_BUFF_COUNT];
	memset(assert_msg_buff, 0, sizeof(assert_msg_buff));

	va_list va;
	va_start(va, line);
	int charCount = vswprintf_s(assert_msg_buff, ASSERT_MSG_BUFF_COUNT, msg, va);
	va_end(va);

	win32_log_stderr(assert_msg_buff);

	memset(assert_msg_buff, 0, sizeof(assert_msg_buff));
	swprintf_s(assert_msg_buff, ASSERT_MSG_BUFF_COUNT, L"FILE: %s, LINE %d", filename, line);

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
	APP_STATE.exitRequested = true;
}

void win32_log_stdout(const wchar_t* msg)
{
	fputws(msg, stdout);
	fputwc('\n', stdout);
}

void win32_logf_stdout(const wchar_t* msg, ...)
{
	static const ui32 LOG_FORMAT_BUFF_SIZE = 1024;

	wchar_t log_msg_buff[LOG_FORMAT_BUFF_SIZE];
	memset(log_msg_buff, 0, sizeof(log_msg_buff));

	va_list va;
	va_start(va, msg);
	int charCount = vswprintf_s(log_msg_buff, LOG_FORMAT_BUFF_SIZE, msg, va);
	va_end(va);

	win32_log_stdout(log_msg_buff);
}

void win32_log_stderr(const wchar_t* msg)
{
	fputws(msg, stderr);
	fputwc('\n', stderr);
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
	win32_log_stdout(L"Initializing IronAge.io Game Server.\nPlatform = Win32 x64\n\n");

	// Get handle to console for the logging functions.
	// TODO(Marc): Prepare for more complex, flexible logging to other outputs.

	// Initialize platform.
	game_server_platform win32_platform = game_server_platform{

		.shutdown = exit,

		.log_stdout = win32_log_stdout,
		.logf_stdout = win32_logf_stdout,
		.log_stderr = win32_log_stderr,

		.write_file = win32_write_file
	};

	// ... TODO(Marc) Many more platform functions / properties to add !
	
	ui8* game_server_mem = (ui8*)VirtualAlloc(NULL, GAME_SERVER_MEM_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	if (game_server_mem == nullptr)
	{
		DWORD errCode = GetLastError();
		ASSERT_MSG(0, L"Failed to allocate Game Server memory. Error code = %d", errCode);
	}

	APP_STATE.gameServer = game_server_init(win32_platform, game_server_mem, GAME_SERVER_MEM_SIZE);
	ASSERT_MSG(APP_STATE.gameServer != nullptr, L"Failed to initialize Game Server.");

	// Main loop: measure the time elapsed since the previous iteration and hand it to the server.
	LARGE_INTEGER counter_frequency;
	QueryPerformanceFrequency(&counter_frequency);

	LARGE_INTEGER last_counter;
	QueryPerformanceCounter(&last_counter);

	while (!APP_STATE.exitRequested)
	{
		LARGE_INTEGER current_counter;
		QueryPerformanceCounter(&current_counter);

		// Integer difference first, converted to seconds last, to limit precision loss.
		float deltatime = (float)(current_counter.QuadPart - last_counter.QuadPart) / (float)counter_frequency.QuadPart;
		last_counter = current_counter;

		game_server_tick(win32_platform, *APP_STATE.gameServer, deltatime);
	}

	return 0;
}
