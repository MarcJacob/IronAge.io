# Build system revamp

Input date: 2026-10-03

## Item

Replace CMake with a simpler build, and restructure the build outputs:
- Game Common builds as a static library on its own, linked into both the
  client wasm and the game server.
- The game server becomes a dynamic library, hot-reloadable by the platform
  layer while it keeps the same state (same memory layout).
- Fallback when the layout changes: a "re-init" routine in the server that
  cuts off all existing connections, matches, etc.

## Description

- CMake is too heavy for the current unity-compiling of each component.
- Building Game Common separately guarantees identical compilation for the
  wasm client and the server before it is linked in.
- Hot reload: the win32 platform keeps the memory block and state, reloads the
  server DLL, and passes it the same state. Needs a stable platform / server
  interface and a stable state layout across reloads.
- Open design: the DLL <-> platform interface, what is considered persistent
  state, and what re-init resets.
- Reverses the earlier decision to defer the CMake -> .bat move.

## Why

Simplifies the build, makes Game Common compilation behaviour identical
across targets, and is the base for fast iteration
(`dev_iteration_loop_2026-10-03.md`). Multi-file architecture change; sits
outside the current phase sequencing.
