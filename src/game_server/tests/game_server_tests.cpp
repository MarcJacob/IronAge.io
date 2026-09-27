// Implementation of testing mode functionality for the server.

#include "game_server_tests.h"

#include "../game_server.h"

static void game_server_test_echo_game_client(game_server& server, game_server_client& client)
{
	game_message_header* message = nullptr;
	while (client.game_client_peek_message(message))
	{
		server.logf("TEST", "Client %d: message type %d, payload %d bytes.", client.handle.value, message->message_type, message->payloadSize);

		if (!client.game_client_send_message(*message)) break; // Sending buffer full: try again next tick.
		client.game_client_consume_message();
	}
}

void game_server_test_mode_tick(game_server& server)
{	
	// TEST: Run the shared test scenario in its own arena allocated from main memory,
	// dump the resulting match state to specified file and shut down.

	ASSERT(server.platform != nullptr);
	game_server_platform& platform = *server.platform;

	server.log("TEST", "Running in test scenario mode.\nRunning test scenario match...");

	// Alloc & build test scenario params.
	game_match_start_params* scenario_params = match_test_scenario_get_params(server.main_memory);
	ASSERT(scenario_params != nullptr);

	mem_arena scenario_memory = mem_arena_create_sub(server.main_memory, match_get_required_mem(*scenario_params));
	game_match* scenario_match = match_run_test_scenario(scenario_memory);
	ASSERT(scenario_match != nullptr);

	if (server.init_params.test_scenario_dump_filename == nullptr)
	{
		server.log("TEST", "No dump file specified. Going straight to shutdown.");
		platform.shutdown(0);
	}

	server.logf("TEST", "Dumping scenario match end state to file \"%s\".", server.init_params.test_scenario_dump_filename);

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

				ia_memcpy(state.dump_mem + state.dump_size, bytes, byte_count);
				state.dump_size += byte_count;
			};

		dump_stream.dump_func = dump_func;
		dump_stream.state = &dump_state;
		match_dump_gamestate(*scenario_match, dump_stream);

		// Write the snapshot so it can be compared with the one simulated by the web client.
		if (!platform.write_resource_file(server.init_params.test_scenario_dump_filename, dump_state.dump_mem, dump_state.dump_size))
		{
			server.log("TEST", LOG_ERROR, "Failed to write native snapshot file.");
		}
		else
		{
			server.log("TEST", LOG_SUCCESS, "Match ending state dumped successfully.");
		}
	}

	server.log("TEST", "Test over. Reverting to normal function...");
	server.init_params.run_test_scenario = false;
}

