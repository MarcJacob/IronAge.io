// Testing mode function exports.

#include "game_client_web.h"

// DETERMINISM TEST: runs the shared test scenario and exposes the resulting snapshot so JS can compare it with the native one.
static ui8 determinism_match_memory[MiB(1)];
static ui8 determinism_snapshot[KiB(4)];

struct determinism_dump_state
{
	ui8* dump_mem;
	ui64 dump_size;
};

// Runs the test scenario, storing the snapshot in an internal buffer. Returns the snapshot size in bytes, or 0 on failure.
WASM_EXPORT ui32 gameclient_run_determinism_scenario()
{
	mem_arena arena = mem_arena_create(determinism_match_memory, sizeof(determinism_match_memory));
	game_match* match = match_run_test_scenario(arena);
	if (match == nullptr)
	{
		return 0;
	}

	match_dump_stream dump_stream = {};
	ui64 dump_size = match_dump_gamestate(*match, dump_stream);
	if (dump_size == 0 || dump_size > sizeof(determinism_snapshot))
	{
		return 0;
	}

	determinism_dump_state dump_state = { determinism_snapshot, 0 };
	dump_stream.dump_func = [](void* state, ui8* bytes, ui64 byte_count)
		{
			determinism_dump_state& dump_state = *(determinism_dump_state*)state;
			ia_memcpy(dump_state.dump_mem + dump_state.dump_size, bytes, byte_count);
			dump_state.dump_size += byte_count;
		};
	dump_stream.state = &dump_state;
	match_dump_gamestate(*match, dump_stream);

	return (ui32)dump_size;
}

// Address of the snapshot buffer in wasm memory, valid after gameclient_run_determinism_scenario.
WASM_EXPORT ui8* gameclient_get_determinism_snapshot()
{
	return determinism_snapshot;
}

