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

void web_server_on_client_disconnected(game_server& server, game_server_client& client)
{
	if (client.connection_context == nullptr) return;

	// Give the Web Server Client structure back to the pool.
	// We can check that the client's non-game client connection context points to an element of the web server's own clients table to establish ownership.
	if ((iptr)client.connection_context >= (iptr)server.web->clients
		&& (iptr)client.connection_context < (iptr)&server.web->clients[WEB_SERVER_MAX_CLIENTS])
	{
		server.logf("WEB SERVER", LOG_WARNING, "HTTP Client %d dropped due to Game Server Client disconnection.", client.handle.value);
		ia_memzero(client.connection_context, sizeof(web_server_client));
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

		server.logf("WEB SERVER", LOG_SUCCESS, "Accepted client %d as an HTTP client.", client.handle.value);

		return true;
	}

	server.log("WEB SERVER", LOG_ERROR, "Out of Web client slots, can't accept new Web client.");
	return false;
}

static ia_string_view http_get_content_type(const ia_string_view& path)
{
	struct content_type { const ia_string_view extension; const ia_string_view type; };

	// NOTE(Marc): Should move this out of the function on the next tidy-up pass.
	static const content_type SUPPORTED_CONTENT_TYPES[] = {
		{ ".html", "text/html; charset=utf-8" },
		{ ".js", "text/javascript; charset=utf-8" },
		{ ".css", "text/css; charset=utf-8" },
		{ ".wasm", "application/wasm" },
		{ ".txt", "text/plain; charset=utf-8" },
		{ ".ico", "image/x-icon"},
		{ ".svg", "image/svg+xml"},
	};
	static constexpr ui8 SUPPORTED_CONTENT_TYPE_COUNT = sizeof(SUPPORTED_CONTENT_TYPES) / sizeof(content_type);

	// Match the found extension against our supported content types.
	for (ui32 typeIndex = 0; typeIndex < SUPPORTED_CONTENT_TYPE_COUNT; typeIndex++)
	{
		if (ia_string_ends_with(path, SUPPORTED_CONTENT_TYPES[typeIndex].extension))
			return SUPPORTED_CONTENT_TYPES[typeIndex].type;
	}

	return "application/octet-stream"; // If no extension is found, treat the file as a byte stream.
}

const http_file* web_server_find_file(web_server& web, const ia_string_view& target)
{
	if (target.length == 0) return nullptr;
	if (target.view_str[0] != '/') return nullptr;

	// Name asked for: everything after the '/', up to any query string or fragment. Empty means the site root.

	// Strip away query section.
	ia_string_view targetName = ia_string_get_until(target, '?', target.length);

	// Strip away start '/'.
	ia_string_chop_right(targetName, 1);
	if (targetName.length == 0)
	{
		targetName = "index.html";
	}


	for (ui32 fileIndex = 0; fileIndex < web.file_count; fileIndex++)
	{
		const http_file& file = web.files[fileIndex];

		if (targetName == file.resource_name)
			return &file;
	}

	return nullptr;
}

void web_server_reload_files(game_server& server)
{
	// TODO(Marc): When implementing actual runtime reload, it needs to be done while we have the guarantee that no files are currently being server.
	// During that time, we can just stop reacting to GET requests or even any request at all. Reloading the files should never take that long or happen that often,
	// or happen in critical circumstances, so it can be kept very simple. The reload flag preventing this collision can be made atomic for good measure.
	ASSERT(server.web->file_count == 0); // Temp assert so I don't forget the todo above.

	game_server_platform& platform = *server.platform;
	web_server& web = *server.web;

	// Clear existing files.
	for (ui32 fileIndex = 0; fileIndex < web.file_count; fileIndex++)
	{
		http_file& file = web.files[fileIndex];
		file = {};
	}
	web.file_count = 0;
	web.file_data_memory.clear();

	ui64 totalSize = 0;
	for (ui32 fileIndex = 0; fileIndex < server.resource_file_count; fileIndex++)
	{
		const game_server_resource_path& serverFile = server.resource_files[fileIndex];
		if (!ia_string_starts_with(serverFile, server.init_params.web_root))
		{
			// Not located in web root.
			continue;
		}
		ASSERT_MSG(web.file_count < WEB_SERVER_MAX_FILES, "Too many web files to serve (max is %d), at \"%.*s\".", WEB_SERVER_MAX_FILES, (int)serverFile.length, serverFile._str);

		http_file& file = web.files[web.file_count];
		ui64 fileSize = platform.read_resource_file(serverFile, nullptr, 0);
		ASSERT_MSG(fileSize > 0, "Web file \"%.*s\" not found or empty.", (int)serverFile.length, serverFile._str);

		totalSize += fileSize;
		ASSERT_MSG(totalSize <= WEB_SERVER_TOTAL_FILE_DATA_MEM, "Web files exceed the %llu bytes budget at \"%.*s\".",
			WEB_SERVER_TOTAL_FILE_DATA_MEM, (int)serverFile.length, serverFile._str);

		file.data = web.file_data_memory.alloc<ui8>(fileSize);
		ASSERT_MSG(file.data != nullptr, "Not enough server memory to load web file \"%.*s\".", (int)serverFile.length, serverFile._str);

		ui64 readSize = platform.read_resource_file(serverFile, file.data, fileSize);
		ASSERT_MSG(readSize == fileSize, "Failed to read web file \"%.*s\".", (int)serverFile.length, serverFile._str);

		file.resource_name = { serverFile._str + server.init_params.web_root.length, serverFile.length - server.init_params.web_root.length };
		file.size = (ui32)fileSize;
		file.content_type = http_get_content_type(file.resource_name);

		server.logf("WEB SERVER", LOG_SUCCESS, "Loaded \"%s\" (%ud bytes).", file.resource_name, fileSize);
		web.file_count++;
	}

	ASSERT_MSG(web.file_count > 0, "No resource file found under specified web root.");
}

web_server* web_server_init(game_server& server)
{
	// Allocate web server itself directly in main server memory, then allocate memory for self + sub-allocate it to specific parts.

	web_server* web = server.main_memory.alloc<web_server>();
	ASSERT_MSG(web != nullptr, "Not enough server memory for the Web server.");

	web->memory = mem_arena_create_sub(server.main_memory, WEB_SERVER_TOTAL_MEM);
	web->file_data_memory = mem_arena_create_sub(web->memory, WEB_SERVER_TOTAL_FILE_DATA_MEM);

	return web;
}

void web_server_tick(game_server& server)
{
	web_server& webServer = *server.web;

	// Handle send / receive on applicable connections.
	for (ui16 clientIndex = 0; clientIndex < WEB_SERVER_MAX_CLIENTS; clientIndex++)
	{
		web_server_client& client = webServer.clients[clientIndex];
		if (!client.is_active()) continue;

		if (client.is_http())
		{
			web_server_http_tick_client(server, client);
		}
		else
		{
			web_server_websocket_tick_client(server, client);
		}
	}
}
