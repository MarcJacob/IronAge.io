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
static constexpr ui32 HTTP_CLIENT_MAX_REQUEST_TARGET_LEN = 256; // Maximum number of characters in a valid http request target name.
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
			ia_static_string<HTTP_RESPONSE_HEAD_BUFFER_SIZE> str;
			ui32 size; // Total byte size of the HEAD section of the response we're serving.
			ui32 sent; // Bytes sent of the HEAD section of the response we're serving.
		} head;

		struct
		{
			const ui8* buff; // Response body, points into an external buffer (usually a preloaded resource).
							 // NOTE(Marc): buff of body. A little like me <3 (working on this def made me lose some muscle mass)
			ui32 size; // Total size of the BODY section of the response we're serving.
			ui32 sent; // Total bytes sent of the BODY section of the response we're serving.
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
};

// Parsed http request. String values within are NOT zero-terminated but bound by an associated length value.
// 
// NOTE(Marc): As we don't support any kind of request other than a simple resource get, this doesn't contain a body.
struct http_request
{
	static constexpr ui8 MAX_HEADER_FIELD_COUNT = 64;

	enum class METHOD
	{
		NONE,
		UNSUPPORTED, // Method is known but not supported.
		GET, // Retrieve a server resource.
		HEAD, // Retrieve metadata related to a server resource (same as GET but no resource content sent back).
		// ...

		UNKNOWN,

	} method;

	ia_string_view target_name;

	// Associates a header field name and string value with a combined size.
	struct header_field
	{
		ia_string_view name;
		ia_string_view value;
	};
	struct
	{
		header_field fields[MAX_HEADER_FIELD_COUNT]; // All fields found in header.
		ui8 field_count;
	} header;

	ui32 total_size; // Total request size.
};

// Looks for a header field with the provided name and places it in out_field. Returns whether it was found.
static bool http_request_find_header_field(const http_request& request, const ia_string_view& field_name, http_request::header_field& out_field)
{
	for (ui8 fieldIndex = 0; fieldIndex < request.header.field_count; fieldIndex++)
	{
		if (ia_string_equal(request.header.fields[fieldIndex].name, field_name, false))
		{
			out_field = request.header.fields[fieldIndex];
			return true;
		}
	}

	out_field = {};
	return false;
}

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

		ia_string_view methodName = ia_string_get_word_n((char*)bytes, byte_count);

		if (methodName == method.method_name)
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
static const http_file* http_server_find_file(http_server& http, const ia_string_view& target)
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


	for (ui32 fileIndex = 0; fileIndex < http.file_count; fileIndex++)
	{
		const http_file& file = http.files[fileIndex];

		if (targetName == file.name)
			return &file;
	}

	return nullptr;
}

// Starts sending a response on the httpConnection. The body, if any, is sent straight from the given memory, which must stay valid until the response is done.
static void http_server_serve_content(game_server& server, http_client& client,
	const char* status, const char* content_type, const ui8* content, ui32 content_size)
{
	ui32 head_write_pos = 0;

	// Build response head section.
	client.response.head.str.length = 0; // TODO(Marc): String reset function.
	head_write_pos += ia_string_push(client.response.head.str, "HTTP/1.1 ");
	head_write_pos += ia_string_push(client.response.head.str, status);
	head_write_pos += ia_string_push(client.response.head.str, "\r\nContent-Type: ");
	head_write_pos += ia_string_push(client.response.head.str, content_type);
	head_write_pos += ia_string_push(client.response.head.str, "\r\nContent-Length: ");

	char bodySizeStrBuff[32] = {0}; // TODO(Marc): Fiiiiiiiiiiiiiiiiiiiine I guess I'll make a proper string format function. Eventually.
	{
		ui32 appendCount = 0;
		ia_str_append_ui64(bodySizeStrBuff, sizeof(bodySizeStrBuff) - 1, appendCount, content_size);
	}
	head_write_pos += ia_string_push(client.response.head.str, bodySizeStrBuff);
	head_write_pos += ia_string_push(client.response.head.str, "\r\nCache-Control: no-cache\r\n\r\n"); // Dev server: always re-fetch after a redeploy.

	// Flag the http client as being responded to with the appropriate head & body sizes.
	// TODO(Marc): A lot of the time, we won't be sending anything to the client. It may be worth having staging memory for response buffers that is not linked to specific clients.
	client.last_send_progress_ms = server.uptime_ms;
	client.response.in_flight = true;

	client.response.head.sent = 0;
	client.response.head.size = head_write_pos;

	if (content != nullptr && content_size > 0)
	{
		client.response.body.buff = content;
		client.response.body.sent = 0;
		client.response.body.size = content_size;
	}
	else
	{
		client.response.body.buff = nullptr;
		client.response.body.sent = 0;
		client.response.body.size = 0;
	}
}

