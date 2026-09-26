// Websocket specific functionality of the web server, for clients already upgraded to Websocket: frame encoding / decoding.
// Unity-compiled by web_server.cpp.

// TODO(Marc): Although the overall architecture is mine, this specific file is currently *mostly* authored by AI by telling it to
// build the websocket frame parsing system. I have reviewed it line by line and am satisfied that I understand it well, but at some point
// I want to look it over again once I have more experience applying it to the actual game. The way message queuing works probably needs to work quite differently.
// As of now what I did is perform complete reviews as the AI generated code for each functionality, performed manual corrections by hand and by smaller prompt, then
// went through it all again and added some comments.

#include "web_server.h"
#include "game_common/game_messages.h"

// BEGIN WEBSOCKET FRAMES

enum class WEBSOCKET_OPCODE : ui8
{
	CONTINUATION = 0x0,
	TEXT = 0x1,
	BINARY = 0x2,
	CLOSE = 0x8,
	PING = 0x9,
	PONG = 0xA,
};

// Result of attempting to read a frame, or of checking that it can be accepted.
// Every INVALID_* value is a frame that can't be accepted, and is the status code to send along with the Close frame answering it.
enum class WEBSOCKET_FRAME_PARSE_RESULT : ui16
{
	INCOMPLETE = 0,	// Not enough bytes to know whether the frame is valid, or to hold all of it.
	COMPLETE = 1,	// A whole, valid frame is present.

	INVALID_PROTOCOL_ERROR = 1002,		// The frame breaks the protocol rules.
	INVALID_UNSUPPORTED_DATA = 1003,	// The frame carries data we don't support (text, fragmented messages).
	INVALID_PAYLOAD = 1007,				// The frame payload isn't valid data of the type expected.
	INVALID_TOO_BIG = 1009,				// The frame is too large for us to hold.
};

// Returns whether the result is one of the INVALID_* values.
static inline bool websocket_frame_parse_result_is_invalid(WEBSOCKET_FRAME_PARSE_RESULT result)
{
	return (ui16)result >= (ui16)WEBSOCKET_FRAME_PARSE_RESULT::INVALID_PROTOCOL_ERROR;
}

// Description of a Websocket frame received from a client. The payload is not held, it follows the header in the bytes the frame was parsed from.
struct websocket_frame
{
	bool fin; // Is this the final frame of a message ?
	WEBSOCKET_OPCODE opcode;
	ui8 mask_key[4];
	ui32 header_size; // Bytes before the payload: base header, extended length and masking key.
	ui32 payload_size;
	ui32 total_size; // header_size + payload_size.
};

