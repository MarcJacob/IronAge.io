// Main implementation file for the http server functionality of the game server.
// Little more than a file serving functionality for the game server to distribute the game client over http.

static constexpr ui16 HTTP_MAX_CONNECTIONS = 64;
static constexpr ui16 HTTP_MAX_FILES = 32; // Files that can be preloaded for serving.
static constexpr ui64 HTTP_MAX_FILES_TOTAL_SIZE = MiB(32); // Budget for all preloaded files together.
static constexpr ui32 HTTP_REQUEST_BUFFER_SIZE = 2048; // A request head (request line + headers) must fit in here.
static constexpr ui32 HTTP_RESPONSE_HEAD_BUFFER_SIZE = 256;
static constexpr ui32 HTTP_SEND_CHUNK_SIZE = 4096;
static constexpr ui32 HTTP_PATH_BUFFER_SIZE = 256;
static constexpr time_ms HTTP_SEND_STALL_TIMEOUT_MS = 10000; // Response making no progress for this long gets its httpConnection closed.
static constexpr time_ms HTTP_IDLE_TIMEOUT_MS = 30000; // Connection with no request coming in for this long gets closed.

// Holds the state of an active http httpConnection.
struct http_connection
{
	bool in_use; // If false, the structure can be used to register a new httpConnection.

	bool closing; // If true, the server has requested this httpConnection be closed.

	game_server_platform::net_connection_handle handle;

	ui32 request_size; // Bytes currently held in request_buffer.
	ui8 request_buffer[HTTP_REQUEST_BUFFER_SIZE];

	time_ms last_activity_ms; // Last time the httpConnection was opened, received bytes, or finished a response. Used to drop idle connections.

	
	bool responding; // Response in flight, if any: head then body. Body is served straight from a preloaded file.

	char head[HTTP_RESPONSE_HEAD_BUFFER_SIZE];

	ui32 head_size; // Total size of the HEAD section of the response we're serving.
	ui32 head_sent; // Total bytes sent of the HEAD section of the response we're serving.

	ui32 body_size; // Total size of the BODY section of the response we're serving.
	ui32 body_sent; // Total bytes sent of the BODY section of the response we're serving.

	const ui8* body; // Response body, points into a preloaded file. Null if the response has no body.
	time_ms last_send_progress_ms; // Last time the response started or made progress. Used to drop connections that stop accepting data.
};

// A file preloaded in server memory at init, served as-is to requests for its target_name.
struct http_file
{
	const char* name; // Name relative to web_root, as given in the init params (which must outlive the server).
	const char* content_type;
	ui8* data;
	ui32 size;
};

// State of the http server sub-system of the game server.
// Holds a set of files preloaded in memory, to serve to prospective browser-based game clients.
struct http_server
{
	http_connection connections[HTTP_MAX_CONNECTIONS];

	http_file files[HTTP_MAX_FILES];
	ui32 file_count;
};