// Answers with an empty-bodied status response, optionally dropping the client afterward.
static void http_server_send_response_status(game_server& server, http_client& client, 
	const char* status_msg, bool drop_client)
{
	ASSERT_MSG(client.response.in_flight == false, "Attempted to re-send response data to HTTP client while a response was already in flight.");

	// Build response head section.
	client.response.head.str.length = 0; // TODO(Marc): String reset function.

	ui32 head_write_pos = 0;
	head_write_pos += ia_string_push(client.response.head.str, "HTTP/1.1 ");
	head_write_pos += ia_string_push(client.response.head.str, status_msg);
	head_write_pos += ia_string_push(client.response.head.str, "\r\n");

	if (drop_client)
	{
		client.being_dropped = true;
		head_write_pos += ia_string_push(client.response.head.str, "Connection: Close\r\n");
	}
	else
	{
		head_write_pos += ia_string_push(client.response.head.str, "Content-Type: text/plain charset=utf-8\r\n");
		head_write_pos += ia_string_push(client.response.head.str, "Content-Length: 0\r\n");
	}

	head_write_pos += ia_string_push(client.response.head.str, "\r\n");

	client.last_activity_ms = server.uptime_ms;

	client.response.head.sent = 0;
	client.response.head.size = head_write_pos;
	client.response.body.buff = nullptr;
	client.response.body.sent = 0;
	client.response.body.size = 0;

	client.response.in_flight = true;
}

