// Main implementation file for the Game Server code.
// Must be linked or compiled into whatever platform layer is used.

#include "game_server/game_server_platform.h"
#include "game_server/game_server.h"

// Unity-compile the Game Common code into the server.
#include "../game_common/game_common_main.cpp"

game_server* game_server_init(game_server_platform& platform, ui8* memory, ui64 memory_size)
{
	ASSERT_MSG(memory != nullptr && memory_size > GiB(2), L"Game server requires at least 2 Gibibytes of memory !");

	// Allocate and initialize new game server at the start of memory.
	game_server* newServer = (game_server*)memory;

	// Create main arena allocator for the server. 
	// It will be split into various static "Sections" that each hold the data needed by a feature of the server.
	newServer->main_memory = mem_arena_create(memory + sizeof(game_server), memory_size - sizeof(game_server));

	return newServer;
}

void game_server_tick(game_server_platform& platform, game_server& server, float deltatime)
{
	// TEST: Run the shared test scenario in its own arena, dump the resulting match state straight to memory and shut down.
	{
		mem_arena scenario_memory = mem_arena_create_sub(server.main_memory, MiB(512));
		game_match* scenario_match = match_run_test_scenario(scenario_memory);
		ASSERT(scenario_match != nullptr);

		match_dump_stream dump_stream = { };
		ui64 dumpSize = match_dump_gamestate(*scenario_match, dump_stream);

		struct dump_state_data
		{
			ui8* dump_mem;
			ui64 dump_size;
		} dump_state = {0};

		if (dumpSize > 0)
		{

			dump_state.dump_mem = server.main_memory.alloc<ui8>(dumpSize);
			ASSERT(dump_state.dump_mem != nullptr);

			auto dump_func = [](void* dump_state, ui8* bytes, ui64 byte_count)
				{
					dump_state_data& state = *(dump_state_data*)(dump_state);

					gcommon_memcpy(state.dump_mem + state.dump_size, bytes, byte_count);
					state.dump_size += byte_count;
				};

			dump_stream.dump_func = dump_func;
			dump_stream.state = &dump_state;
			match_dump_gamestate(*scenario_match, dump_stream);

			// Write the snapshot so it can be compared with the one simulated by the web client.
			if (!platform.write_file("snapshot_native.bin", dump_state.dump_mem, dump_state.dump_size))
			{
				platform.log_stderr(L"Failed to write native snapshot file.");
				platform.shutdown(1);
			}
		}

		platform.shutdown(0);
	}
}