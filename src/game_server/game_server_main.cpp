// Main implementation file for the Game Server code.
// Must be linked or compiled into whatever platform layer is used.

#include "game_server/game_server_platform.h"
#include "game_server.h"

// Unity-compile the Game Common code into the server.
#include "../game_common/game_common_main.cpp"

// Unity-compile the HTTP file serving code.
#include "game_server_http.cpp"

// Initializes a new match slot in the server from a piece of memory to use and the slot index to initialize.
// The slot must currently be uninitialized.
// If successful, the passed memory arena is now owned by the slot itself. The one passed as param should be discarded.
bool game_server_init_match_slot(game_server& server, mem_arena& slot_mem, ui8 slot_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	if (slot.state != MATCH_SLOT_STATE::UNINITIALIZED)
	{
		server.platform->logf(LOG_ERROR, "Match slot index %d is already initialized (current state = %d).", slot_index, slot.state);
		return false;
	}

	server.platform->logf("Initializing server match slot index %d.", slot_index);

	slot = {};
	slot.state = MATCH_SLOT_STATE::WAITING;
	slot.slot_memory = slot_mem;

	return true;
}


bool game_server_open_lobby(game_server& server, ui8 slot_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	
	if (slot.state != MATCH_SLOT_STATE::WAITING)
	{
		server.platform->logf(LOG_ERROR, "Attempted to open lobby in slot %d which wasn't properly (re)initialized.", slot_index);
		return false;
	}

	server.platform->logf("Opening lobby in match slot %d.", slot_index);

	slot.state = MATCH_SLOT_STATE::IN_LOBBY;

	// TO IMPLEMENT: Lobby system, tied to the client connection system / player system (abstraction might be convenient for AI / bot players ?)

	return true;
}

bool game_server_start_match_slot(game_server& server, ui8 slot_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	
	if (slot.state != MATCH_SLOT_STATE::IN_LOBBY)
	{
		server.platform->logf(LOG_ERROR, "Attempted to start match in slot %d which wasn't in lobby.", slot_index);
		return false;
	}

	server.platform->logf("Starting match in match slot %d.", slot_index);

	// Create match.

	slot.match.match_ptr = slot.slot_memory.alloc<game_match>();
	slot.match.last_tick_time = server.time_ms;

	game_match& match = *slot.match.match_ptr;
	match_start(slot.slot_memory, server.time_ms, slot.match_params, match);

	slot.state = MATCH_SLOT_STATE::MATCH_ONGOING;

	return true;
}

// Sets a match slot as having ended. During that time the match is no longer ticking but its state is still available
// for score-keeping and reporting.
bool game_server_end_match_slot(game_server& server, ui8 slot_index)
{	
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	
	if (slot.state != MATCH_SLOT_STATE::MATCH_ONGOING)
	{
		server.platform->logf(LOG_ERROR, "Attempted to end match in slot %d which wasn't ongoing.", slot_index);
		return false;
	}

	server.platform->logf("Ending match in match slot %d.", slot_index);

	slot.state = MATCH_SLOT_STATE::MATCH_ENDED;

	// TO IMPLEMENT.

	return true;
}

bool game_server_reset_match_slot(game_server& server, ui8 slot_index)
{
	ASSERT(slot_index < server.init_params.match_slot_count);

	match_slot& slot = server.match_slots[slot_index];
	if (slot.state != MATCH_SLOT_STATE::MATCH_ENDED)
	{
		server.platform->logf(LOG_ERROR, "Attempted to reset match slot %d which wasn't and ended match.", slot_index);
		return false;
	}

	server.platform->logf("Resetting server match slot index %d.", slot_index);

	// Zero out the slot and set it back to waiting. Conserve only its memory.

	mem_arena slot_mem = slot.slot_memory;

	slot = {};
	slot.state = MATCH_SLOT_STATE::WAITING;
	
	// Reset memory.
	slot.slot_memory = slot_mem;
	slot.slot_memory.allocated_count = 0; // TODO(Marc): A "clear" function pointer that can be adapted to allocation strategy will be needed at some point.
	ia_memzero(slot.slot_memory.mem_start, slot.slot_memory.mem_size);

	// From there the slot can be put into lobby mode to start accepting players, or directly have its parameters set and match started.

	return true;
}

