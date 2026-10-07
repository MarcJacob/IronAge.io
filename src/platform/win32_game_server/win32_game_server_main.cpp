// win32_main.cpp : Defines the entry point for the Game Server application on the Win32 platform.

// Include Win32 common headers.
#include "core/assert.h"
#include "core/memory.h"
#include "core/string.h"
#include "game_server/game_server_resources.h"
#include "win32_game_server_platform.h"

// Include Game Server abstract platform.
#include "game_server/game_server_platform.h"

// Unity-compile the rest of the platform code.
#include "win32_game_server_net.cpp"

// Include standard library stuff.
#include <minwindef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

static constexpr ui64 GAME_SERVER_MEM_SIZE = GiB(4);
static ia_string_view GAME_SERVER_RESOURCES_DIR = {};   // Filled in from first parameter. 
                                                        // @TODO(Marc): Just a temporary adaptation,
                                                        // CMake was providing a full absolute path I'd rather not put in a script,
                                                        // and it doesn't handle being a relative path very well right now.

// Assertion functions.

// Format the assertion message in a fixed stack buffer and go through the Win32 assert function. 
void ASSERT_MSG_FUNC(const char* msg, const char* filename, ui32 line, ...)
{
	static const ui32 ASSERT_MSG_BUFF_COUNT = 1024;

	char assert_msg_buff[ASSERT_MSG_BUFF_COUNT];
	ia_memset(assert_msg_buff, 0, sizeof(assert_msg_buff));

	va_list va;
	va_start(va, line);
	int charCount = vsprintf_s(assert_msg_buff, ASSERT_MSG_BUFF_COUNT, msg, va);
	va_end(va);

	win32_log("ASSERT", LOG_ERROR, assert_msg_buff);
	OutputDebugString(assert_msg_buff);

	ia_memset(assert_msg_buff, 0, sizeof(assert_msg_buff));
	sprintf_s(assert_msg_buff, ASSERT_MSG_BUFF_COUNT, "FILE: %s, LINE %d", filename, line);

	win32_log("ASSERT", LOG_ERROR, assert_msg_buff);
	OutputDebugString(assert_msg_buff);

	raise(SIGABRT);
}

// Break into the debugger right here if one is attached, then raise SIGABRT.
void ASSERT_EXIT_FUNC()
{
    OutputDebugString("Assertion triggered. Ending program...");
	raise(SIGABRT);
}

// Implementation of global assertion handler symbol declared in assert.h.
_ASSERTION_HANDLER* _ASSERTION_HANDLER_PTR = nullptr;

