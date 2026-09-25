// Main implementation file for the http server functionality of the game server.
// Little more than a file serving functionality for the game server to distribute the game client over http, and upgrade to websocket
// to enter the main game client <-> game server communication protocol.

#include "core.h"

#include "game_server.h"
#include "game_server_clients.h"

// Static configuration of HTTP Server.
// TODO(Marc): Add to runtime configuration system. Substructure of game server initialization ?

static constexpr ui16 HTTP_MAX_CONNECTIONS = 64;
static constexpr ui16 HTTP_MAX_FILES = 32; // Files that can be preloaded for serving.
static constexpr ui64 HTTP_MAX_FILES_TOTAL_SIZE = MiB(32); // Budget for all preloaded files together.

static constexpr ui32 HTTP_CLIENT_MAX_REQUEST_SIZE = 2048;
static constexpr ui32 HTTP_RESPONSE_HEAD_BUFFER_SIZE = 256;
static constexpr time_ms HTTP_CLIENT_IDLE_TIMEOUT_MS = 30000; // Connection with no request / activity for this long gets closed.

static constexpr ui32 HTTP_SEND_CHUNK_SIZE = 4096;
static constexpr ui32 HTTP_PATH_BUFFER_SIZE = 256;
static constexpr time_ms HTTP_SEND_STALL_TIMEOUT_MS = 10000; // Response making no progress for this long gets its httpConnection closed.


// Holds the state of an active http client.
struct http_client
{
	bool is_active; // If false, the structure can be used to register a new http client.
	bool being_dropped; // If true, the server has requested this client be closed. This is used so outbound data can be sent fully before actually dropping.

	game_server_client::client_handle client_handle; // Handle to related game server client.


	time_ms last_activity_ms; // Last time the http client was opened, received bytes, or finished a response. Used to drop idle connections.
	time_ms last_send_progress_ms; // Last time the response started or made progress. Used to drop connections that stop accepting data.

	// Request reception buffer. The data is buffered and interpreted as characters.
	struct
	{
		char buff[HTTP_CLIENT_MAX_REQUEST_SIZE]; // Reception buffer for this client. Acts as the upper limit for request sizes we can handle.
		ui32 size; // Number of received bytes awaiting processing.
	} request;

	struct response_struct
	{
		bool in_flight; // Is response actively being sent ?

		struct
		{
			char _buff[HTTP_RESPONSE_HEAD_BUFFER_SIZE]; // Response head.
			ui32 _buff_size; // Total size of the HEAD section of the response we're serving.
			ui32 _buff_sent; // Total bytes sent of the HEAD section of the response we're serving.
		} head;

		struct
		{
			const ui8* _buff; // Response body, points into an external buffer (usually a preloaded resource).
			ui32 _buff_size; // Total size of the BODY section of the response we're serving.
			ui32 _buff_sent; // Total bytes sent of the BODY section of the response we're serving.
		} body;

	} response;
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
	http_client clients[HTTP_MAX_CONNECTIONS];

	http_file files[HTTP_MAX_FILES];
	ui32 file_count;

	mem_arena request_mem; // Scratch memory used to process client requests.
};

// Parsed http request.
// NOTE(Marc): As we don't support any kind of request other than a simple resource get, this doesn't contain a body.
struct http_request
{
	enum class METHOD
	{
		NONE,
		UNSUPPORTED, // Method is known but not supported.
		GET, // Retrieve a server resource.
		HEAD, // Retrieve metadata related to a server resource (same as GET but no resource content sent back).
		// ...

		UNKNOWN,

	} method;

	char* target_name; // Zero-terminated name of target resource.
	ui16 total_size; // Total request size.

	// Associates a header field name and string value with a combined size.
	struct header_val_pair
	{
		ui16 size; // name + val_str combined length.
		char* name; // Zero-terminated name of the header field.
		char* val_str; // Zero-terminated value string.
	};
	struct
	{
		header_val_pair* fields; // All fields found in header.
		ui8 field_count;
		ui16 size; // Combined size of all fields.
	} header;
};

struct http_supported_method
{
	const char* method_name;
	http_request::METHOD supported_method;
};

