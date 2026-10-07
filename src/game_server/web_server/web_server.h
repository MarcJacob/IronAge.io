// Symbol declarations for the web server component of the game server.
// Shared between the main web server file and its http & websocket specific files.

#ifndef WEB_SERVER_INCLUDED
#define WEB_SERVER_INCLUDED

#include "core.h"

#include "../game_server.h"
#include "../game_server_clients.h"
#include "core/memory.h"

// Static configuration of Web Server.
// TODO(Marc): Add to runtime configuration system. Substructure of game server initialization ?

static constexpr ui64 WEB_SERVER_TOTAL_MEM = MiB(128);
static constexpr ui64 WEB_SERVER_TOTAL_FILE_DATA_MEM = MiB(96);

static constexpr char WEB_ROOT_FOLDER[] = "./web_root"; // Resources-folder-relative path of serveable files.

static constexpr ui16 WEB_SERVER_MAX_CLIENTS = 64;
static constexpr ui16 WEB_SERVER_MAX_FILES = 32; // Max number of files that can be preloaded for serving.

static constexpr ui32 WEB_CLIENT_RECEPTION_BUFFER_SIZE = 2048;
static constexpr ui32 WEBSOCKET_CLIENT_MESSAGES_RECEPTION_BUFFER_SIZE = 1024;

static constexpr time_ms HTTP_CLIENT_TIMEOUT_MS = 2000; // HTTP connection with no request / activity for this long gets closed.
static constexpr time_ms WEBSOCKET_CLIENT_TIMEOUT_MS = 6000; // Websocket connection that sent nothing (not even a pong) for this long gets closed.
static constexpr time_ms WEBSOCKET_PING_PERIOD_MS = 2000; // A websocket client is sent a ping this often.
														  // Keep it well under the timeout, so that a few pings can go unanswered before the client is dropped.

static constexpr ui32 HTTP_CLIENT_MAX_REQUEST_TARGET_LEN = 256; // Maximum number of characters in a valid http request target name.
static constexpr ui32 HTTP_RESPONSE_HEAD_BUFFER_SIZE = 256;

static constexpr ui32 HTTP_RECEPTION_CHUNK_SIZE = 1024;
static constexpr ui32 HTTP_SEND_CHUNK_SIZE = 4096;
static constexpr ui32 HTTP_PATH_BUFFER_SIZE = 256;
static constexpr time_ms HTTP_SEND_STALL_TIMEOUT_MS = 10000; // Response making no progress for this long gets its httpConnection closed.

static constexpr ui32 WEBSOCKET_RECEPTION_CHUNK_SIZE = 1024;
static constexpr ui32 WEBSOCKET_SEND_BUFFER_SIZE = 4096;

// BEGIN CLIENT STRUCTURES
// @TODO(Marc): Rename those to "connections" instead as it makes a lot more sense with their actual role.

// Upgraded client, recognized as a Game Client by the Game Server.
// Holds the necessary data to appropriately encode / decode Websocket frames for reading by the general server network protocol code.
// Passed as Game Client Connection Context in relevant functions.
struct websocket_client
{
	// Bytes reception buffer.
	struct
	{
		static_mem_arena<WEB_CLIENT_RECEPTION_BUFFER_SIZE> buffer; // Reception buffer for unprocessed network bytes.
        static_mem_arena<WEBSOCKET_CLIENT_MESSAGES_RECEPTION_BUFFER_SIZE> processed_buffer; // Processed Game Message bytes.
	} reception; // NOTE(Marc): This must remain at the same offset as the equivalent buffer in http_client so it doesn't need a copy when upgrading a client !

	// Bytes sending buffer.
	struct
	{
		static_mem_arena<WEBSOCKET_SEND_BUFFER_SIZE> buff; // Sending buffer for this client. Contains queued Game Messages.
		time_ms blocked_since_ms; // Time at which the platform first refused the bytes currently awaiting dispatch. 0 if it has not refused them.
		time_ms last_ping_ms; // Last time a ping was queued up for this client. A ping goes out once WEBSOCKET_PING_PERIOD_MS have gone by since this or since
							  // the client last sent anything (web_server_client::last_activity_ms), whichever is more recent.
	} sending;
};

// Holds the state of an active http client.
struct http_client
{
	// Request reception buffer. The data is buffered and interpreted as characters.
	struct
	{
        static_mem_arena<WEB_CLIENT_RECEPTION_BUFFER_SIZE> buffer;
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

// Discriminated union wrapper of possible client types for the web server.
struct web_server_client
{
	enum class STATE : ui8
	{
		INACTIVE, // This is a free slot for a new connection to use.

