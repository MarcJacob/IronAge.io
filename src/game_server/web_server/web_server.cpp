// Main implementation file for the web server functionality of the game server.
// Serves files over http to distribute the game client, and upgrades http clients to websocket
// to enter the main game client <-> game server communication protocol.
//
// Holds the behavior common to all web server clients. Protocol specific functionality lives in
// web_server_http.cpp (including the upgrade from http to websocket) and web_server_websocket.cpp,
// which are unity-compiled at the end of this file.

#include "web_server.h"

// Unity-compile the protocol specific web server code.
#include "web_server_http.cpp"
#include "web_server_websocket.cpp"

web_server* web_server_init(game_server& server)
{
	web_server* web = server.main_memory.alloc<web_server>();
	ASSERT_MSG(web != nullptr, "Not enough server memory for the Web server.");

	return web;
}

void web_server_on_client_disconnected(game_server& server, game_server_client& client)
{
	ASSERT(client.connection_context != nullptr);

	// Give the Web Server Client structure back to the pool.
	// We can check that the client's non-game client connection context points to an element of the web server's own clients table to establish ownership.
	if ((iptr)client.connection_context >= (iptr)server.web->clients
		&& (iptr)client.connection_context < (iptr)&server.web->clients[WEB_SERVER_MAX_CLIENTS])
	{
		server.logf("WEB SERVER", LOG_WARNING, "HTTP Client %d dropped due to Game Server Client disconnection.", client.handle.value);
		*(web_server_client*)client.connection_context = {};
	}
}

bool web_server_try_accept_client(game_server& server, game_server_client& client, const ui8* bytes, ui32 byte_count)
{
	ASSERT(byte_count <= WEB_CLIENT_RECEPTION_BUFFER_SIZE);

	if (!web_server_http_recognize_request(bytes, byte_count)) return false;

	web_server& web = *server.web;
	for (ui16 connectionIndex = 0; connectionIndex < WEB_SERVER_MAX_CLIENTS; connectionIndex++)
	{
		web_server_client& newClient = web.clients[connectionIndex];
		if (newClient.is_active()) continue;

		// Allocate new client. Start it out as HTTP.
		newClient.state = web_server_client::STATE::ACTIVE_HTTP;
		newClient.client_handle = client.handle;
		newClient.last_activity_ms = server.uptime_ms;

		http_client& httpClient = newClient.http;

		// Take the bytes from the Unknown client reception buffer and use them as the first received Request bytes in the new HTTP client.
		ia_memcpy(httpClient.request.buff, bytes, byte_count);
		httpClient.request.size = byte_count;

		// Promote client connection to NON GAME CLIENT and set the Web Server Client structure as its context.
		client.type = game_server_client::TYPE::NON_GAME_CLIENT;
		client.connection_context = &newClient;

		server.logf("WEB SERVER", "Accepted client %d as an HTTP client.", client.handle.value);

		return true;
	}

	server.log("WEB SERVER", LOG_ERROR, "Out of Web client slots, can't accept new Web client.");
	return false;
}

static const char* http_get_content_type(const char* path)
{
	struct content_type { const char* extension; const char* type; };

	// NOTE(Marc): Should move this out of the function on the next tidy-up pass.
	static const content_type SUPPORTED_CONTENT_TYPES[] = {
		{ "html", "text/html; charset=utf-8" },
		{ "js", "text/javascript; charset=utf-8" },
		{ "css", "text/css; charset=utf-8" },
		{ "wasm", "application/wasm" },
		{ "txt", "text/plain; charset=utf-8" },
		{ "ico", "image/x-icon"},
	};
	static constexpr ui8 SUPPORTED_CONTENT_TYPE_COUNT = sizeof(SUPPORTED_CONTENT_TYPES) / sizeof(content_type);

	// Find the extension start address of the last path segment.
	// Stays null if the path ends with '/'.
	const char* extension = nullptr;
	for (const char* c = path; *c != '\0'; c++)
	{
		if (*c == '/') extension = nullptr;
		else if (*c == '.') extension = c + 1;
	}

	if (extension != nullptr)
	{
		// Match the found extension against our supported content types.
		for (ui32 typeIndex = 0; typeIndex < SUPPORTED_CONTENT_TYPE_COUNT; typeIndex++)
		{
			if (ia_str_equal(extension, SUPPORTED_CONTENT_TYPES[typeIndex].extension))
				return SUPPORTED_CONTENT_TYPES[typeIndex].type;
		}
	}

	return "application/octet-stream"; // If no extension is found, treat the file as a byte stream.
}