static const http_supported_method HTTP_SUPPORTED_METHODS[] =
{
	{ "GET", http_request::METHOD::GET },
	{ "HEAD", http_request::METHOD::HEAD },

	// ... TODO(Marc): Support more methods ?
	{ "POST", http_request::METHOD::UNSUPPORTED },
	{ "PUT", http_request::METHOD::UNSUPPORTED },
	{ "DELETE", http_request::METHOD::UNSUPPORTED },
	{ "CONNECT", http_request::METHOD::UNSUPPORTED },
	{ "OPTIONS", http_request::METHOD::UNSUPPORTED },
	{ "TRACE", http_request::METHOD::UNSUPPORTED },
};
const ui8 SUPPORTED_METHOD_COUNT = sizeof(HTTP_SUPPORTED_METHODS) / sizeof(http_supported_method);


static http_server* http_server_init(game_server& server)
{
	http_server* http = server.main_memory.alloc<http_server>();
	ASSERT_MSG(http != nullptr, "Not enough server memory for the HTTP server.");

	http->request_mem = mem_arena_create_sub(server.main_memory, HTTP_CLIENT_MAX_REQUEST_SIZE);

	return http;
}

void http_server_on_client_disconnected(game_server& server, game_server_client& client)
{
	server.logf("HTTP SERVER", LOG_WARNING, "HTTP Client %d lost client.", client.handle.value);

	// Give the HTTP client structure back to the pool.
	if (client.http != nullptr) *client.http = {};
}

// Checks whether the bytes just received from an unknown client look like the start of an HTTP request (a known method followed by a space).
// If they do, gives the client an HTTP client structure holding those bytes and sets its type to HTTP.
// Returns whether the client was accepted. Bytes must fit the HTTP request buffer.
static bool http_server_try_accept_client(game_server& server, game_server_client& client, const ui8* bytes, ui32 byte_count)
{
	ASSERT(byte_count <= HTTP_CLIENT_MAX_REQUEST_SIZE);

	ui8 methodIndex;
	for (methodIndex = 0; methodIndex < SUPPORTED_METHOD_COUNT; methodIndex++)
	{
		const http_supported_method& method = HTTP_SUPPORTED_METHODS[methodIndex];

		char method_str_buff[16] = { 0 };
		ia_str_get_word((char*)bytes, method_str_buff, sizeof(method_str_buff) - 1);

		if (ia_str_equal(method_str_buff, method.method_name))
		{
			break;
		}
	}

	if (methodIndex == SUPPORTED_METHOD_COUNT) return false;

	http_server& http = *server.http;
	for (ui16 connectionIndex = 0; connectionIndex < HTTP_MAX_CONNECTIONS; connectionIndex++)
	{
		http_client& httpClient = http.clients[connectionIndex];
		if (httpClient.is_active) continue;

		httpClient = {};
		httpClient.is_active = true;
		httpClient.client_handle = client.handle;
		httpClient.last_activity_ms = server.uptime_ms;

		// The bytes were already taken from the platform client, so they become the start of the request.
		ia_memcpy(httpClient.request.buff, bytes, byte_count);
		httpClient.request.size = byte_count;

		client.type = game_server_client::TYPE::HTTP;
		client.http = &httpClient;

		server.logf("HTTP SERVER", "Accepted client %d as an HTTP client.", client.handle.value);
		return true;
	}

	server.log("HTTP SERVER", LOG_ERROR, "Out of HTTP client slots, can't accept new HTTP client.");
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

		server.logf("HTTP SERVER", "Loaded \"%s\" (%llu bytes).", file.name, fileSize);
	}

	http.file_count = params.web_file_count;
}