// Attempts to read a frame at the start of the given bytes, as sent by a client.
// A frame is found invalid as soon as enough bytes are there to tell, and a frame larger than max_frame_size is rejected before its payload even arrives.
// out_frame is only valid when the result is COMPLETE.
static WEBSOCKET_FRAME_PARSE_RESULT websocket_parse_frame(const ui8* bytes, ui32 size, ui32 max_frame_size,
	websocket_frame& out_frame)
{
	out_frame = {};

	// Size check for basic header bytes.
	if (size < 2) return WEBSOCKET_FRAME_PARSE_RESULT::INCOMPLETE;

	// Is the frame split into multiple part ? TODO(Marc): We may need to support it in the future but I don't expect client messages will ever be that big.
	bool fin = (bytes[0] & 0x80) != 0;

	ui8 reservedBits = bytes[0] & 0x70;

	// Opcode = least significant 4 bits of first byte.
	ui8 opcode = bytes[0] & 0x0F;

	// Are the payload bytes masked ?
	bool masked = (bytes[1] & 0x80) != 0;

	// Second byte, ignoring most significant bit. [0, 127]
	ui8 shortLength = bytes[1] & 0x7F;

	// Everything that can be checked from the first two bytes.

	bool validOpcode = opcode == (ui8)WEBSOCKET_OPCODE::CONTINUATION
		|| opcode == (ui8)WEBSOCKET_OPCODE::TEXT
		|| opcode == (ui8)WEBSOCKET_OPCODE::BINARY
		|| opcode == (ui8)WEBSOCKET_OPCODE::CLOSE
		|| opcode == (ui8)WEBSOCKET_OPCODE::PING
		|| opcode == (ui8)WEBSOCKET_OPCODE::PONG;

	bool isControl = opcode >= (ui8)WEBSOCKET_OPCODE::CLOSE;

	if (reservedBits != 0 // No extension was negotiated, so reserved bits must be 0.
		|| !masked // Clients must mask their frames.
		|| !validOpcode
		|| (isControl && !fin) // Control frames can't be fragmented...
		|| (isControl && shortLength > 125)) // ... and are limited to 125 bytes of payload.
	{
		return WEBSOCKET_FRAME_PARSE_RESULT::INVALID_PROTOCOL_ERROR;
	}

	// Payload length: either directly in the second byte, or in the following 2 or 8 bytes (big-endian).
	// The last two values of the short length values are used to determine that.

	ui32 headerSize = 2;
	ui64 payloadSize = shortLength;

	if (shortLength == 126) // All bits set except first = length is over the next two bytes (and header grows to encompass them).
	{
		// Size check for 2 size bytes.
		if (size < 4) return WEBSOCKET_FRAME_PARSE_RESULT::INCOMPLETE;

		payloadSize = ((ui64)bytes[2] << 8) | bytes[3];
		headerSize = 4;

		if (payloadSize < 126) // Lengths must use the shortest possible encoding.
		{
			return WEBSOCKET_FRAME_PARSE_RESULT::INVALID_PROTOCOL_ERROR;
		}
	}
	else if (shortLength == 127) // All bits set = length is over the next 8 bytes (and header grows to encompass them).
	{
		// Size check 8 for size bytes.
		if (size < 10) return WEBSOCKET_FRAME_PARSE_RESULT::INCOMPLETE;

		payloadSize = 0;
		for (ui8 byteIndex = 0; byteIndex < 8; byteIndex++)
		{
			payloadSize = (payloadSize << 8) | bytes[2 + byteIndex];
		}
		headerSize = 10;

		if ((payloadSize >> 63) != 0 // Most significant bit must be 0.
			|| payloadSize <= 0xFFFF) // Lengths must use the shortest possible encoding.
		{
			return WEBSOCKET_FRAME_PARSE_RESULT::INVALID_PROTOCOL_ERROR;
		}
	}

	// Grow header to encompass masking key that always comes after size bytes.
	headerSize += 4;

	// Size check for total header size including the masking key.
	if (size < headerSize) return WEBSOCKET_FRAME_PARSE_RESULT::INCOMPLETE;

	if ((ui64)headerSize + payloadSize > max_frame_size)
	{
		return WEBSOCKET_FRAME_PARSE_RESULT::INVALID_TOO_BIG;
	}

	if (size < headerSize + payloadSize) return WEBSOCKET_FRAME_PARSE_RESULT::INCOMPLETE;

	// Extract frame structure values from the header.

	out_frame.fin = fin;
	out_frame.opcode = (WEBSOCKET_OPCODE)opcode;
	ia_memcpy(out_frame.mask_key, bytes + headerSize - 4, 4);
	out_frame.header_size = headerSize;
	out_frame.payload_size = (ui32)payloadSize;
	out_frame.total_size = headerSize + (ui32)payloadSize;

	return WEBSOCKET_FRAME_PARSE_RESULT::COMPLETE;
}

// Unmasks a frame payload in place.
static void websocket_unmask_payload(ui8* payload, ui32 payload_size, const ui8* mask_key)
{
	for (ui32 byteIndex = 0; byteIndex < payload_size; byteIndex++)
	{
		payload[byteIndex] ^= mask_key[byteIndex & 3];
	}
}

// END WEBSOCKET FRAMES