static http_server* http_server_init(game_server& server)
{
	http_server* http = server.main_memory.alloc<http_server>();
	ASSERT_MSG(http != nullptr, "Not enough server memory for the HTTP server.");

	return http;
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

// Loads every file listed in the init params into server memory, from web_root. Fatal if any of them can't be loaded.
static void http_server_load_files(game_server& server)
{
	game_server_platform& platform = *server.platform;
	http_server& http = *server.http;
	const game_server_init_params& params = server.init_params;

	ASSERT_MSG(params.web_file_count <= HTTP_MAX_FILES, "Too many web files to serve (%d, max is %d).", params.web_file_count, HTTP_MAX_FILES);

	ui64 totalSize = 0;
	for (ui32 fileIndex = 0; fileIndex < params.web_file_count; fileIndex++)
	{
		http_file& file = http.files[fileIndex];
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
		ASSERT_MSG(totalSize <= HTTP_MAX_FILES_TOTAL_SIZE, "Web files exceed the %llu bytes budget at \"%s\".", HTTP_MAX_FILES_TOTAL_SIZE, filePath);

		file.data = server.main_memory.alloc<ui8>(fileSize);
		ASSERT_MSG(file.data != nullptr, "Not enough server memory to load web file \"%s\".", filePath);

		ui64 readSize = platform.read_resource_file(filePath, file.data, fileSize);
		ASSERT_MSG(readSize == fileSize, "Failed to read web file \"%s\".", filePath);

		file.size = (ui32)fileSize;
		file.content_type = http_get_content_type(file.name);

		platform.logf_stdout("Game Server HTTP: Loaded \"%s\" (%llu bytes).", file.name, fileSize);
	}

	http.file_count = params.web_file_count;
}

// Finds the preloaded file a request target ("/", "/src/main.js?x=1") asks for. Returns null if there is none.
// The target is only ever compared against the names of preloaded files, never used as a path.
static const http_file* http_server_find_file(http_server& http, const char* target)
{
	if (target[0] != '/') return nullptr;

	// Name asked for: everything after the '/', up to any query string or fragment. Empty means the site root.

	const char* target_name = target + 1; // Discard the first '/'

	if (*target_name == '\0')
	{
		target_name = "index.html";
	}

	for (ui32 fileIndex = 0; fileIndex < http.file_count; fileIndex++)
	{
		const http_file& file = http.files[fileIndex];

		if (ia_str_equal(file.name, target_name))
			return &file;
	}

	return nullptr;
}

// Starts sending a response on the httpConnection. The body, if any, is sent straight from the given memory, which must stay valid until the response is done.
static void http_server_send_response(game_server& server, http_connection& connection,
	const char* status, const char* content_type, const ui8* body, ui32 body_size)
{
	ui32 head_write_pos = 0;

	// Build response head section.
	ia_str_append(connection.head, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, "HTTP/1.1 ");
	ia_str_append(connection.head, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, status);
	ia_str_append(connection.head, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, "\r\nContent-Type: ");
	ia_str_append(connection.head, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, content_type);
	ia_str_append(connection.head, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, "\r\nContent-Length: ");
	ia_str_append_ui64(connection.head, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, body_size);
	ia_str_append(connection.head, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, "\r\nCache-Control: no-cache\r\n\r\n"); // Dev server: always re-fetch after a redeploy.

	// Flag the httpConnection as being responded to with the appropriate head & body sizes.
	connection.last_send_progress_ms = server.time_ms;
	connection.responding = true;

	connection.head_sent = 0;
	connection.head_size = head_write_pos;

	connection.body = body;
	connection.body_sent = 0;
	connection.body_size = body_size;
}

// Answers with an empty-bodied status response.
static void http_server_send_response_status(game_server& server, http_connection& connection, const char* status)
{
	http_server_send_response(server, connection, status, "text/plain; charset=utf-8", nullptr, 0);
}

// Handles a complete request head sitting at the start of the httpConnection's request buffer.
static void http_server_handle_request(game_server& server, http_connection& connection, ui32 head_size)
{
	game_server_platform& platform = *server.platform;

	// Request line: METHOD SP TARGET SP HTTP/1.x
	const char* request = (const char*)connection.request_buffer;

	ui32 methodNameEndPos = 0;
	while (methodNameEndPos < head_size && request[methodNameEndPos] != ' ' && request[methodNameEndPos] != '\r') methodNameEndPos++;

	ui32 targetNameStartPos = methodNameEndPos + 1;
	ui32 targetNameEndPos = targetNameStartPos;
	while (targetNameEndPos < head_size && request[targetNameEndPos] != ' ' && request[targetNameEndPos] != '\r') targetNameEndPos++;

	ui32 targetNameLen = targetNameEndPos - targetNameStartPos;

	bool wellFormed = request[methodNameEndPos] == ' '									// Check that method target_name is correctly delimited.
		&& targetNameEndPos > targetNameStartPos && request[targetNameEndPos] == ' '	// Check that target target_name is correctly delimited.
		&& targetNameLen < HTTP_PATH_BUFFER_SIZE										// Check that target target_name fits our path buffer.
		&& targetNameEndPos + 8 < head_size												// 
		&& ia_str_expect(request + targetNameEndPos + 1, "HTTP/");

	if (!wellFormed)
	{
		platform.logf_stderr("Game Server HTTP: Malformed request on httpConnection handle %d, closing.", connection.handle);
		platform.net_close_connection(connection.handle);
		connection.closing = true;
		return;
	}

	char targetNameBuff[HTTP_PATH_BUFFER_SIZE];

	ia_memcpy(targetNameBuff, request + targetNameStartPos, targetNameLen);
	targetNameBuff[targetNameLen] = '\0';

	bool isGet = methodNameEndPos == 3 && ia_str_expect(request, "GET");
	if (!isGet)
	{
		platform.logf_stdout("Game Server HTTP: Unsupported method on httpConnection handle %d -> 405.", connection.handle);
		http_server_send_response_status(server, connection,"405 Method Not Allowed");
		return;
	}

	// Find the file from the pre-loaded collection on the server.
	const http_file* file = http_server_find_file(*server.http, targetNameBuff);
	if (file == nullptr)
	{
		platform.logf_stdout("Game Server HTTP: GET %s -> 404.", targetNameBuff);
		http_server_send_response_status(server, connection, "404 Not Found");
		return;
	}

	platform.logf_stdout("Game Server HTTP: GET %s -> 200 (%d bytes).", targetNameBuff, file->size);
	http_server_send_response(server, connection, "200 OK", file->content_type, file->data, file->size);
}

// Pushes as much of the response as the platform will take. Whatever is left goes on the next tick.
static void http_server_progress_response(game_server& server, http_connection& connection)
{
	game_server_platform& platform = *server.platform;

	// Send fails if the platform send buffer is full or the httpConnection is closed. Either way, keep trying until it makes no progress for too long.
	bool stalled = false;

	// Try to send more data over.
	while (connection.head_sent < connection.head_size)
	{
		ui32 chunk = connection.head_size - connection.head_sent;
		if (chunk > HTTP_SEND_CHUNK_SIZE) chunk = HTTP_SEND_CHUNK_SIZE;

		if (platform.net_send_bytes(connection.handle, (const ui8*)connection.head + connection.head_sent, chunk))
		{
			connection.head_sent += chunk;
			connection.last_send_progress_ms = server.time_ms;
		}
		else
			stalled = true;
	}

	// Head was sent and we're not stalled -> continue to body.
	while (!stalled && connection.body_sent < connection.body_size)
	{
		ui32 chunk = connection.body_size - connection.body_sent;
		if (chunk > HTTP_SEND_CHUNK_SIZE) chunk = HTTP_SEND_CHUNK_SIZE;

		if (platform.net_send_bytes(connection.handle, connection.body + connection.body_sent, chunk))
		{
			connection.body_sent += chunk;
			connection.last_send_progress_ms = server.time_ms;
		}
		else 
			stalled = true;
	}

	// When stalled on head or body, determine how long since last recorded transfer activity and if past a threshold, drop the transfer & httpConnection.
	if (stalled)
	{
		if (server.time_ms - connection.last_send_progress_ms > HTTP_SEND_STALL_TIMEOUT_MS)
		{
			platform.logf_stderr("Game Server HTTP: Response on httpConnection handle %d stalled, closing.", connection.handle);
			platform.net_close_connection(connection.handle);
			connection.closing = true;
		}
		return;
	}

	// Response fully handed to the platform. Ready for the next request on this httpConnection.
	connection.responding = false;
	connection.last_activity_ms = server.time_ms;
}

// Receives and handles requests on the httpConnection. One request at a time, later pipelined ones wait for their turn in the request buffer.
// Always receives, even mid-response, so the platform can see the httpConnection as drained once the peer is gone.
static void http_server_receive(game_server& server, http_connection& connection)
{
	game_server_platform& platform = *server.platform;

	// Check that we're not about to overflow request buffer size. TODO(Marc): Discard previous requests if needed ?
	if (connection.request_size < HTTP_REQUEST_BUFFER_SIZE)
	{
		// Receive bytes on the httpConnection and place them in the request buffer.
		ui32 receivedBytes = platform.net_receive_bytes(connection.handle,
			connection.request_buffer + connection.request_size, HTTP_REQUEST_BUFFER_SIZE - connection.request_size);

		if (receivedBytes > 0)
		{
			connection.request_size += receivedBytes;
			connection.last_activity_ms = server.time_ms;
		}
	}

	// If we're already responding to a request on that httpConnection, don't go further.
	if (connection.responding) return;

	// Otherwise we can handle the request. Do a preliminary scan to find the end of the head segment.

	ui32 headSize = 0;
	for (ui32 i = 0; i + 3 < connection.request_size; i++)
	{
		if (ia_str_expect((const char*)&connection.request_buffer[i], "\r\n\r\n"))
		{
			headSize = i + 4;
			break;
		}
	}

	if (headSize == 0)
	{
		if (connection.request_size == HTTP_REQUEST_BUFFER_SIZE)
		{
			platform.logf_stderr("Game Server HTTP: Request head too large on httpConnection handle %d, closing.", connection.handle);
			platform.net_close_connection(connection.handle);
			connection.closing = true;
		}
		else if (server.time_ms - connection.last_activity_ms > HTTP_IDLE_TIMEOUT_MS)
		{
			platform.logf_stdout("Game Server HTTP: Connection handle %d idle, closing.", connection.handle);
			platform.net_close_connection(connection.handle);
			connection.closing = true;
		}
		return;
	}

	http_server_handle_request(server, connection, headSize);

	// Drop the handled request from the buffer, keeping any bytes that came after it.
	ia_memcpy(connection.request_buffer, connection.request_buffer + headSize, connection.request_size - headSize);
	connection.request_size -= headSize;
}

// Runs all HTTP serving for this tick, for any new connections.
static void http_server_tick(game_server& server)
{
	game_server_platform& platform = *server.platform;
	http_server& http = *server.http;

	constexpr ui16 CONNECTIONS_QUERY_BUFFER_SIZE = 32;

	// TEMP(Marc): Do the main connections handling here. At some point we'll move it back to "pure server code" and html connections will just be one kind
	// of service provided by the server.

	// Closed connections first, so a handle the platform re-uses is free on our end before its new httpConnection shows up.
	game_server_platform::net_connection_handle closedHandles[CONNECTIONS_QUERY_BUFFER_SIZE];

	ui32 closedConnectionsCount = platform.net_query_closed_connections(closedHandles, CONNECTIONS_QUERY_BUFFER_SIZE);
	for (ui16 closedIndex = 0; closedIndex < closedConnectionsCount; closedIndex++)
	{
		for (ui16 connectionIndex = 0; connectionIndex < HTTP_MAX_CONNECTIONS; connectionIndex++)
		{
			http_connection& httpConnection = http.connections[connectionIndex];
			if (httpConnection.in_use && httpConnection.handle == closedHandles[closedIndex])
			{
				httpConnection = {}; // Reset HTTP httpConnection.
				break;
			}
		}
	}

	// Handle new connections.
	game_server_platform::in_connection newConnections[CONNECTIONS_QUERY_BUFFER_SIZE];

	ui32 newConnectionsCount = platform.net_query_new_connections(newConnections, CONNECTIONS_QUERY_BUFFER_SIZE);
	for (ui16 newIndex = 0; newIndex < newConnectionsCount; newIndex++)
	{
		http_connection* freeConnection = nullptr;
		for (ui16 connectionIndex = 0; connectionIndex < HTTP_MAX_CONNECTIONS; connectionIndex++)
		{
			if (!http.connections[connectionIndex].in_use)
			{
				freeConnection = &http.connections[connectionIndex];
				break;
			}
		}

		if (freeConnection == nullptr)
		{
			platform.logf_stderr("Game Server HTTP: No room for httpConnection handle %d, closing.", newConnections[newIndex].platform_handle);
			platform.net_close_connection(newConnections[newIndex].platform_handle);
			continue;
		}

		// Reset httpConnection slot and mark it in use by the platform httpConnection.
		*freeConnection = {};
		freeConnection->in_use = true;
		freeConnection->handle = newConnections[newIndex].platform_handle;
		freeConnection->last_activity_ms = server.time_ms;
	}

	// Handle send / receive on applicable connections.
	for (ui16 connectionIndex = 0; connectionIndex < HTTP_MAX_CONNECTIONS; connectionIndex++)
	{
		http_connection& connection = http.connections[connectionIndex];
		if (!connection.in_use || connection.closing) continue;

		if (connection.responding) 
			http_server_progress_response(server, connection);

		if (!connection.closing)
			http_server_receive(server, connection);
	}
}