// Finds the preloaded file a request target ("/", "/src/main.js?x=1") asks for. Returns null if there is none.
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
static void http_server_send_response(game_server& server, http_client& connection,
	const char* status, const char* content_type, const ui8* body, ui32 body_size)
{
	ui32 head_write_pos = 0;

	// Build response head section.
	ia_str_append(connection.response.head._buff, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, "HTTP/1.1 ");
	ia_str_append(connection.response.head._buff, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, status);
	ia_str_append(connection.response.head._buff, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, "\r\nContent-Type: ");
	ia_str_append(connection.response.head._buff, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, content_type);
	ia_str_append(connection.response.head._buff, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, "\r\nContent-Length: ");
	ia_str_append_ui64(connection.response.head._buff, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, body_size);
	ia_str_append(connection.response.head._buff, HTTP_RESPONSE_HEAD_BUFFER_SIZE, head_write_pos, "\r\nCache-Control: no-cache\r\n\r\n"); // Dev server: always re-fetch after a redeploy.

	// Flag the http client as being responded to with the appropriate head & body sizes.
	// TODO(Marc): A lot of the time, we won't be sending anything to the client. It may be worth having staging memory for response buffers that is not linked to specific clients.
	connection.last_send_progress_ms = server.uptime_ms;
	connection.response.in_flight = true;

	connection.response.head._buff_sent = 0;
	connection.response.head._buff_size = head_write_pos;

	if (body != nullptr && body_size > 0)
	{
		connection.response.body._buff = body;
		connection.response.body._buff_sent = 0;
		connection.response.body._buff_size = body_size;
	}
	else
	{
		connection.response.body._buff = nullptr;
		connection.response.body._buff_sent = 0;
		connection.response.body._buff_size = 0;
	}
}

// Answers with an empty-bodied status response.
static void http_server_send_response_status(game_server& server, http_client& client, const char* status)
{
	http_server_send_response(server, client, status, "text/plain; charset=utf-8", nullptr, 0);
}

// Pushes as much of the response as the platform will take. Whatever is left goes on the next tick.
static void http_server_progress_response(game_server& server, http_client& client)
{
	game_server_platform& platform = *server.platform;

	// Send fails if the platform send buffer is full or the httpConnection is closed. Either way, keep trying until it makes no progress for too long.
	bool stalled = false;

	// Try to send more data over.
	while (client.response.head._buff_sent < client.response.head._buff_size)
	{
		ui32 chunk = client.response.head._buff_size - client.response.head._buff_sent;
		if (chunk > HTTP_SEND_CHUNK_SIZE) chunk = HTTP_SEND_CHUNK_SIZE;

		if (game_server_client_send_message(server, client.client_handle, (const ui8*)client.response.head._buff + client.response.head._buff_sent, chunk))
		{
			client.response.head._buff_sent += chunk;
			client.last_send_progress_ms = server.uptime_ms;
		}
		else
			stalled = true;
	}

	// Head was sent and we're not stalled -> continue to body.
	while (!stalled && client.response.body._buff_sent < client.response.body._buff_size)
	{
		ui32 chunk = client.response.body._buff_size - client.response.body._buff_sent;
		if (chunk > HTTP_SEND_CHUNK_SIZE) chunk = HTTP_SEND_CHUNK_SIZE;

		if (game_server_client_send_message(server, client.client_handle, client.response.body._buff + client.response.body._buff_sent, chunk))
		{
			client.response.body._buff_sent += chunk;
			client.last_send_progress_ms = server.uptime_ms;
		}
		else 
			stalled = true;
	}

	// When stalled on head or body, determine how long since last recorded transfer activity and if past a threshold, drop the transfer & httpConnection.
	if (stalled)
	{
		if (server.uptime_ms - client.last_send_progress_ms > HTTP_SEND_STALL_TIMEOUT_MS)
		{
			server.logf("HTTP SERVER", LOG_ERROR, "Response on client handle %d stalled, closing.", client.client_handle.value);
			game_server_client_drop(server, client.client_handle);
			client.being_dropped = true;
		}
		return;
	}

	// Response fully handed to the platform. Ready for the next request on this client.
	client.response.in_flight = false;
	client.last_activity_ms = server.uptime_ms;

	// Drop the client if flagged for closing.
	if (client.being_dropped)
	{
		game_server_client_drop(server, client.client_handle);
	}
}