// Appends a frame, as sent by the server (unmasked, never fragmented), to the client's sending buffer.
// The frame is put together in full, contiguously, so it's handed to the platform in a single call and can never end up half sent.
// Returns false, adding nothing, if it doesn't fit in the room left in the buffer.
static bool web_server_websocket_queue_frame(web_server_client& web_client,
	WEBSOCKET_OPCODE opcode, const ui8* payload, ui32 payload_size)
{
	websocket_client& websocket = web_client.websocket;

	ui8 header[10];
	ui32 headerSize = 0;

	header[headerSize++] = 0x80 | (ui8)opcode; // FIN + opcode. TODO(Marc): Support multi-part frames !
	if (payload_size < 126)
	{
		header[headerSize++] = (ui8)payload_size; // Encode over a single byte.
	}
	else if (payload_size <= 0xFFFF)
	{
		// Leave first byte on 126 as indication, then encode over next 2 bytes.
		header[headerSize++] = 126;
		header[headerSize++] = (ui8)(payload_size >> 8);
		header[headerSize++] = (ui8)(payload_size & 0xFF);
	}
	else
	{
		// Leave first byte on 127 as indication, then encode over next 8 bytes.
		header[headerSize++] = 127;
		for (ui8 byte = 0; byte < 8; byte++)
		{
			header[headerSize++] = (ui8)(((ui64)payload_size >> (52 - byte * 8)) & 0xFF);
		}
	}

	if (headerSize + payload_size > WEBSOCKET_SEND_BUFFER_SIZE - websocket.sending.size) return false;

	ia_memcpy(websocket.sending.buff + websocket.sending.size, header, headerSize);
	ia_memcpy(websocket.sending.buff + websocket.sending.size + headerSize, payload, payload_size);
	websocket.sending.size += headerSize + payload_size;

	return true;
}

// Queues up a Close frame answering an invalid frame, with the result as its status code, and flags the client for dropping once it's been sent.
static void web_server_websocket_close_client(game_server& server, web_server_client& web_client, WEBSOCKET_FRAME_PARSE_RESULT invalid_result)
{
	ASSERT(websocket_frame_parse_result_is_invalid(invalid_result));

	ui8 payload[2] = { (ui8)((ui16)invalid_result >> 8), (ui8)((ui16)invalid_result & 0xFF) };
	web_server_websocket_queue_frame(web_client, WEBSOCKET_OPCODE::CLOSE, payload, sizeof(payload));

	web_client.in_drop = true;

	server.logf("WEB SERVER", LOG_WARNING, "Closing Websocket client %d with code %d.", web_client.client_handle.value, (i32)invalid_result);
}

// Removes bytes from the reception buffer, shifting everything after them towards the start.
static void web_server_websocket_remove_reception_bytes(websocket_client& websocket, ui32 offset, ui32 byte_count)
{
	ASSERT(offset + byte_count <= websocket.reception.size);

	// TODO(Marc): Redesign reception buffering. Making it a ring buffer could work.
	ia_memcpy(websocket.reception.buff + offset, websocket.reception.buff + offset + byte_count, websocket.reception.size - offset - byte_count);
	websocket.reception.size -= byte_count;
}

bool web_server_websocket_client_send_message(game_server_client& client, const game_message_header& message)
{
	ASSERT(client.connection_context != nullptr);
	web_server_client& web_client = *(web_server_client*)client.connection_context;
	ASSERT(web_client.is_websocket());

	// Not taking any more messages once the client is on its way out.
	if (web_client.in_drop) return false;

	// The whole game message, header included, is the payload of a single binary frame. It's added to the sending buffer in full, along with the Websocket framing,
	// and goes out to the platform on the next tick, together with anything else that was queued up.
	ui32 messageSize = sizeof(game_message_header) + message.payloadSize;
	ASSERT_MSG(messageSize + 4 <= WEBSOCKET_SEND_BUFFER_SIZE, "Game message of %d bytes can never fit a Websocket frame in the sending buffer.", messageSize);

	// Returns false if there's no room left in the sending buffer right now. The message can be sent again once it has emptied.
	return web_server_websocket_queue_frame(web_client, WEBSOCKET_OPCODE::BINARY, (const ui8*)&message, messageSize);
}

bool web_server_websocket_client_peek_message(game_server_client& client, game_message_header*& out_message_ptr)
{
	ASSERT(client.connection_context != nullptr);
	web_server_client& web_client = *(web_server_client*)client.connection_context;
	ASSERT(web_client.is_websocket());

	websocket_client& websocket = web_client.websocket;
	if (websocket.reception.queued_size == 0) return false;

	// Queued frames were already validated and unmasked when they were received. The first of them is at the start of the buffer:
	// reading its header again gives us where its payload, which is the game message, starts.
	websocket_frame frame;
	WEBSOCKET_FRAME_PARSE_RESULT result = websocket_parse_frame(websocket.reception.buff, websocket.reception.queued_size, WEB_CLIENT_RECEPTION_BUFFER_SIZE, frame);
	ASSERT(result == WEBSOCKET_FRAME_PARSE_RESULT::COMPLETE);

	out_message_ptr = (game_message_header*)(websocket.reception.buff + frame.header_size);
	return true;
}