		ACTIVE, // Not directly used, indicates the beginning of state values that flags the client as active.
		ACTIVE_HTTP, // Active client in HTTP dialogue with the server.
		ACTIVE_WEBSOCKET, // Active client in Websocket dialogue with the server. Registered as a Game Client connection with the Game Server.

		IN_UPGRADE_WEBSOCKET, // Active client we've stopped listening to and will upgrade to Websocket once we're done sending them data.

		STATE_COUNT,
	} state;

	// Is this an active client connection or one that can be used to hold a new connection ?
	inline bool is_active() const { return (ui8)state >= (ui8)STATE::ACTIVE && (ui8)state < (ui8)STATE::STATE_COUNT; }

	bool in_drop; // Clients flagged as in drop will be dropped / disconnected once any remaining outbound data has been sent.

	game_server_client::client_handle client_handle; // Handle to related game server client.
	time_ms last_activity_ms; // Last time the client was opened, received bytes, or finished a response. Used to drop idle connections.
	time_ms last_send_progress_ms; // Last time the response started or made progress. Used to drop connections that stop accepting data.

	inline bool is_websocket() const { return state == STATE::ACTIVE_WEBSOCKET; }
	inline bool is_http() const { return !is_websocket() && state != STATE::INACTIVE; }
	union
	{
		http_client http;
		websocket_client websocket;
	};
};

// END CLIENT STRUCTURES

// A file preloaded in server memory at init, served as-is to requests for its target_name.
struct http_file
{
	ia_string_view resource_name; // Directly views into the resource file paths in main server.
	enum class CONTENT_TYPE : ui8
	{
		OCTET_STREAM, // Fallback for unsupported extensions.
		HTML,
		JS,
		CSS,
		WASM,
		TXT,
		ICO,
		SVG,
	};
	CONTENT_TYPE content_type;
	ui8* data;
	ui32 size;
};

// State of the web server sub-system of the game server.
// Holds a set of files preloaded in memory, to serve to prospective browser-based game clients,
// as well as the clients connected through http or websocket.
struct web_server
{
	// Client collections, by type.
	// Later we may want to have a shared memory pool for the two somehow. Unioning them might work.
	web_server_client clients[WEB_SERVER_MAX_CLIENTS];

	// General-purpose memory allocated to the web server.
	struct mem_arena memory;

	// Resources this server can serve.
	struct mem_arena file_data_memory; // Memory where file data is stored.
	http_file files[WEB_SERVER_MAX_FILES];
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

// BEGIN WEB SERVER MAIN FILE FUNCTIONS (web_server.cpp)


// Finds the preloaded file a request target ("/", "/src/main.js?x=1") asks for. Returns null if there is none.
const http_file* web_server_find_file(web_server& web, const ia_string_view& target);

// Resolves a content type to its MIME string.
ia_string_view web_server_get_content_type_string(http_file::CONTENT_TYPE type);

// Event handler for game server clients disconnecting: gives back the web server client structures the disconnected client was using.
void web_server_on_client_disconnected(game_server& server, game_server_client& client);

// Checks whether the bytes just received from an unknown client are understood by the web server (currently: the start of an HTTP request).
// If they are, the client is taken in by the web server and promoted to a NON_GAME_CLIENT.
// Returns whether the client was accepted. Bytes must fit the HTTP request buffer.
bool web_server_try_accept_client(game_server& server, game_server_client& client, const ui8* bytes, ui32 byte_count);

// Initializes web server.
web_server* web_server_init(game_server& server);

// (Re)Loads every resource file located in the resource folder/web root path specified in parameters. Fatal if any of them can't be loaded.
// The files are found via the game server's current Resource Files buffer, so it needs to be up-to-date with what is on platform.
void web_server_reload_files(game_server& server);

// Ticks web server.
void web_server_tick(game_server& server);

// END WEB SERVER MAIN FILE FUNCTIONS

// Runs one tick of http dialogue for a client in an http state: response progress, upgrade to websocket completion, request handling, dropping.
void web_server_http_tick_client(game_server& server, web_server_client& client);

// Called from http websocket upgrade function so websocket-sided initialization can kick in.
void web_server_websocket_on_client_promotion(game_server& server, web_server_client& client);

// Runs one tick of Websocket reception / sending handling for a client in a Websocket state.
void web_server_websocket_tick_client(game_server& server, web_server_client& client);

#endif // WEB_SERVER_INCLUDED