void web_server_load_files(game_server& server)
{
	game_server_platform& platform = *server.platform;
	web_server& web = *server.web;
	const game_server_init_params& params = server.init_params;

	ASSERT_MSG(params.web_file_count <= WEB_SERVER_MAX_FILES, "Too many web files to serve (%d, max is %d).", params.web_file_count, WEB_SERVER_MAX_FILES);

	ui64 totalSize = 0;
	for (ui32 fileIndex = 0; fileIndex < params.web_file_count; fileIndex++)
	{
		http_file& file = web.files[fileIndex];
		file.name = params.web_files[fileIndex];

		// Build the platform path: <web_root>/<target_name>
		char filePath[HTTP_PATH_BUFFER_SIZE * 2];
		ui32 pathLen = 0;
		bool pathFits = ia_str_append(filePath, sizeof(filePath) - 1, pathLen, params.web_root)
			&& ia_str_append(filePath, sizeof(filePath) - 1, pathLen, "/")
			&& ia_str_append(filePath, sizeof(filePath) - 1, pathLen, file.name);

		ASSERT_MSG(pathFits, "Web file path too long: %s", file.name);
		filePath[pathLen] = '\0';

		ui64 fileSize = platform.read_resource_file(filePath, nullptr, 0);
		ASSERT_MSG(fileSize > 0, "Web file \"%s\" not found or empty.", filePath);

		totalSize += fileSize;
		ASSERT_MSG(totalSize <= WEB_SERVER_MAX_FILES_TOTAL_SIZE, "Web files exceed the %llu bytes budget at \"%s\".", WEB_SERVER_MAX_FILES_TOTAL_SIZE, filePath);

		file.data = server.main_memory.alloc<ui8>(fileSize);
		ASSERT_MSG(file.data != nullptr, "Not enough server memory to load web file \"%s\".", filePath);

		ui64 readSize = platform.read_resource_file(filePath, file.data, fileSize);
		ASSERT_MSG(readSize == fileSize, "Failed to read web file \"%s\".", filePath);

		file.size = (ui32)fileSize;
		file.content_type = http_get_content_type(file.name);

		server.logf("WEB SERVER", "Loaded \"%s\" (%llu bytes).", file.name, fileSize);
	}

	web.file_count = params.web_file_count;
}

const http_file* web_server_find_file(web_server& web, const ia_string_view& target)
{
	if (target.length == 0) return nullptr;
	if (target.view_str[0] != '/') return nullptr;

	// Name asked for: everything after the '/', up to any query string or fragment. Empty means the site root.

	// Strip away query section.
	ia_string_view targetName = ia_string_get_until(target.view_str, '?', target.length);

	// Strip away start '/'.
	targetName.view_str++;
	targetName.length--;
	if (targetName.length == 0)
	{
		targetName = "index.html";
	}


	for (ui32 fileIndex = 0; fileIndex < web.file_count; fileIndex++)
	{
		const http_file& file = web.files[fileIndex];

		if (targetName == file.name)
			return &file;
	}

	return nullptr;
}

void web_server_tick(game_server& server)
{
	web_server& webServer = *server.web;

	// Handle send / receive on applicable connections.
	for (ui16 clientIndex = 0; clientIndex < WEB_SERVER_MAX_CLIENTS; clientIndex++)
	{
		web_server_client& client = webServer.clients[clientIndex];
		if (!client.is_active()) continue;

		if (client.is_websocket() == false)
		{
			web_server_http_tick_client(server, client);
		}
	}
}
