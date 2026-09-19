// Entry point for the Web WASM client compilation and execution.
// Mostly just includes web client backend code. This file exists as the compilation target for consistency.

// Unity-compile with game client main.
#include "../client_web/game_client_main.cpp"

// Platform implementation of the core assertion functions: no way to report a message here, so just trap.
void ASSERT_EXIT_FUNC() { __builtin_trap(); }
void ASSERT_MSG_FUNC(const char* assertMsg, const char* filename, ui32 line, ...) { __builtin_trap(); }
