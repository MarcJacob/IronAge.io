// Websocket specific functionality of the web server, for clients already upgraded to Websocket: frame encoding / decoding.
// Unity-compiled by web_server.cpp.

#include "web_server.h"

bool web_server_websocket_client_send_message(game_server_client& client, const game_message_header& message)
{
	// Find web client related to game server client and add the game message to the sending buffer in full along with the WebSocket framing required.

	return false;
}

bool web_server_websocket_client_receive_message(game_server_client& client, game_message_header*& out_message_ptr)
{
	// Find web client related to game server client, peek the reception buffer to see if a full frame has been received, and if yes,
	// interpret it has a game message.

	return false;
}

void web_server_websocket_on_client_promotion(game_server& server, web_server_client& web_client)
{
	ASSERT(web_client.state == web_server_client::STATE::IN_UPGRADE_WEBSOCKET);
	web_client.state = web_server_client::STATE::ACTIVE_WEBSOCKET;

	web_client.last_activity_ms = server.uptime_ms;

	// Reception buffer:
	// Perform a "soft change" without resetting all of the underlying memory. Since the http reception buffer is at the same offset as the websocket reception buffer,
	// the transfer of data between the two is automatic.

	// Zero out the send buffer.
	web_client.websocket.sending = {};

	// Promote game server client, assigning it the send & receive functions to use.
	game_server_promote_game_client(server, web_client.client_handle, 
		web_server_websocket_client_send_message, web_server_websocket_client_receive_message);

	// Done !
}

void web_server_websocket_client_sending(game_server& server, web_server_client& client)
{

}

void web_server_websocket_client_reception(game_server& server, web_server_client& client)
{

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
	}

	if (!client.in_drop)
	{
		web_server_websocket_client_reception(server, client);
	}
}