// Pushes as much of the response as the platform will take. Whatever is left goes on the next tick.
static void http_server_progress_response(game_server& server, http_client& client)
{
	game_server_platform& platform = *server.platform;

	// Send fails if the platform send buffer is full or the httpConnection is closed. Either way, keep trying until it makes no progress for too long.
	bool stalled = false;

	// Try to send more data over.
	while (client.response.head.sent < client.response.head.size)
	{
		ui32 chunkSize = ia_min(HTTP_SEND_CHUNK_SIZE, client.response.head.size - client.response.head.sent);

		if (game_server_client_send_message(server, client.client_handle, (const ui8*)client.response.head.str._str + client.response.head.sent, chunkSize))
		{
			client.response.head.sent += chunkSize;
			client.last_send_progress_ms = server.uptime_ms;
		}
		else
			stalled = true;
	}

	// Head was sent and we're not stalled -> continue to body.
	while (!stalled && client.response.body.sent < client.response.body.size)
	{
		ui32 chunk = client.response.body.size - client.response.body.sent;
		if (chunk > HTTP_SEND_CHUNK_SIZE) chunk = HTTP_SEND_CHUNK_SIZE;

		if (game_server_client_send_message(server, client.client_handle, client.response.body.buff + client.response.body.sent, chunk))
		{
			client.response.body.sent += chunk;
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
	// TODO(Marc): Per request type function ?

	const http_file* targetFile = nullptr;
	http_request::header_field connectionField;

	switch (request.method)
	{
	case http_request::METHOD::GET:
	case http_request::METHOD::HEAD:

		// Fetch file resource. If HEAD, send back only the metadata.
		targetFile = http_server_find_file(*server.http, request.target_name);
		if (targetFile == nullptr)
		{
			http_server_send_response_status(server, client, "404 Not Found", false);
			break;
		}

		if (request.method == http_request::METHOD::GET)
			http_server_serve_content(server, client, "200 Ok", targetFile->content_type, targetFile->data, targetFile->size);
		if (request.method == http_request::METHOD::HEAD)
			http_server_serve_content(server, client, "200 Ok", targetFile->content_type, nullptr, targetFile->size);

		// If the request has header field "Connection: Close" then we can drop the client.
		if (http_request_find_header_field(request, "Connection", connectionField))
		{
			if (connectionField.value == "close")
			{
				client.being_dropped = true;
			}
		}

		break;
	case http_request::METHOD::UNKNOWN:
		http_server_send_response_status(server, client, "501 Not Implemented", true);
		server.logf("HTTP SERVER", LOG_WARNING, "Client handle %d requested unknown method. Closing client.", client.client_handle.value);
		break;
	case http_request::METHOD::UNSUPPORTED:
	default:
		http_server_send_response_status(server, client, "405 Method Not Allowed\r\nAllow: GET, HEAD", true);
		server.logf("HTTP SERVER", LOG_WARNING, "Client handle %d requested unsupported method. Closing client.", client.client_handle.value);
		break;
	}
}

// Receives bytes buffered on the platform for a client, then attempts to parse a single request to be processed.
// Returns true if a request was successfully parsed OR no bytes were received (check request method value).
// Returns false if the request fatally failed to parse, triggering a status code response and dropping the client.
static bool http_server_receive(game_server& server, http_client& client, http_request& out_request)
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
			http_server_send_response_status(server, client, "431 Request Header Fields Too Large", true);
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
		ia_string_view methodName = ia_string_get_word(requestBytes + readBytes);
		readBytes += methodName.length;
		readBytes++; // Include (expected) space.

		for (int supportedMethodIndex = 0; supportedMethodIndex < SUPPORTED_METHOD_COUNT; supportedMethodIndex++)
		{
			const http_supported_method& supportedMethod = HTTP_SUPPORTED_METHODS[supportedMethodIndex];
			if (methodName == supportedMethod.method_name)
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
		http_server_send_response_status(server, client, "400 Bad Request", true);
		return false;
	}

	// Target
	{
		// We only support simple origin-form names as target (starting with '/'), with query or fragment segments being completely discarded.
		// No special characters are present in the available file names, so special character encodings are not yet decoded and will simply cut name off.
		// TODO(Marc): Accept special characters in get_word and add decoding routine.
		out_request.target_name = ia_string_get_word(requestBytes + readBytes, HTTP_CLIENT_MAX_REQUEST_TARGET_LEN, "/_.?");

		// Protect against target names exceeding max length.
		if (out_request.target_name.length == HTTP_CLIENT_MAX_REQUEST_TARGET_LEN)
		{
			http_server_send_response_status(server, client, "414 URL Too Long", true);
			return false;
		}

		readBytes += out_request.target_name.length;
		readBytes++; // Include expected space.
	}

	// Check HTTP version (only 1.1 is supported).
	if (readBytes >= head_end_index 
		|| !ia_str_expect(requestBytes + readBytes, "HTTP/1.1\r\n"))
	{
		http_server_send_response_status(server, client, "505 HTTP Version Not Supported.", true);
		server.logf("HTTP SERVER", LOG_WARNING, "Client handle %d used wrong HTTP version. Closing client.", client.client_handle.value);
		return false;
	}
	readBytes += sizeof("HTTP/1.1\r\n") - 1;

	// Parse header fields.
	while(readBytes < head_end_index - 3)
	{
		if (out_request.header.field_count == http_request::MAX_HEADER_FIELD_COUNT)
		{
			http_server_send_response_status(server, client, "431 Too Many Headers", true);
			return false;
		}

		// Read a word as field name, expect a semicolon, then read all characters until end of line as value.
		http_request::header_field& field = out_request.header.fields[out_request.header.field_count++];

		// Name
		field.name = ia_string_get_word_n(requestBytes + readBytes, head_end_index - readBytes, 0, "-_.");
		if (field.name.length == 0 
			|| readBytes + field.name.length == head_end_index
			|| requestBytes[readBytes + field.name.length] != ':')
		{
			http_server_send_response_status(server, client, "400 Bad Request", true);
			return false;
		}
		readBytes += field.name.length + 1; // Name + ':'.

		// Value
		field.value = ia_string_get_until(requestBytes + readBytes, '\r');	
		if (readBytes + field.value.length == head_end_index
			|| requestBytes[readBytes + field.value.length + 1] != '\n')
		{
			http_server_send_response_status(server, client, "400 Bad Request", true);
			return false;
		}
		readBytes += field.value.length + 2; // Name + '\r\n'.

		// Trim field value's leading and trailing whitespaces & tabs.
		while (field.value.length > 0 
			&& (field.value.view_str[0] == ' ' 
				|| field.value.view_str[0] == '\t'))
		{
			field.value.view_str++;
			field.value.length--;
		}
		while (field.value.length > 0 
			&& (field.value.view_str[field.value.length - 1] == ' ' 
				|| field.value.view_str[field.value.length - 1] == '\t'))
		{
			field.value.length--;
		}
	}

	readBytes += 2; // Include closing "\r\n".

	out_request.total_size = readBytes;
	return true;
}

// Frees the memory being used by the passed request from the client's reception buffer.
// After calling this, the http_request structure should be considered freed and be disposed of.
void http_server_dispose_request(game_server& server, http_client& client, http_request& request)
{
	// Left-shift remaining bytes in client request buffer to the left.
	ia_memcpy(client.request.buff, client.request.buff + request.total_size, client.request.size - request.total_size);
	client.request.size -= request.total_size;
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

		if (client.response.in_flight)
		{
			// Progress response sending.
			http_server_progress_response(server, client);
		}
		else if (!client.being_dropped)
		{
			// Handle next request if one can be parsed from reception buffer.
			{
				http_request nextRequest;
				if (http_server_receive(server, client, nextRequest)
					&& nextRequest.method != http_request::METHOD::NONE)
				{
					http_server_handle_request(server, client, nextRequest);
					http_server_dispose_request(server, client, nextRequest);
				}
			}

			// Flag the client for dropping if it has been idle for too long (no bytes received, no response finished).
			if (server.uptime_ms - client.last_activity_ms > HTTP_CLIENT_IDLE_TIMEOUT_MS)
			{
				server.logf("HTTP SERVER", LOG_WARNING, "Client handle %d idle for over %llu ms. Dropping client.",
					client.client_handle.value, HTTP_CLIENT_IDLE_TIMEOUT_MS);
				client.being_dropped = true;
			}
		}
		else // Client has no response in flight and is being dropped... finish dropping them !
		{
			game_server_client_drop(server, client.client_handle);
		}
	}
}