game_server* game_server_init(game_server_platform& platform, game_server_init_params& init_params, ui8* memory, ui64 memory_size)
{
	ASSERT_MSG(memory != nullptr && memory_size > GiB(2), "Game server requires at least 2 Gibibytes of memory !");

	// Check init params.
	if (init_params.web_root == nullptr || ia_str_len(init_params.web_root) == 0)
	{
		platform.log(LOG_ERROR, "HTTP Server requires a valid web root folder, relative to the platform resources path. Aborting.");
		return nullptr;
	}
	if (init_params.web_files == nullptr || init_params.web_file_count == 0)
	{
		platform.log(LOG_ERROR, "HTTP Server requires at least one file to serve. Aborting.");
		return nullptr;
	}
	if (init_params.match_slot_count == 0)
	{
		platform.log(LOG_ERROR, "Game Server requires at least one match slot to function. Aborting.");
		return nullptr;
	}

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

	// Initialize HTTP Server.

	newServer->http = http_server_init(*newServer);
	http_server_load_files(*newServer);

	return newServer;
}

void game_server_test_mode_tick(game_server& server)
{	
	// TEST: Run the shared test scenario in its own arena allocated from main memory,
	// dump the resulting match state to specified file and shut down.

	ASSERT(server.platform != nullptr);
	game_server_platform& platform = *server.platform;

	platform.log("Game Server running in test scenario mode.\nRunning test scenario match...");

	game_match_start_params scenario_params = match_test_scenario_get_params();

	mem_arena scenario_memory = mem_arena_create_sub(server.main_memory, match_get_required_mem(scenario_params));
	game_match* scenario_match = match_run_test_scenario(scenario_memory);
	ASSERT(scenario_match != nullptr);

	if (server.init_params.test_scenario_dump_filename == nullptr)
	{
		platform.log("No dump file specified. Going straight to shutdown.");
		platform.shutdown(0);
	}

	platform.logf("Dumping scenario match end state to file \"%s\".", server.init_params.test_scenario_dump_filename);

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
			platform.log(LOG_ERROR, "Failed to write native snapshot file.");
		}
		else
		{
			platform.log(LOG_SUCCESS, "Match ending state dumped successfully.");
		}
	}

	platform.log("Shutting down...");
	platform.shutdown(0);
}

void game_server_tick(game_server& server, time_ms platform_time_ms)
{
	// ... for convenience.
	ASSERT(server.platform != nullptr);
	game_server_platform& platform = *server.platform;

	server.time_ms = platform_time_ms;

	// Run in test mode if configured to do so.
	if (server.init_params.run_test_scenario)
	{
		game_server_test_mode_tick(server);
		return;
	}

	// Serve the web client bundle over HTTP on all platform connections.
	http_server_tick(server);

	// Manage match slots.
	for (ui8 slotIndex = 0; slotIndex < server.init_params.match_slot_count; slotIndex++)
	{
		match_slot& slot = server.match_slots[slotIndex];
		time_ms nextTickTimeMs = 0;
		ui64 msPerTick = 0;

		switch (slot.state)
		{
		case MATCH_SLOT_STATE::MATCH_ONGOING:
			// Ongoing match tick logic.
			
			msPerTick = (1000 / slot.match_params.tick_rate);

			// TODO: Avoid starvation by budgeting ticking time on each slot and ticking each once evenly instead of catching each one up then the next.
			nextTickTimeMs = slot.match.last_tick_time + msPerTick;
			while (nextTickTimeMs < server.time_ms)
			{
				// TODO: Input system based on received network messages.
				match_tick_commands tickCommands = {};
				match_tick(*slot.match.match_ptr, tickCommands);

				// TEST: End match after 2500 ticks.
				if (slot.match.match_ptr->tick == 2500)
				{
					game_server_end_match_slot(server, slotIndex);
					break;
				}

				slot.match.last_tick_time = nextTickTimeMs;
				nextTickTimeMs += msPerTick;
			}

			break;
		default:
			// Do nothing.
			break;
		}
	}

	server.tick_count++;

	if (server.shutdown_triggered)
	{
		platform.shutdown(0);
	}
}

void game_server_stop(game_server& server)
{
	server.platform->log(LOG_WARNING, "Game Server shutting down...");
	// TODO(Marc): Shut down work / checks to be done here (gracefully end connections / matches).
	// ...

	server.platform->log(LOG_SUCCESS, "Game Server shutdown complete."); // Log the fact the server did everything it wanted to do before shutting down.
}
