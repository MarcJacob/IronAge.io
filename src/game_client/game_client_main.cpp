// Main implementation file for the web client backend. Pure C++ logic - no WASM-specific concerns; see
// src/platform/wasm_client_main.cpp for the WASM export wrappers that drive this from JS.

#include "core.h"
#include "core/memory.h"
#include "game_client/game_client_backend.h"

#include "game_client.h"

// Unity-compile the Game Common code into the web client.
#include "../game_common/game_common_main.cpp"

// Unity-compile the rest of the backend.
#include "game_client_net.cpp"
#include "game_client_render.cpp"
#include "game_client_input.cpp"
#include "game_client_query.cpp"
#include "game_common/match/commands.h"
#include "game_common/match/match.h"
#include "game_common/match/start_params.h"
#include "game_common/match/world/entities/world_entity_settlements.h"
#include "game_common/match/world/world.h"

game_client* game_client_init(mem_arena& backend_memory)
{
	ui64 memoryAvailable = backend_memory.mem_size - backend_memory.allocated_count;
	ASSERT_MSG(backend_memory.mem_start != nullptr && memoryAvailable > sizeof(game_client),
		"Client backend requires more than %llu bytes, got %llu.", sizeof(game_client), memoryAvailable);

	// Initialize backend, assign it its memory.
	game_client* backend = backend_memory.alloc<game_client>();
	*backend = {};

	backend->memory = &backend_memory;

	// Sub-allocate the match / render / input queue arenas once. Match (re)starts, render rebuilds, and input queue
	// drains all reuse this same backing memory via clear(), rather than sub-allocating again each time.
	static constexpr ui64 LOCAL_MATCH_MEM_SIZE = MiB(64);
	static constexpr ui64 RENDER_MEM_SIZE = KiB(64);
	static constexpr ui64 COMMAND_QUEUE_MEM_SIZE = KiB(1);
	static constexpr ui64 RENDER_TILES_MEM_SIZE = MiB(32); // 4 bytes per world tile.
	static constexpr ui64 RENDER_FRAME_MEM_SIZE = MiB(16);
	static constexpr ui64 PATH_SCRATCH_MEM_SIZE = KiB(256); // Limits how far a path query can search.

	backend->local_match_mem = mem_arena_create_sub(*backend->memory, LOCAL_MATCH_MEM_SIZE);
	backend->path_scratch = mem_arena_create_sub(*backend->memory, PATH_SCRATCH_MEM_SIZE);
	backend->render_tiles_mem = mem_arena_create_sub(*backend->memory, RENDER_TILES_MEM_SIZE);
	backend->render_state.frame.memory = mem_arena_create_sub(*backend->memory, RENDER_FRAME_MEM_SIZE);

	backend->input.command_queue = mem_arena_create_sub(*backend->memory, COMMAND_QUEUE_MEM_SIZE);
	backend->input.command_queue_builder.target_mem = &backend->input.command_queue;
	ASSERT(backend->input.command_queue_builder.init());
	return backend;
}

void game_client_tick(game_client& backend, float delta_time)
{
	if (backend.local_match != nullptr)
	{
        backend.match_time_ms += (ui32)(delta_time / 1000.f);

        // If in SP match, the client ticks it locally without relying on the server.
        if (backend.state.current == CLIENT_STATE::IN_MATCH_SP)
        {
            ui32 matchTickPeriodMS = 1000 / backend.local_match->start_params->tick_rate;
            if (backend.match_last_tick_time_ms + matchTickPeriodMS > backend.match_time_ms)
            {
                static_mem_arena<1024> localCommandsMem {};
                match_tick_commands_builder tickCommandsBuilder { &localCommandsMem };
                tickCommandsBuilder.init();

                match_command_sequence& queuedSequence = *backend.input.command_queue_builder._sequence_start;
                if (queuedSequence.command_count > 0)
                {
                    tickCommandsBuilder.push_validated_sequence(0, *backend.local_match, queuedSequence);
                }

                match_tick(*backend.local_match, *tickCommandsBuilder._tick_commands_start);

                backend.match_last_tick_time_ms = backend.match_time_ms;
            }
        }
        
		game_client_apply_viewport_input(backend, delta_time);
		game_client_rebuild_render_state(backend.render_state, *backend.local_match);
	}
}

