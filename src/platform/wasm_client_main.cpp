// Entry point for the Web WASM client compilation and execution.
// This platform layer is special in the sense that it mostly exports functionality to be started from the host web page.

// Unity-compile with game client main.
#include "../client_web/game_client_main.cpp"

// Only functions marked WASM_EXPORT are exported (build uses -fvisibility=hidden).
#define WASM_EXPORT extern "C" __attribute__((visibility("default")))

// Test functions to be called from JS ? I'm still very new to how WebAssembly works.
WASM_EXPORT int gameclient_add(int a, int b)
{
	return a + b * 2;
}

WASM_EXPORT const char* gameclient_getHello()
{
	return "Hello, world !\n";
}
