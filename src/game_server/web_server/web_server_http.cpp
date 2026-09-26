// HTTP specific functionality of the web server: request reception & parsing, request handling, response sending,
// and the upgrade of http clients to websocket.
// Unity-compiled by web_server.cpp.

#include "web_server.h"

bool http_request_find_header_field(const http_request& request, const ia_string_view& field_name, http_request::header_field& out_field)
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
static const ui8 SUPPORTED_METHOD_COUNT = sizeof(HTTP_SUPPORTED_METHODS) / sizeof(http_supported_method);

bool web_server_http_recognize_request(const ui8* bytes, ui32 byte_count)
{
	ia_string_view methodName = ia_string_get_word_n((char*)bytes, byte_count);

	for (ui8 methodIndex = 0; methodIndex < SUPPORTED_METHOD_COUNT; methodIndex++)
	{
		const http_supported_method& method = HTTP_SUPPORTED_METHODS[methodIndex];

		if (methodName == method.method_name)
		{
			return true;
		}
	}

	return false;
}

void web_server_http_serve_content(game_server& server, web_server_client& web_client,
	const char* status, const char* content_type, const ui8* content, ui32 content_size)
{
	ASSERT(web_client.is_websocket() == false);
	http_client& client = web_client.http;

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
	web_client.last_send_progress_ms = server.uptime_ms;

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

void web_server_http_send_response_status(game_server& server, web_server_client& web_client,
	const char* status_msg, bool drop_client)
{
	ASSERT(web_client.is_websocket() == false);
	http_client& client = web_client.http;

	ASSERT_MSG(client.response.in_flight == false, "Attempted to re-send response data to HTTP client while a response was already in flight.");

	// Build response head section.
	client.response.head.str.length = 0; // TODO(Marc): String reset function.

	ui32 head_write_pos = 0;
	head_write_pos += ia_string_push(client.response.head.str, "HTTP/1.1 ");
	head_write_pos += ia_string_push(client.response.head.str, status_msg);
	head_write_pos += ia_string_push(client.response.head.str, "\r\n");

	if (drop_client)
	{
		web_client.in_drop = true;
		head_write_pos += ia_string_push(client.response.head.str, "Connection: Close\r\n");
	}
	else
	{
		head_write_pos += ia_string_push(client.response.head.str, "Content-Type: text/plain charset=utf-8\r\n");
		head_write_pos += ia_string_push(client.response.head.str, "Content-Length: 0\r\n");
	}

	head_write_pos += ia_string_push(client.response.head.str, "\r\n");

	web_client.last_activity_ms = server.uptime_ms;

	client.response.head.sent = 0;
	client.response.head.size = head_write_pos;
	client.response.body.buff = nullptr;
	client.response.body.sent = 0;
	client.response.body.size = 0;

	client.response.in_flight = true;
}

static void web_server_http_handle_request_get_file(game_server& server, web_server_client& web_client, http_request& request)
{
	const http_file* targetFile = nullptr;

	// Fetch file resource. If HEAD, send back only the metadata.
	targetFile = web_server_find_file(*server.web, request.target_name);
	if (targetFile == nullptr)
	{
		web_server_http_send_response_status(server, web_client, "404 Not Found", false);
		return;
	}

	if (request.method == http_request::METHOD::GET)
		web_server_http_serve_content(server, web_client, "200 Ok", targetFile->content_type, targetFile->data, targetFile->size);
	if (request.method == http_request::METHOD::HEAD)
		web_server_http_serve_content(server, web_client, "200 Ok", targetFile->content_type, nullptr, targetFile->size);
}

// Handles a request to upgrade to the websocket protocol.
// If successful, the server sends the handshake approval and flags the client to be upgraded to Websocket once it has been sent
// (see web_server_http_finish_upgrade_websocket). Otherwise, an error status is sent and the client is dropped.
static bool web_server_http_handle_request_upgrade_websocket(game_server& server, web_server_client& web_client, http_request& request)
{
	ASSERT(request.method == http_request::METHOD::GET);

	ASSERT(web_client.is_websocket() == false);
	http_client& client = web_client.http;

	// Handle websocket upgrade handshake. Check that all the necessary headers are in place,
	// determine response key and send back the upgrade handshake approval as a status line.
	// Send status 400 if anything goes wrong.

	using header_field = http_request::header_field;

	header_field connectionField;
	if (!http_request_find_header_field(request, "connection", connectionField)
		|| !ia_string_contains(connectionField.value, "Upgrade", false))
	{
		web_server_http_send_response_status(server, web_client, "400 Bad Request", true);
		return false;
	}

	header_field upgradeField;
	if (!http_request_find_header_field(request, "upgrade", upgradeField)
		|| !ia_string_equal(upgradeField.value, "websocket", false))
	{
		web_server_http_send_response_status(server, web_client, "400 Bad Request", true);
		return false;
	}

	header_field secWebsocketKeyField;
	header_field secWebsocketVersionField;
	if (!http_request_find_header_field(request, "sec-websocket-key", secWebsocketKeyField)
		|| !http_request_find_header_field(request, "sec-websocket-version", secWebsocketVersionField))
	{
		web_server_http_send_response_status(server, web_client, "400 Bad Request", true);
		return false;
	}

	// Let's treat the origin and host fields as optional for now.
	header_field originField;
	header_field hostField;
	http_request_find_header_field(request, "origin", originField);
	http_request_find_header_field(request, "host", hostField);

	// We need to determine the acceptance key.
	// Take the received key, add it together with a specific GUID and have it go through a SHA-1 hash.

	static constexpr ui16 WEBSOCKET_IN_KEY_MAX_LEN = 128;

	// Check key length and version value.
	if (secWebsocketKeyField.value.length > WEBSOCKET_IN_KEY_MAX_LEN)
	{
		web_server_http_send_response_status(server, web_client, "414 URL Too Long", true);
		return false;
	}
	else if (secWebsocketKeyField.value.length == 0)
	{
		web_server_http_send_response_status(server, web_client, "400 Bad Request", true);
		return false;
	}
	if (secWebsocketVersionField.value != "13")
	{
		web_server_http_send_response_status(server, web_client, "426 Upgrade Required", true);
		return false;
	}

	static constexpr char WEBSOCKET_HANDSHAKE_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	static constexpr ui16 WEBSOCKET_HANDSHAKE_GUID_LEN = sizeof(WEBSOCKET_HANDSHAKE_GUID) - 1;

	char key_processing_buff[WEBSOCKET_IN_KEY_MAX_LEN + WEBSOCKET_HANDSHAKE_GUID_LEN];
	ia_memcpy(key_processing_buff, secWebsocketKeyField.value.view_str, secWebsocketKeyField.value.length);
	ia_memcpy(key_processing_buff + secWebsocketKeyField.value.length, WEBSOCKET_HANDSHAKE_GUID, WEBSOCKET_HANDSHAKE_GUID_LEN);

	sha1_result hash = ia_sha1((ui8*)key_processing_buff, secWebsocketKeyField.value.length + WEBSOCKET_HANDSHAKE_GUID_LEN);

	// Encode result back to base64.
	char response_key_buff[(sizeof(hash.result) + 2) / 3 * 4 + 1];
	ui32 response_key_len = ia_base64_encode((ui8*)&hash, sizeof(hash.result), response_key_buff, sizeof(response_key_buff));
	ASSERT(response_key_len > 0);

	response_key_buff[response_key_len] = '\0';

	// Send response.

	ia_static_string<256> response_str = {};
	ia_string_push(response_str, "HTTP/1.1 101 Switching Protocols\r\n");
	ia_string_push(response_str, "Upgrade: websocket\r\n");
	ia_string_push(response_str, "Connection: Upgrade\r\n");
	ia_string_push(response_str, "Sec-Websocket-Accept: ");
	ia_string_push(response_str, response_key_buff);
	ia_string_push(response_str, "\r\n\r\n");
	// Let's just assume that the response_str has trailing zeroes so it can be used as a C string directly.

	// Build & send custom response head section.
	{
		ASSERT(client.response.in_flight == false);

		client.response.head.str.length = 0; // TODO(Marc): String reset function.

		ui32 response_size = 0;
		response_size += ia_string_push(client.response.head.str, response_str._str);

		web_client.last_activity_ms = server.uptime_ms;

		client.response.head.sent = 0;
		client.response.head.size = response_size;
		client.response.body.buff = nullptr;
		client.response.body.sent = 0;
		client.response.body.size = 0;

		client.response.in_flight = true;
	}

	server.logf("HTTP", LOG_SUCCESS, "Successful Websocket upgrade handshake with HTTP Client %d.", web_client.client_handle.value);
	server.log("HTTP", "\tFlagging Client for type upgrade to Websocket...");

	ASSERT(web_client.state == web_server_client::STATE::ACTIVE_HTTP);
	web_client.state = web_server_client::STATE::IN_UPGRADE_WEBSOCKET;
	return true;
}

// Completes the upgrade of a client that's done being sent its handshake approval: the client becomes a Websocket client.
// The web server client keeps its slot, so the game server client's connection context stays valid, but its http data is reset
// since the union storage now belongs to the websocket client.
static void web_server_http_finish_upgrade_websocket(game_server& server, web_server_client& web_client)
{
	ASSERT(web_client.state == web_server_client::STATE::IN_UPGRADE_WEBSOCKET);

	// Perform a "soft change" without resetting all of the underlying memory. Since the http reception buffer is at the same offset as the websocket reception buffer,
	// the transfer of data between the two is automatic.

	web_client.state = web_server_client::STATE::ACTIVE_WEBSOCKET;
	web_client.last_activity_ms = server.uptime_ms;

	// Zero out the send buffer.
	web_client.websocket.sending = {};

	server.logf("HTTP", LOG_SUCCESS, "Client connection %d upgraded to Websocket.", web_client.client_handle.value);
}

void web_server_http_handle_request(game_server& server, web_server_client& web_client, http_request& request)
{
	ASSERT(web_client.is_websocket() == false);

	// NOTE(Marc): Currently only GET and HEAD method requests are handled.
	// TODO(Marc): Per request type function ?

	http_request::header_field connectionField;

	switch (request.method)
	{
	case http_request::METHOD::GET:
		if (request.target_name == "/ws")
		{
			web_server_http_handle_request_upgrade_websocket(server, web_client, request);
			break;
		}
		// Fallthrough
	case http_request::METHOD::HEAD:
		web_server_http_handle_request_get_file(server, web_client, request);
		break;
	case http_request::METHOD::UNKNOWN:
		web_server_http_send_response_status(server, web_client, "501 Not Implemented", true);
		server.logf("WEB SERVER", LOG_WARNING, "Client handle %d requested unknown method. Closing client.", web_client.client_handle.value);
		break;
	case http_request::METHOD::UNSUPPORTED:
	default:
		web_server_http_send_response_status(server, web_client, "405 Method Not Allowed\r\nAllow: GET, HEAD", true);
		server.logf("WEB SERVER", LOG_WARNING, "Client handle %d requested unsupported method. Closing client.", web_client.client_handle.value);
		break;
	}

	// If the request has header field "Connection: Close" then we can drop the client.
	// This can only be applied if the client is specifically in the ACTIVE_HTTP state. Any other state prevents dropping here.
	if (web_client.state == web_server_client::STATE::ACTIVE_HTTP
		&& http_request_find_header_field(request, "Connection", connectionField)
		&& connectionField.value == "close")
	{
		web_client.in_drop = true;
	}
}

bool web_server_http_receive(game_server& server, web_server_client& web_client, http_request& out_request)
{
	ASSERT(web_client.is_websocket() == false);
	http_client& client = web_client.http;

	game_server_platform& platform = *server.platform;
	out_request = {};

	// Check that we're not about to overflow request buffer size. TODO(Marc): Discard previous requests if needed ?
	if (client.request.size < WEB_CLIENT_RECEPTION_BUFFER_SIZE - 1)
	{
		// Receive bytes on the httpConnection and place them in the request buffer.
		ui32 receivedBytes = game_server_client_receive_net_bytes(server, web_client.client_handle,
			(ui8*)client.request.buff + client.request.size, WEB_CLIENT_RECEPTION_BUFFER_SIZE - client.request.size - 1);

		if (receivedBytes > 0)
		{
			client.request.size += receivedBytes;
			web_client.last_activity_ms = server.uptime_ms;
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
		if (client.request.size >= WEB_CLIENT_RECEPTION_BUFFER_SIZE - 1)
		{
			web_server_http_send_response_status(server, web_client, "431 Request Header Fields Too Large", true);
			server.logf("WEB SERVER", LOG_WARNING, "Client handle %d sent a request head larger than %d bytes. Closing client.",
				web_client.client_handle.value, WEB_CLIENT_RECEPTION_BUFFER_SIZE);
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
		web_server_http_send_response_status(server, web_client, "400 Bad Request", true);
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
			web_server_http_send_response_status(server, web_client, "414 URL Too Long", true);
			return false;
		}

		readBytes += out_request.target_name.length;
		readBytes++; // Include expected space.
	}

	// Check HTTP version (only 1.1 is supported).
	if (readBytes >= head_end_index
		|| !ia_str_expect(requestBytes + readBytes, "HTTP/1.1\r\n"))
	{
		web_server_http_send_response_status(server, web_client, "505 HTTP Version Not Supported.", true);
		server.logf("WEB SERVER", LOG_WARNING, "Client handle %d used wrong HTTP version. Closing client.", web_client.client_handle.value);
		return false;
	}
	readBytes += sizeof("HTTP/1.1\r\n") - 1;

	// Parse header fields.
	while(readBytes < head_end_index - 3)
	{
		if (out_request.header.field_count == http_request::MAX_HEADER_FIELD_COUNT)
		{
			web_server_http_send_response_status(server, web_client, "431 Too Many Headers", true);
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
			web_server_http_send_response_status(server, web_client, "400 Bad Request", true);
			return false;
		}
		readBytes += field.name.length + 1; // Name + ':'.

		// Value
		field.value = ia_string_get_until(requestBytes + readBytes, '\r');
		if (readBytes + field.value.length == head_end_index
			|| requestBytes[readBytes + field.value.length + 1] != '\n')
		{
			web_server_http_send_response_status(server, web_client, "400 Bad Request", true);
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

void web_server_http_dispose_request(game_server& server, web_server_client& web_client, http_request& request)
{
	ASSERT(web_client.is_websocket() == false);

	// Left-shift remaining bytes in client request buffer to the left.
	ia_memcpy(web_client.http.request.buff, web_client.http.request.buff + request.total_size, web_client.http.request.size - request.total_size);
	web_client.http.request.size -= request.total_size;
}

void web_server_http_progress_response(game_server& server, web_server_client& web_client)
{
	game_server_platform& platform = *server.platform;

	// Send fails if the platform send buffer is full or the httpConnection is closed. Either way, keep trying until it makes no progress for too long.
	bool stalled = false;

	// Try to send more data over.
	while (web_client.http.response.head.sent < web_client.http.response.head.size)
	{
		ui32 chunkSize = ia_min(HTTP_SEND_CHUNK_SIZE, web_client.http.response.head.size - web_client.http.response.head.sent);

		if (game_server_client_send_net_bytes(server, web_client.client_handle,
			(const ui8*)web_client.http.response.head.str._str + web_client.http.response.head.sent, chunkSize))
		{
			web_client.http.response.head.sent += chunkSize;
			web_client.last_send_progress_ms = server.uptime_ms;
		}
		else
			stalled = true;
	}

	// Head was sent and we're not stalled -> continue to body.
	while (!stalled && web_client.http.response.body.sent < web_client.http.response.body.size)
	{
		ui32 chunk = web_client.http.response.body.size - web_client.http.response.body.sent;
		if (chunk > HTTP_SEND_CHUNK_SIZE) chunk = HTTP_SEND_CHUNK_SIZE;

		if (game_server_client_send_net_bytes(server, web_client.client_handle, web_client.http.response.body.buff + web_client.http.response.body.sent, chunk))
		{
			web_client.http.response.body.sent += chunk;
			web_client.last_send_progress_ms = server.uptime_ms;
		}
		else
			stalled = true;
	}

	// When stalled on head or body, determine how long since last recorded transfer activity and if past a threshold, drop the transfer & httpConnection.
	if (stalled)
	{
		if (server.uptime_ms - web_client.last_send_progress_ms > HTTP_SEND_STALL_TIMEOUT_MS)
		{
			server.logf("WEB SERVER", LOG_ERROR, "Response on client handle %d stalled, closing.", web_client.client_handle.value);
			game_server_client_drop(server, web_client.client_handle);
			web_client.in_drop = true;
		}
		return;
	}

	// Response fully handed to the platform. Ready for the next request on this client.
	web_client.http.response.in_flight = false;
	web_client.last_activity_ms = server.uptime_ms;

	// Drop the client if flagged for closing.
	if (web_client.in_drop)
	{
		game_server_client_drop(server, web_client.client_handle);
	}
}

void web_server_http_tick_client(game_server& server, web_server_client& client)
{
	ASSERT(client.is_websocket() == false);

	if (client.http.response.in_flight)
	{
		// Progress response sending.
		web_server_http_progress_response(server, client);
	}
	else if (client.state == web_server_client::STATE::IN_UPGRADE_WEBSOCKET)
	{
		// The handshake approval is done being sent, complete the upgrade.
		// This is checked before receiving a new request, otherwise the client's next bytes would be parsed as http.
		web_server_http_finish_upgrade_websocket(server, client);
	}
	else if (!client.in_drop)
	{
		// Handle next request if one can be parsed from reception buffer.
		{
			http_request nextRequest;
			if (web_server_http_receive(server, client, nextRequest)
				&& nextRequest.method != http_request::METHOD::NONE)
			{
				web_server_http_handle_request(server, client, nextRequest);
				web_server_http_dispose_request(server, client, nextRequest);
			}
		}

		// Flag the client for dropping if it has been idle for too long (no bytes received, no response finished).
		if (server.uptime_ms - client.last_activity_ms > HTTP_CLIENT_IDLE_TIMEOUT_MS)
		{
			server.logf("WEB SERVER", LOG_WARNING, "Client handle %d idle for over %llu ms. Dropping client.",
				client.client_handle.value, HTTP_CLIENT_IDLE_TIMEOUT_MS);
			client.in_drop = true;
		}
	}
	else  // Client has no response in flight, no upgrade in progress and is being dropped... finish dropping them !
	{
		game_server_client_drop(server, client.client_handle);
	}
}
