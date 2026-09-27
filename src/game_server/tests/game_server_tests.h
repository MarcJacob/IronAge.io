// Symbol declarations for the Game Server test mode.

// NOTE/TODO(Marc): Just a "dump" for some testing symbols I wanted to move out of the main code for now.
// Need to work on designing a good testing system:
// - Startable from command line along with the server itself
// - Queueable tests and pipeable results (or just keep dumping to resource files)
// - Client-server tests clients can connect to with a special handshake
// - Auto-start clients ?

#ifndef GAME_SERVER_TESTS_INCLUDED
#define GAME_SERVER_TESTS_INCLUDED

struct game_server;
struct game_server_client;

// Checks the client for received messages and echoes them back with the exact same content.
static void game_server_test_echo_game_client(game_server& server, game_server_client& client);

// Alternative tick function ran by the server when in test mode.
void game_server_test_mode_tick(game_server& server);

#endif // GAME_SERVER_TESTS_INCLUDED