// Starts the local match with the given start parameters. params must already live in backend.local_match_mem
// (the caller copies it there before calling this) - this does NOT clear that arena or copy params again, since
// that would destroy params before match_start can read it.
bool game_client_begin_match_with_params(game_client& backend, game_match_start_params& params)
{
	game_match* localMatch = backend.local_match_mem.alloc<game_match>();
	if (!match_start(backend.local_match_mem, 0, params, *localMatch))
	{
		backend.local_match = nullptr;
		return false;
	}

	backend.local_match = localMatch;
	backend.render_state.world_size = params.world_size_regions * world_terrain::REGION_SIZE;

    ui64 tileCount = backend.render_state.world_size.x * backend.render_state.world_size.y;

    // Initialize render state memory blocks & bitmaps, reusing the same backing memory every match.
    backend.render_tiles_mem.clear();

    backend.render_state.tiles.terrain_tiles = backend.render_tiles_mem.alloc<TERRAIN_TILE_TYPE>(tileCount);
    ASSERT(backend.render_state.tiles.terrain_tiles != nullptr);

    backend.render_state.tiles.owner_settlement_tiles_bitmap = backend.render_tiles_mem.alloc<ui16>(tileCount);
    ASSERT(backend.render_state.tiles.owner_settlement_tiles_bitmap != nullptr);

    backend.render_state.tiles.tile_flags_bitmap = backend.render_tiles_mem.alloc<ui8>(tileCount);
    ASSERT(backend.render_state.tiles.tile_flags_bitmap != nullptr);

    // Copy match terrain (stored region by region) into the row-major terrain bitmap.
    {
        const world_terrain& terrain = *localMatch->world->terrain;
        const ui16 width = backend.render_state.world_size.x;
        const ui16 height = backend.render_state.world_size.y;
        for (ui16 y = 0; y < height; y++)
        {
            for (ui16 x = 0; x < width; x++)
            {
                ui32 regionIndex = (y / world_terrain::REGION_SIZE) * terrain.size_regions.x + (x / world_terrain::REGION_SIZE);
                ui32 tileIndex = (y % world_terrain::REGION_SIZE) * world_terrain::REGION_SIZE + (x % world_terrain::REGION_SIZE);
                backend.render_state.tiles.terrain_tiles[(ui64)y * width + x] = terrain.regions[regionIndex].terrain_types[tileIndex];
            }
        }
    }

    // Init owner and tile flags buffers to 0.
    ia_memzero(backend.render_state.tiles.owner_settlement_tiles_bitmap, tileCount * sizeof(ui16));
    ia_memzero(backend.render_state.tiles.tile_flags_bitmap, tileCount);

    // Reset time measurements.
    backend.match_time_ms = 0;
    backend.match_last_tick_time_ms = 0;

    // Per-frame memory is created once in game_client_init and cleared by the render rebuild.
	return true;
}

void game_client_begin_singleplayer(game_client& backend)
{
    ASSERT(backend.state.current == CLIENT_STATE::LOBBY_LIST); // @Temp(Marc): Can only begin SP play from lobby list,
                                                               // where the user has a name and is "ready" to start playing.

    // @TEMP(Marc): Always control player ID 0, with whatever is in own_name. The other names are left to whatever they were
    // last set to, if anything.
    backend.state.match_player_name_count = 1;
    backend.state.match_player_names[0] = backend.state.own_name;
    backend.controlled_player_id = 0;

    // Copy test params into the match's own memory.
    backend.local_match_mem.clear();

    game_match_start_params* params = match_test_scenario_get_params(backend.local_match_mem);
    ASSERT(game_client_begin_match_with_params(backend, *params));

    // Spawn all players at random locations.
    static_mem_arena<1024> scratch;
    match_tick_commands_builder spawnCommandsBuilder { &scratch };
    spawnCommandsBuilder.init();

    spawnCommandsBuilder.push_new_sequence(INVALID_MATCH_PLAYER_ID); // "Server" commands are not associated to a specific player id.

    // @TODO(Marc): Refactor a lot of the standard "match hosting" code and commands to common code so they can be shared
    // by client & server, including the support of AI players.

    // Pick a random location for all players to begin with, retrying a few times to find a valid one (else the last one is accepted anyway).
    game_match& match = *backend.local_match;
    rand_generator_32 randGen = { match.start_params->random_seed };
    for (match_player_id playerID = 0; playerID < params->player_count; playerID++)
    {
        world_location randLoc;
        for (ui8 tryIndex = 0; tryIndex < 32; tryIndex++)
        {
            randLoc = randGen.next_vec2({10, 10}, match.start_params->world_size_regions * world_terrain::REGION_SIZE - world_location {10, 10});
            if (world_is_valid_settlement_location(*match.world, randLoc, playerID)) break;
        }

        
        auto& spawnPlayer = *spawnCommandsBuilder.push_command<command_data_server_spawn_player>(
                MATCH_COMMAND_TYPE::SERVER_SPAWN_PLAYER);

        spawnPlayer.spawned_player = playerID;
        spawnPlayer.spawn_location = randLoc;
    }

    // Perform first tick.
    match_tick(match, *spawnCommandsBuilder._tick_commands_start);

    // Refresh render state so it can take match params into account.
    game_client_rebuild_render_state(backend.render_state, *backend.local_match);

    // @TODO(Marc): Move lobby code out of server-only so the client can have its own lobby.
    // For now we just set the IN_MATCH state immediately.
    backend.state.current = CLIENT_STATE::IN_MATCH_SP;
}

void game_client_end_local_match(game_client& backend)
{
	backend.local_match = nullptr;
	backend.local_match_mem.clear();

	// Render state: forget the tile arrays (their memory is reused by the next match) and the frame's entities.
	backend.render_tiles_mem.clear();
	backend.render_state.tiles = {};
	backend.render_state.frame.memory.clear();
	backend.render_state.frame.entity_count = 0;
	backend.render_state.frame.entity_states = nullptr;

	// Stale output / queued input.
	backend.net_output_msg_size = 0;
	backend.input.command_queue.clear();
	ASSERT(backend.input.command_queue_builder.init());
}