// Handles a complete request for a client.
static void http_server_handle_request(game_server& server, http_client& client, http_request& request)
{
	// NOTE(Marc): Currently only GET and HEAD method requests are handled.
	const http_file* targetFile = nullptr;
	switch (request.method)
	{
	case http_request::METHOD::GET:
	case http_request::METHOD::HEAD:

		// Fetch file resource. If HEAD, send back only the metadata.
		targetFile = http_server_find_file(*server.http, request.target_name);
		if (targetFile == nullptr)
		{
			http_server_send_response_status(server, client, "404 Not Found");
			break;
		}

		if (request.method == http_request::METHOD::GET)
			http_server_send_response(server, client, "200 Ok", targetFile->content_type, targetFile->data, targetFile->size);
		if (request.method == http_request::METHOD::HEAD)
			http_server_send_response(server, client, "200 Ok", targetFile->content_type, nullptr, targetFile->size);

		break;
	case http_request::METHOD::UNKNOWN:
		http_server_send_response_status(server, client, "501 Not Implemented");
		client.being_dropped = true;

		server.logf("HTTP SERVER", LOG_WARNING, "Client handle %d requested unknown method. Closing client.", client.client_handle.value);
		break;
	case http_request::METHOD::UNSUPPORTED:
	default:
		http_server_send_response_status(server, client, "405 Method Not Allowed\r\nAllow: GET, HEAD");
		client.being_dropped = true;

		server.logf("HTTP SERVER", LOG_WARNING, "Client handle %d requested unsupported method. Closing client.", client.client_handle.value);
		break;
	}
}

// Receives bytes buffered on the platform for a client, then attempts to parse a single request to be processed.
// If successful, the bytes used to form the request are consumed and the remainder are shifted towards the start of the reception buffer.
// In such case, the string elements in the request structure will be allocated through the provided memory arena.
// Returns true if a request was successfully parsed OR no bytes were received (check request method value).
// Returns false if the request fatally failed to parse, triggering a status code response and dropping the client.
static bool http_server_receive(game_server& server, http_client& client, mem_arena& memory, http_request& out_request)
{
	game_server_platform& platform = *server.platform;
	out_request = {};

	// Check that we're not about to overflow request buffer size. TODO(Marc): Discard previous requests if needed ?
	if (client.request.size < HTTP_CLIENT_MAX_REQUEST_SIZE - 1)
	{
		// Receive bytes on the httpConnection and place them in the request buffer.
		ui32 receivedBytes = game_server_client_receive_message(server, client.client_handle,
			(ui8*)client.request.buff + client.request.size, HTTP_CLIENT_MAX_REQUEST_SIZE - client.request.size - 1);

		if (receivedBytes > 0)
		{
			client.request.size += receivedBytes;
			client.last_activity_ms = server.uptime_ms;
		}
	}

	// Prepass over the buffer to find whether it contains a complete head.
	ui16 head_end_index = 0;

	// Read header fields if any bytes are left.
	bool foundHead = false;
	if (client.request.size >= 4)
	while (head_end_index < client.request.size - 3)
	{
		if (ia_str_expect(client.request.buff + head_end_index, "\r\n\r\n"))
		{
			foundHead = true;
			head_end_index += 4;
			break;
		}
		head_end_index++;
	}

	if (!foundHead)
	{
		// No full head present in the request buffer. If the buffer is full, it never will be.
		if (client.request.size >= HTTP_CLIENT_MAX_REQUEST_SIZE - 1)
		{
			http_server_send_response_status(server, client, "431 Request Header Fields Too Large");
			client.being_dropped = true;

			server.logf("HTTP SERVER", LOG_WARNING, "Client handle %d sent a request head larger than %d bytes. Closing client.",
				client.client_handle.value, HTTP_CLIENT_MAX_REQUEST_SIZE);
			return false;
		}

		return true;
	}

	// Parse request from buffered characters.

	char* requestBytes = client.request.buff;
	ui32 readBytes = 0;

	// Method
	{
		char method_str_buff[8] = { 0 };
		readBytes += ia_str_get_word(requestBytes + readBytes, method_str_buff, sizeof(method_str_buff) - 1);
		readBytes++; // Include (expected) space.

		for (int supportedMethodIndex = 0; supportedMethodIndex < SUPPORTED_METHOD_COUNT; supportedMethodIndex++)
		{
			const http_supported_method& supportedMethod = HTTP_SUPPORTED_METHODS[supportedMethodIndex];
			if (ia_str_equal(supportedMethod.method_name, method_str_buff))
			{
				out_request.method = supportedMethod.supported_method;
			}
		}
		if (out_request.method == http_request::METHOD::NONE)
		{
			out_request.method = http_request::METHOD::UNKNOWN;
		}
	}

	// If there are no more bytes afterward, this is a bad request.
	if (readBytes >= head_end_index)
	{
		http_server_send_response_status(server, client, "400 Bad Request");
		client.being_dropped = true;
		return false;
	}

	// Target
	{
		// We only support simple origin-form names as target (starting with '/'), with query or fragment segments being completely discarded.
		// No special characters are present in the available file names, so special character encodings are not yet decoded.
		char target_str_buff[HTTP_PATH_BUFFER_SIZE] = { 0 };
		ui16 targetLen = ia_str_get_word(requestBytes + readBytes, target_str_buff, sizeof(target_str_buff));

		// Protect against target names exceeding max length.
		if (targetLen == sizeof(target_str_buff))
		{
			http_server_send_response_status(server, client, "414 URL Too Long");
			client.being_dropped = true;
			return false;
		}

		readBytes += targetLen;
		readBytes++; // Included expected space.

		// Cut-off target name at first query character encountered.
		for (ui16 targetCharIndex = 0; targetCharIndex < targetLen; targetCharIndex++)
		{
			if (target_str_buff[targetCharIndex] == '?')
			{
				targetLen = targetCharIndex;
				break;
			}
		}

		out_request.target_name = memory.alloc<char>(targetLen + 1);
		ia_memcpy(out_request.target_name, target_str_buff, targetLen);
		out_request.target_name[targetLen] = '\0';
	}

	// Check HTTP version (only 1.1 is supported).
	if (readBytes >= head_end_index 
		|| !ia_str_expect(requestBytes + readBytes, "HTTP/1.1\r\n"))
	{
		http_server_send_response_status(server, client, "505 HTTP Version Not Supported.");
		client.being_dropped = true;

		server.logf("HTTP SERVER", LOG_WARNING, "Client handle %d used wrong HTTP version. Closing client.", client.client_handle.value);
		return false;
	}
	readBytes += sizeof("HTTP/1.1\r\n") - 1;

	// Jump read to head end.
	// NOTE(Marc): This is in place to effectively ignore header field values, temporarily.
	readBytes = head_end_index;
	out_request.total_size = readBytes;

	// Left-shift remaining bytes in client request buffer to the left.
	ia_memcpy(client.request.buff, client.request.buff + readBytes, client.request.size - readBytes);
	client.request.size -= readBytes;

	return true;
}

