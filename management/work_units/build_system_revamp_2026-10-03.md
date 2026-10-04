# Build system revamp

Input date: 2026-10-03

## Item

Replace CMake with a simpler build and restructure outputs:
- Game Common builds as a static library, linked into the client wasm and the
  game server (identical compilation for both).
- Game server becomes a dynamic library, hot-reloaded by the win32 platform
  while it keeps state (same memory block and layout).
- Fallback on layout change: a server "re-init" routine that cuts all
  connections, matches, etc.

## Decisions

- Full script-based build: drops CMake entirely, `.cmd` script per component
  calling its compiler directly. Reverses the earlier "defer CMake -> .bat
  move" note - simplicity won out over CMake's VS-integration/portability.
  Multi-file architecture change; outside the current phase sequencing.
  Follow-on, outside this unit's scope: move primary editing to VIM, keep VS
  (or another tool) only as a debugger.
- GameCommon compiles once per target architecture (win32, wasm32 - not
  literally one object reused on both, different ABIs), as a static library.
- Script layout: no build scripts in `game_common`, `game_server` or
  `game_client` - those folders stay pure (no build opinion of their own).
  Each platform/app pair script (`win32_game_server`, `web_game_client`)
  compiles its own copy of GameCommon's sources with its own flags, and
  unity-includes its app-pair's main .cpp directly (e.g.
  `win32_game_server_main.cpp` -> `game_server_main.cpp`). "Identical
  compilation across targets" is no longer structurally shared - it's on
  whoever writes a new platform/app pair to copy GameCommon's flags
  correctly. Mitigation, not yet decided which: an imprint in `game_common`
  documenting the required flags, a top-level build README, and/or a helper
  script in `game_common` that returns default flags for a given
  compiler/linker (returns config only, builds nothing - stays pure).
- Root: one driver script builds every platform/app pair + deploy + (later)
  live-reload notify. A `build_env.bat` locates the repo root (via `.git`)
  and exports shared path/output-root env vars; per-call specifics (toolchain
  variant, extra defines) are passed as script arguments, not env vars.
- Server holds no static/global state today (confirmed) - everything hangs
  off the one `VirtualAlloc`'d block via `mem_arena`. Platform-side statics
  (`WIN32_PLATFORM`, logging locks, `win32_net_component`) stay as they are;
  reload only ever touches the DLL and that one block.
- `win32_net_component` stays platform-owned, outside the block; the server
  only ever sees it through the existing `game_server_platform` function
  pointer table. Not affected by reload.
- The 3 platform->server lifecycle calls (`game_server_init`,
  `game_server_tick`, `game_server_stop`, today ordinary linked calls) become
  function pointers the platform resolves at runtime (`GetProcAddress`) after
  each load/reload of the server DLL, instead of being linked at build time.
  The reverse direction (server->platform) is already a function-pointer
  table (`game_server_platform`) and needs no change.

## Survey findings (read-only pass, 2026-10-04)

- Today: 2 CMake targets only (`IronAgeIO_WebClient`, `IronAgeIO_GameServer`),
  each one unity `#include` chain down to a single source file. No library
  target exists; GameCommon is `#include`d into both and compiled twice.
- No platform `#ifdef`s inside GameCommon - it doesn't special-case win32 vs
  wasm today, which is favorable for the split.

## Progress

- [WIP] Increment 1 (script-based build of today's two executables):
  `scripts/` folder created, `dev_env_setup.cmd` in place (repo-root
  detection, path variables). Plan laid out in `scripts/scripts.imprint.md`:
  `*_build.cmd` per project, `deploy_web_client.cmd`, `full_rebuild.cmd`
  (root driver, also triggers deploy), `full_ship.cmd` (release + archive,
  not started).

## Next step

1. [WIP] Replace CMake with `.cmd` scripts producing today's same two
   executables (no DLL/static-lib split yet) - proves the script build works.
2. Split GameCommon compilation into each platform/app pair script per the
   layout decided above.
3. Turn the game server into a DLL exporting `game_server_init` / `_tick` /
   `_stop`; platform loads it via `LoadLibrary` + `GetProcAddress`.
4. Platform-side hot-reload: detect new DLL, unload/reload, rebind the three
   function pointers, keep passing the same memory block.
