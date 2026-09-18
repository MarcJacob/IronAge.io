// Main implementation file for the Game Server code.
// Must be linked or compiled into whatever platform layer is used.

#include "game_server/game_server_platform.h"
#include "game_server/game_server.h"

game_server* game_server_init(game_server_platform& platform, ui8* memory, ui64 memory_size)
{
	ASSERT_MSG(memory != nullptr && memory_size > 0, L"Game server requires at least 4 Gigabytes of memory !");

	// Allocate and initialize new game server at the start of memory.

	game_server* newServer = (game_server*)memory;

	newServer->memStart = memory + sizeof(game_server);
	newServer->memSize = memory_size - sizeof(game_server);

	return newServer;
}

void game_server_tick(game_server_platform& platform, game_server& server, float deltatime)
{
	platform.log_stdout(L"Test");
}