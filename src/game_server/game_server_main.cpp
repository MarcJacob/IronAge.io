// Main implementation file for the Game Server code.
// Must be linked or compiled into whatever platform layer is used.

#include "game_server/game_server_platform.h"
#include "game_server.h"

// Unity-compile the Game Common code into the server.
#include "../game_common/game_common_main.cpp"

// Initializes a new match slot in the server from a piece of memory to use and a the slot index to (re)initialize.
// The slot must currently be uninitialized or awaiting post-match cleanup.
// If successful, the passed memory arena is now owned by the slot itself. The one passed as param should be discarded.
// 
// Returns pointer to newly-created slot for convenience.
// Returns nullptr in case of non-fatal error.
match_slot* game_server_init_match_slot(game_server& server, mem_arena& slot_mem, ui8 slot_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	if (slot.state != MATCH_SLOT_STATE::UNINITIALIZED && slot.state != MATCH_SLOT_STATE::AWAITING_CLEANUP)
	{
		server.platform->logf_stderr("Match slot index %d is already initialized (current state = %d).", slot_index, slot.state);
		return nullptr;
	}

	// If the slot is awaiting cleanup, we want to re-use the same memory, so ensure that we've passed the correct arena.
	// NOTE(Marc): Later this constraint will be removed once the memory management system is more sophisticated and we may start wanting to move slots around in memory.
	if (slot.state == MATCH_SLOT_STATE::AWAITING_CLEANUP)
	{
		ASSERT(slot_mem.mem_start == slot.slot_memory.mem_start);
	}

	server.platform->logf_stdout("Initializing server match slot index %d.", slot_index);

	// Zero out the slot and set it back to waiting, conserving only its arena.
	slot = {};
	slot.state = MATCH_SLOT_STATE::WAITING;
	slot.slot_memory = slot_mem;

	// From there the slot can be put into lobby mode to start accepting players, or directly have its parameters set and match started.
	return &slot;
}

game_server* game_server_init(game_server_platform& platform, game_server_init_params& init_params, ui8* memory, ui64 memory_size)
{
	ASSERT_MSG(memory != nullptr && memory_size > GiB(2), "Game server requires at least 2 Gibibytes of memory !");

	// Allocate and initialize new game server at the start of memory.
	game_server* newServer = (game_server*)memory;
	newServer->platform = &platform;
	newServer->init_params = init_params;

	// Create main arena allocator for the server. 
	// It will be split into various static "Sections" that each hold the data needed by a feature of the server.
	newServer->main_memory = mem_arena_create(memory + sizeof(game_server), memory_size - sizeof(game_server));

	// Initialize all match slots.

	newServer->match_slots = newServer->main_memory.alloc<match_slot>(newServer->init_params.match_slot_count);

	constexpr ui64 MEM_PER_SLOT = MiB(2); // TEMP(Marc): Just keep this number above the required memory for a match or a lobby, whichever is larger.
	for (ui8 matchSlotIndex = 0; matchSlotIndex < newServer->init_params.match_slot_count; matchSlotIndex++)
	{
		mem_arena slot_mem = mem_arena_create_sub(newServer->main_memory, MEM_PER_SLOT);	
		game_server_init_match_slot(*newServer, slot_mem, matchSlotIndex);
	}

	return newServer;
}

void game_server_tick(game_server& server, float deltatime)
{
	// ... for convenience.
	ASSERT(server.platform != nullptr);
	game_server_platform& platform = *server.platform;

	if (server.init_params.run_test_scenario)
	{
		// TEST: Run the shared test scenario in its own arena, dump the resulting match state straight to memory and shut down.

		platform.log_stdout("Game Server running in test scenario mode.\nRunning test scenario match...");

		game_match_create_params scenario_params = match_test_scenario_get_params();

		mem_arena scenario_memory = mem_arena_create_sub(server.main_memory, match_get_required_mem(scenario_params));
		game_match* scenario_match = match_run_test_scenario(scenario_memory);
		ASSERT(scenario_match != nullptr);

		if (server.init_params.test_scenario_dump_filename == nullptr)
		{
			platform.log_stdout("No dump file specified. Going straight to shutdown.");
			goto SERVER_SHUTDOWN;
		}

		platform.logf_stdout("Dumping scenario match end state to file \"%s\".", server.init_params.test_scenario_dump_filename);

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
			if (!platform.write_file(server.init_params.test_scenario_dump_filename, dump_state.dump_mem, dump_state.dump_size))
			{
				platform.log_stderr("Failed to write native snapshot file.");
				platform.shutdown(1);
			}
		}

		goto SERVER_SHUTDOWN;
	}

	if (server.shutdown_triggered)
	{
	SERVER_SHUTDOWN:
		platform.log_stdout("Game Server shutting down...");
		// TODO(Marc): Shut down work / checks to be done here (gracefully end connections / matches).
		platform.shutdown(0);
	}
}