void web_server_websocket_client_consume_message(game_server_client& client)
{
	ASSERT(client.connection_context != nullptr);
	web_server_client& web_client = *(web_server_client*)client.connection_context;
	ASSERT(web_client.is_websocket());

	websocket_client& websocket = web_client.websocket;
	if (websocket.reception.queued_size == 0) return;

	websocket_frame frame;
	WEBSOCKET_FRAME_PARSE_RESULT result = websocket_parse_frame(websocket.reception.buff, websocket.reception.queued_size, WEB_CLIENT_RECEPTION_BUFFER_SIZE, frame);
	ASSERT(result == WEBSOCKET_FRAME_PARSE_RESULT::COMPLETE);

	// The frame is at the start of the buffer, so the queued frames after it and the yet-to-be-processed bytes all move up together.
	web_server_websocket_remove_reception_bytes(websocket, 0, frame.total_size);
	websocket.reception.queued_size -= frame.total_size;
}

void web_server_websocket_on_client_promotion(game_server& server, web_server_client& web_client)
{
	ASSERT(web_client.state == web_server_client::STATE::IN_UPGRADE_WEBSOCKET);
	web_client.state = web_server_client::STATE::ACTIVE_WEBSOCKET;

	web_client.last_activity_ms = server.uptime_ms;
	web_client.last_send_progress_ms = server.uptime_ms;

	// Reception buffer:
	// Perform a "soft change" without resetting all of the underlying memory. Since the http reception buffer is at the same offset as the websocket reception buffer,
	// the transfer of data between the two is automatic.
	// Any bytes it holds are raw Websocket bytes received after the http request, and none of them have been processed yet.
	web_client.websocket.reception.queued_size = 0;

	// Zero out the send buffer.
	web_client.websocket.sending = {};

	// Promote game server client, assigning it the send, peek & consume functions to use.
	game_server_promote_game_client(server, web_client.client_handle,
		web_server_websocket_client_send_message, web_server_websocket_client_peek_message, web_server_websocket_client_consume_message);

	// Done !
}

// Pushes the bytes waiting in the sending buffer to the platform. The platform takes them all or none, so whatever couldn't go through stays for the next tick.
void web_server_websocket_client_sending(game_server& server, web_server_client& web_client)
{
	websocket_client& websocket = web_client.websocket;
	ASSERT(websocket.sending.size > 0);

	if (game_server_client_send_net_bytes(server, web_client.client_handle, websocket.sending.buff, websocket.sending.size))
	{
		websocket.sending.size = 0;
		websocket.sending.blocked_since_ms = 0;
	}
	else if (websocket.sending.blocked_since_ms == 0)
	{
		// First time the platform refuses the bytes: start measuring how long it keeps doing so.
		websocket.sending.blocked_since_ms = server.uptime_ms;
	}
	else if (server.uptime_ms - websocket.sending.blocked_since_ms > HTTP_SEND_STALL_TIMEOUT_MS)
	{
		server.logf("WEB SERVER", LOG_ERROR, "Sending to Websocket client handle %d stalled, closing.", web_client.client_handle.value);
		web_client.in_drop = true;
	}
}