void win32_shutdown(game_server_platform& platform, int code)
{
	auto& win32Platform = (win32_platform&)platform;
	win32Platform.exitRequested = true;
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

void win32_stdout(LOG_TYPE type, const ia_string_view& string)
{
	EnterCriticalSection(&CS_WIN32_STDOUT);

	char printScratch[256];
	ui32 index = 0;
	while (index < string.length)
	{
		ui32 copySize = ia_min(string.length - index, sizeof(printScratch) - 1);
		memcpy(printScratch, string.view_str + index, copySize);
		index += copySize;

		printScratch[copySize] = '\0';
		win32_print_colored(stdout, GetStdHandle(STD_OUTPUT_HANDLE), type, printScratch);
	}

	LeaveCriticalSection(&CS_WIN32_STDOUT);
}


CRITICAL_SECTION CS_WIN32_STDERR;
void win32_stderr(LOG_TYPE type, const char* buffer)
{
	EnterCriticalSection(&CS_WIN32_STDERR);
	win32_print_colored(stderr, GetStdHandle(STD_ERROR_HANDLE), type, buffer);
	LeaveCriticalSection(&CS_WIN32_STDERR);
}

void win32_stderr(LOG_TYPE type, const ia_string_view& string)
{
	EnterCriticalSection(&CS_WIN32_STDERR);

	char printScratch[256];
	ui32 index = 0;
	while (index < string.length)
	{
		ui32 copySize = ia_min(string.length - index, sizeof(printScratch) - 1);
		memcpy(printScratch, string.view_str + index, copySize);
		index += copySize;

		printScratch[copySize] = '\0';
		win32_print_colored(stderr, GetStdHandle(STD_ERROR_HANDLE), type, printScratch);
	}

	LeaveCriticalSection(&CS_WIN32_STDERR);
}

// Sends a finished buffer to the end point matching its type.
static void win32_route_log(LOG_TYPE type, const ia_string_view& string)
{
	if (type == LOG_ERROR)
	{
		win32_stderr(type, string);
	}
	else
	{
		win32_stdout(type, string);
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

void win32_platform_log(game_server_platform& platform, LOG_TYPE type, const ia_string_view& msg)
{
	win32_route_log(type, msg);
}

void win32_platform_logf(game_server_platform& platform, LOG_TYPE type, const ia_string_view& format, ...)
{
	va_list va;
	va_start(va, format);

	static_mem_arena<WIN32_LOG_BUFF_SIZE> formatMem;
	ia_string msg = ia_string_format_v(format, &formatMem, va);

	va_end(va);

	win32_route_log(type, msg);
}

// The "server resources" folder is GAME_SERVER_RESOURCES_DIR, defined by the build (see CMakeLists.txt).
// Builds <resources dir>/<rel_path> into write_mem, followed by a null terminator right after the string's last character (not counted in its length),
// so the result can be handed to platform calls as a C string through its _str.
// Returns an empty string if the path and its terminator don't fit.
static ia_string win32_resource_path(const ia_string_view& rel_path, mem_arena& write_mem)
{
	// sizeof(DIR) counts the terminator, which stands in for the '/' after the folder. The terminator of the whole path then needs 1 more byte.
	if (GAME_SERVER_RESOURCES_DIR.length + rel_path.length >= write_mem.mem_size - write_mem.allocated_count) return {};

	ia_string_builder pathBuilder(&write_mem);
	pathBuilder.push_back(GAME_SERVER_RESOURCES_DIR);
	pathBuilder.push_back('/');
	pathBuilder.push_back(rel_path);

	pathBuilder.string._str[pathBuilder.string.length] = '\0';
	return pathBuilder.string;
}

// Strips the resource folder from a path, turning it from absolute to relative by replacing it with "./".
// Fatal if the path doesn't start with the resources folder, as the server must be able to ingest every resource file.
static void win32_strip_resources_path(game_server_resource_path& path)
{
	ASSERT_MSG(path.length >= GAME_SERVER_RESOURCES_DIR.length && ia_string_starts_with(path, GAME_SERVER_RESOURCES_DIR),
		"Resource file path \"%.*s\" is not located in the resources folder.", (int)path.length, path._str);

	ia_string_chop_right(path, GAME_SERVER_RESOURCES_DIR.length - 1);
	path._str[0] = '.';
	path._str[1] = '/';
}

ui64 win32_read_resource_file(game_server_platform& platform, const game_server_resource_path& path_relative, ui8* read_buff, ui64 buff_size)
{
	static_mem_arena<MAX_PATH + 1> pathMem;
	ia_string absolutePath = win32_resource_path(path_relative, pathMem);
	if (absolutePath.is_empty()) return 0; // Path too long.

	const char* path = absolutePath._str;

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

bool win32_write_resource_file(game_server_platform& platform, const game_server_resource_path& path_relative, const ui8* data, ui64 size)
{
	static_mem_arena<MAX_PATH + 1> pathMem;
	ia_string absolutePath = win32_resource_path(path_relative, pathMem);
	if (absolutePath.is_empty()) return false; // Path too long.

	const char* path = absolutePath._str;

	FILE* file = nullptr;
	if (fopen_s(&file, path, "wb") != 0 || file == nullptr)
	{
		return false;
	}

	size_t written = fwrite(data, 1, size, file);
	fclose(file);

	return written == size;
}

ui8 win32_list_files_recursive(const ia_string_view& search_path, game_server_resource_path* out_paths, ui8 start_index, ui8 max_index)
{
	ASSERT(out_paths != nullptr);

	if (start_index >= max_index) return 0;

	// A path too long to be listed is fatal, as the server must be able to ingest every resource file.
	ASSERT_MSG(search_path.length < GAME_SERVER_RESOURCE_PATH_MAX_LEN,
		"Resource search path \"%.*s\" is too long (max is %d).", (int)search_path.length, search_path.view_str, (int)GAME_SERVER_RESOURCE_PATH_MAX_LEN - 1);

	// Only accept paths that end with a wildcard.
	if (search_path.is_empty() || !ia_string_ends_with(search_path, "*"))
	{
		return 0;
	}

	char path[GAME_SERVER_RESOURCE_PATH_MAX_LEN];
	memcpy(path, search_path.view_str, search_path.length);
	path[search_path.length] = '\0';

	WIN32_FIND_DATAA findData = {};
	HANDLE findHandle = FindFirstFile(path, &findData);

	if (findHandle == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	// Copy search path into a buffer where the search character is stripped.
	path[search_path.length - 1] = '\0';

	ia_string_view folderPart(path, search_path.length - 1); // Search path without the wildcard.

	ui16 index = start_index;
	do
	{
		// Ignore self and up ref.
		if (findData.cFileName[0] == '.') continue;

		ia_string_view fileName(findData.cFileName);

		// Determine if the next file found is a directory or a file. Call recursively on sub-directories.
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			game_server_resource_path folderPath = {};
			ia_string_append(folderPath, folderPart, true);
			ia_string_append(folderPath, fileName, true);
			ia_string_append(folderPath, "/*", true);

			ASSERT_MSG(folderPath.length == folderPart.length + fileName.length + 2,
				"Resource folder \"%.*s\" in \"%.*s\" has a path too long for a resource file path.", (int)fileName.length, fileName.view_str, (int)folderPart.length, folderPart.view_str);

			ui8 writtenCount = win32_list_files_recursive(folderPath, out_paths, index, max_index);
			index += writtenCount;
		}
		else
		{
			// Make sure the path is pushed from the beginning of the string.
			out_paths[index].length = 0;
			ia_string_append(out_paths[index], folderPart, true);
			ia_string_append(out_paths[index], fileName, true);

			ASSERT_MSG(out_paths[index].length == folderPart.length + fileName.length,
				"Resource file \"%.*s\" in \"%.*s\" has a path too long for a resource file path.", (int)fileName.length, fileName.view_str, (int)folderPart.length, folderPart.view_str);

			index++;
		}
	} while (index < max_index && FindNextFile(findHandle, &findData));

	FindClose(findHandle);
	return index - start_index;
}

ui16 win32_list_resource_files(game_server_platform& platform, const game_server_resource_path& path_relative, game_server_resource_path* out_paths, ui8 max_path_count)
{
	// TODO(Marc): Support for long paths.

	ASSERT(out_paths != nullptr && max_path_count >= 1);

	static_mem_arena<MAX_PATH + 1> pathMem;
	ia_string absolutePath = win32_resource_path(path_relative, pathMem);
	if (absolutePath.is_empty()) return 0; // Path too long.

	ui16 writtenPaths = win32_list_files_recursive(absolutePath, out_paths, 0, max_path_count);

	// Strip resource path from every found file path.
	for (ui16 fileIndex = 0; fileIndex < writtenPaths; fileIndex++)
	{
		win32_strip_resources_path(out_paths[fileIndex]);
	}

	return writtenPaths;
}

// BEGIN PROGRAM ENTRY

static win32_platform WIN32_PLATFORM;  // Static memory storage of the main platform object.
static bool DEV_MODE = true; // When true at program init, will enable the required infrastructure 
                             // to allow quicker development cycles and dev commands on the game server.

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

	WIN32_PLATFORM.read_resource_file_func = win32_read_resource_file;
	WIN32_PLATFORM.write_resource_file_func = win32_write_resource_file;
	WIN32_PLATFORM.list_resource_files_func = win32_list_resource_files;

	WIN32_PLATFORM.net_component = win32_net_start(); // Start networking capabilities.
	ASSERT_MSG(WIN32_PLATFORM.net_component != nullptr, "Win32: Failed to start Net Component.");

	// ... TODO(Marc) Many more platform functions / properties to add !

	WIN32_PLATFORM.initialized = true;
}

// GAME SERVER PROGRAM MANAGEMENT

static constexpr char SERVER_PROGRAM_FILENAME_BASE[] = "./game_server.dll"; // Base name of the DLL filepath containing the game server code.

static game_server_program SERVER_PROGRAM; // Static memory storage of the server program running on this platform.
static HMODULE SERVER_PROGRAM_MODULE;
static ia_static_string<256> SERVER_PROGRAM_FILENAME; // Filepath of file the server program was loaded from. Kept null-terminated.
static ui64 SERVER_PROGRAM_FILE_CREATION_TIME; // Creation time of currently-loaded server program dll file.

// (Re)Loads the Game Server program dll if it is newer than what is currently loaded.
// Returns whether the program was successfully loaded.
void win32_reload_game_server_program()
{
    ASSERT(!SERVER_PROGRAM._is_loaded || DEV_MODE); // Check that we're not attempting a hot reload without being a dev mode.
    if (SERVER_PROGRAM._is_loaded)
    {
        win32_log("Win32", "Unloading existing server program...");
        SERVER_PROGRAM.unload();

        // Unload library and delete old program file.
        FreeLibrary(SERVER_PROGRAM_MODULE);
        SERVER_PROGRAM_MODULE = NULL;
        DeleteFile(SERVER_PROGRAM_FILENAME._str);
    }

    // Load Game Server dll. It is expected to live next to the executable. 
    win32_logf("", "Loading Game Server program...");

    static_mem_arena<256> dllFileNameMem;
    dllFileNameMem.reset();
    ia_string_builder dllFileNameBuilder(&dllFileNameMem);
    dllFileNameBuilder.push_back(SERVER_PROGRAM_FILENAME_BASE);
    dllFileNameBuilder.push_back('\0');

    if (DEV_MODE)
    {
        // In Dev Mode, we want to find the file, copy it alongside its debug symbols and rename them, adding their time stamp.
        // Lookup creation time of base DLL we're about to copy.
        {
            HANDLE baseDLLFile = CreateFile(SERVER_PROGRAM_FILENAME_BASE, GENERIC_READ, NULL, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
            FILETIME createTime, lastAccess, lastWrite;
            GetFileTime(baseDLLFile, &createTime, &lastAccess, &lastWrite);
            CloseHandle(baseDLLFile);

            ui64 createTime64 = (ui64)createTime.dwLowDateTime | ((ui64)createTime.dwHighDateTime << 32);
            win32_logf("", "Game Server reload. Creation times %llu -> %llu", SERVER_PROGRAM_FILE_CREATION_TIME, createTime64);
            SERVER_PROGRAM_FILE_CREATION_TIME = createTime64;
        }

        // Change file name builder string to include timestamp.
        dllFileNameBuilder.chop_left(5); // Remove null terminator + ".dll".
        dllFileNameBuilder.chop_right(2); // Remove "./"
        dllFileNameBuilder.push_back_format("_%llu.dll", SERVER_PROGRAM_FILE_CREATION_TIME); // Append _<timestamp>.dll back on.
        dllFileNameBuilder.push_back('\0'); // Add null terminator.

        // Add folder.
        dllFileNameBuilder.push_front("./HOT_RELOAD_TEMP/");

        // Perform copy, assert on failure (server has to be fully restarted).
        BOOL copySuccess = CopyFile(SERVER_PROGRAM_FILENAME_BASE, dllFileNameBuilder.string._str, 0);
        ASSERT_MSG(copySuccess, "DLL file copy failed on hot reload. Base name = %s | Copy name = %s | Error Code = %d", 
                SERVER_PROGRAM_FILENAME_BASE, dllFileNameBuilder.string._str, GetLastError());

        win32_logf("", "Copied Game Server program DLL to \"%s\".", dllFileNameBuilder.string._str);
    }

    SERVER_PROGRAM_FILENAME = dllFileNameBuilder.string;

    SERVER_PROGRAM_MODULE = LoadLibrary(dllFileNameBuilder.string._str);
    ASSERT_MSG(SERVER_PROGRAM_MODULE != NULL, "Failed to load Game Server DLL module.");

    // Provide the Server Program structure with the load function and let it do the rest.
    game_server_program::load_program_fn load_program_func = (game_server_program::load_program_fn)GetProcAddress(SERVER_PROGRAM_MODULE, "game_server_load_program");
    ASSERT_MSG(load_program_func != nullptr, "Failed to load function from Game Server DLL.");

    SERVER_PROGRAM.load(_ASSERTION_HANDLER_PTR, load_program_func);
    win32_log("", LOG_SUCCESS, "Game Server Program loaded successfully.");
}

// -------------------------------------

void win32_platform_shutdown()
{
	ASSERT(WIN32_PLATFORM.initialized);

	if (SERVER_PROGRAM.is_running)
	{
		win32_log("", "Shutting down Game Server.");
		SERVER_PROGRAM.stop();
	}

	win32_log("", "Platform shutting down...");

	win32_net_stop(*WIN32_PLATFORM.net_component);
	win32_net_free(*WIN32_PLATFORM.net_component);

	win32_log("", LOG_SUCCESS, "Platform shutdown complete.");

	// Delete logging critical sections.
	DeleteCriticalSection(&CS_WIN32_STDOUT);
	DeleteCriticalSection(&CS_WIN32_STDERR);

	WIN32_PLATFORM = {};
}

// Main entry point.
int main(int argc, char** argv)
{	
    // Declare & Build the Assertions Handler.
    _ASSERTION_HANDLER WIN32_ASSERTION_HANDLER = {
        ASSERT_EXIT_FUNC,
        ASSERT_MSG_FUNC,
    };
    _ASSERTION_HANDLER_PTR = &WIN32_ASSERTION_HANDLER;

    // Register console signal handling.
	if (!SetConsoleCtrlHandler(win32_console_ctrl_handler, TRUE))
	{
		win32_logf("", LOG_WARNING, "Failed to register console control handler. Error code = %d", GetLastError());
	}

	// Init logging critical sections.
	InitializeCriticalSection(&CS_WIN32_STDOUT);
	InitializeCriticalSection(&CS_WIN32_STDERR);

	win32_log("", "Initializing IronAge.io Game Server.\nPlatform = Win32 x64\n");

    char RESOURCES_DIR_ABSOLUTE[MAX_PATH];
    if (argc > 1)
    {
        GetFullPathName(argv[1], sizeof(RESOURCES_DIR_ABSOLUTE) - 1, RESOURCES_DIR_ABSOLUTE, NULL);
    }
    else
    {
        const char* RESOURCES_DIR_DEFAULT = "./game_server_resources/";

        GetFullPathName(RESOURCES_DIR_DEFAULT, sizeof(RESOURCES_DIR_ABSOLUTE) - 1, RESOURCES_DIR_ABSOLUTE, NULL);
    }
    win32_logf("", "Server resources DIR path = %s", RESOURCES_DIR_ABSOLUTE); // @TODO(Marc): Check that directory exists.

    GAME_SERVER_RESOURCES_DIR = RESOURCES_DIR_ABSOLUTE; // The buffer will outlive any usage of this variable.

	// Initialize win32 platform structure.

	win32_logf("", "Allocating Game Server memory. Memory size = %llu bytes", GAME_SERVER_MEM_SIZE);

	win32_platform_init();
	ASSERT(WIN32_PLATFORM.initialized);

	ui8* game_server_mem = (ui8*)VirtualAlloc(NULL, GAME_SERVER_MEM_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	ASSERT_MSG(game_server_mem != nullptr, "Failed to allocate Game Server memory. Error code = %d", GetLastError());

	// TODO(Marc): Read command line / config file for those !
	game_server_init_params server_init_params = {

        .dev_mode = DEV_MODE,

		.match_slot_count = 4,
		.max_client_count = 1024,
    
        /*
		.run_test_scenario = false,
		.test_scenario_dump_filename = "snapshot_native.bin",
        */
	};

    // Initial load of Game Server program.
    win32_reload_game_server_program();
    ASSERT_MSG(SERVER_PROGRAM._is_loaded, "Failed to load server program.");

	win32_logf("", "Initializing Game Server...\n");
	SERVER_PROGRAM.is_running = SERVER_PROGRAM.init(WIN32_PLATFORM, server_init_params, game_server_mem, GAME_SERVER_MEM_SIZE);
	if (!SERVER_PROGRAM.is_running)
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

	while (!WIN32_PLATFORM.exitRequested)
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

        if (DEV_MODE)
        {
            // Query file timestamp of loaded Game Server program and compare it to current loadable program file.
            // If loadable file is newer, unload current program and reload it from the new file.
            // @TODO(Marc): Don't do this on every single tick !

            HANDLE baseDLLFile = CreateFile(SERVER_PROGRAM_FILENAME_BASE, GENERIC_READ, NULL, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
            if (baseDLLFile != INVALID_HANDLE_VALUE)
            {
                FILETIME createTime, lastAccess, lastWrite;
                GetFileTime(baseDLLFile, &createTime, &lastAccess, &lastWrite);
                CloseHandle(baseDLLFile);

                ui64 createTime64 = (ui64)createTime.dwLowDateTime | ((ui64)createTime.dwHighDateTime << 32);
                if (createTime64 > SERVER_PROGRAM_FILE_CREATION_TIME)
                {
                    win32_reload_game_server_program();
                }
            }
        }

		// Query uptime and run game server main tick function.
		QueryPerformanceCounter(&current_counter);

		// Measure time since game server initialization in milliseconds.
		ui64 time_ms = (current_counter.QuadPart * 1000 / counter_frequency.QuadPart) - start_ms;

		SERVER_PROGRAM.tick(time_ms);
	}

	win32_platform_shutdown();
	return 0;
}