static void http_server_tick(game_server& server)
{
	game_server_platform& platform = *server.platform;
	http_server& httpServer = *server.http;

	constexpr ui16 CONNECTIONS_QUERY_BUFFER_SIZE = 32;

	// Handle send / receive on applicable connections.
	for (ui16 clientIndex = 0; clientIndex < HTTP_MAX_CONNECTIONS; clientIndex++)
	{
		http_client& client = httpServer.clients[clientIndex];
		if (!client.is_active) continue;

		// Either progress current response, or receive next request if the client isn't in the process of being dropped.
		if (client.response.in_flight)
		{
			http_server_progress_response(server, client);
		}
		else if (!client.being_dropped)
		{
			http_request nextRequest;
			if (http_server_receive(server, client, httpServer.request_mem, nextRequest)
				&& nextRequest.method != http_request::METHOD::NONE)
			{
				http_server_handle_request(server, client, nextRequest);
			}

			// Free request memory.
			httpServer.request_mem.allocated_count = 0;

			// Drop the client if it has been idle for too long (no bytes received, no response finished).
			// Not applied while a response is in flight: that has its own stall timeout.
			if (!client.response.in_flight && !client.being_dropped
				&& server.uptime_ms - client.last_activity_ms > HTTP_CLIENT_IDLE_TIMEOUT_MS)
			{
				server.logf("HTTP SERVER", LOG_WARNING, "Client handle %d idle for over %llu ms. Closing client.",
					client.client_handle.value, HTTP_CLIENT_IDLE_TIMEOUT_MS);
				game_server_client_drop(server, client.client_handle);
			}
		}
	}
}