// Receives bytes from the platform, then processes as many complete frames as the reception buffer holds:
// - Control frames (ping, pong, close) are handled right away and removed from the buffer.
// - Binary frames carry game messages. They're validated, unmasked in place, and stay in the buffer, queued up until the game code consumes them.
// - Anything else is answered with a Close frame.
void web_server_websocket_client_reception(game_server& server, web_server_client& web_client)
{
	websocket_client& websocket = web_client.websocket;

	// Fill reception buffer if anything is available platform-side.
	if (websocket.reception.size < WEB_CLIENT_RECEPTION_BUFFER_SIZE)
	{
		ui32 receivedBytes = game_server_client_receive_net_bytes(server, web_client.client_handle,
			websocket.reception.buff + websocket.reception.size, WEB_CLIENT_RECEPTION_BUFFER_SIZE - websocket.reception.size);

		if (receivedBytes > 0)
		{
			websocket.reception.size += receivedBytes;
			web_client.last_activity_ms = server.uptime_ms;
		}
	}

	// Frames before queued_size are queued game messages. Processing picks up right after them.
	while (!web_client.in_drop && websocket.reception.queued_size < websocket.reception.size)
	{
		ui32 frameOffset = websocket.reception.queued_size;
		ui8* frameBytes = websocket.reception.buff + frameOffset;

		websocket_frame frame;

		// Parse frame structure so control code, close code, payload... can be determined.
		WEBSOCKET_FRAME_PARSE_RESULT result = websocket_parse_frame(frameBytes, websocket.reception.size - frameOffset, WEB_CLIENT_RECEPTION_BUFFER_SIZE, frame);

		if (result == WEBSOCKET_FRAME_PARSE_RESULT::INCOMPLETE) break; // Wait for the rest of the frame.

		if (websocket_frame_parse_result_is_invalid(result))
		{
			web_server_websocket_close_client(server, web_client, result);
			break;
		}

		ui8* payload = frameBytes + frame.header_size;
		websocket_unmask_payload(payload, frame.payload_size, frame.mask_key);

		switch (frame.opcode)
		{
		case WEBSOCKET_OPCODE::PING:
			// Answer with a pong repeating the ping's payload. If there's no room to send it right now, the ping is simply ignored.
			web_server_websocket_queue_frame(web_client, WEBSOCKET_OPCODE::PONG, payload, frame.payload_size);
			web_server_websocket_remove_reception_bytes(websocket, frameOffset, frame.total_size);
			break;
		case WEBSOCKET_OPCODE::PONG:
			// Nothing to do: receiving it already counted as activity.
			web_server_websocket_remove_reception_bytes(websocket, frameOffset, frame.total_size);
			break;
		case WEBSOCKET_OPCODE::CLOSE:
			// Answer with a Close frame repeating the status code if there was one, then let the client be dropped once it's sent.
			web_server_websocket_queue_frame(web_client, WEBSOCKET_OPCODE::CLOSE, payload, (frame.payload_size >= 2) ? 2 : 0);
			web_server_websocket_remove_reception_bytes(websocket, frameOffset, frame.total_size);
			web_client.in_drop = true;
			break;
		case WEBSOCKET_OPCODE::BINARY:
		{
			// Fragmented messages are not supported: they would have to be reassembled.
			if (!frame.fin)
			{
				web_server_websocket_close_client(server, web_client, WEBSOCKET_FRAME_PARSE_RESULT::INVALID_UNSUPPORTED_DATA);
				break;
			}

			// The frame payload is a game message: it must at least hold a message header, and the sizes must agree.
			game_message_header* message = (game_message_header*)payload;
			if (frame.payload_size < sizeof(game_message_header)
				|| sizeof(game_message_header) + message->payloadSize != frame.payload_size)
			{
				web_server_websocket_close_client(server, web_client, WEBSOCKET_FRAME_PARSE_RESULT::INVALID_PAYLOAD);
				break;
			}

			// Valid game message: keep it in the buffer and move on to what follows.
			websocket.reception.queued_size += frame.total_size;
			break;
		}
		case WEBSOCKET_OPCODE::TEXT:
		case WEBSOCKET_OPCODE::CONTINUATION:
		default:
			// Game messages are binary, and fragmented messages are not supported.
			web_server_websocket_close_client(server, web_client, WEBSOCKET_FRAME_PARSE_RESULT::INVALID_UNSUPPORTED_DATA);
			break;
		}
	}
}

void web_server_websocket_tick_client(game_server& server, web_server_client& client)
{
	ASSERT(client.is_websocket());

	if (client.websocket.sending.size > 0)
	{
		web_server_websocket_client_sending(server, client);
	}
	else if (client.in_drop)
	{
		game_server_client_drop(server, client.client_handle);
		return;
	}

	if (!client.in_drop)
	{
		web_server_websocket_client_reception(server, client);

		// If still not in drop after reception, check if we've gone past timeout.
		if (!client.in_drop)
		{
			// Flag the client for dropping if it has been idle for too long (no bytes received).
			if (server.uptime_ms - client.last_activity_ms > WEB_CLIENT_TIMEOUT_MS)
			{
				server.logf("WEB SERVER", LOG_WARNING, "Client handle %d idle for over %llu ms. Dropping client.",
					client.client_handle.value, WEB_CLIENT_TIMEOUT_MS);
				client.in_drop = true;
			}
		}
	}
}
