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
  Decided: imprint in `game_common` documenting the required flags (no
  helper script).
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

- [DONE] Increment 1 (script-based build of today's two executables):
  `scripts/` folder, `dev_env_setup.cmd`, `win32_build_all.cmd`,
  `win32_launch_server.cmd`, `web_game_client_build.cmd`. CMake and
  `deploy_web_client.bat` removed. Full build + iteration works for both
  server and web client.
  - Side change: game server resources folder path is now a launch
    parameter (default: `game_server_resources/` under working directory).
  - `full_ship.cmd` (deploy step) not started.

- [DONE] Game server DLL: build script, platform loads it and runs
  (no hot-reload yet). GameCommon compiled in the DLL; imprint flags not yet
  applied (see Next step 0).
  - Assertions: `assertion_handler` struct of function pointers in
    `assert.h` + global pointer; macros call through it (null-check fallback
    to trap). Each module fills it in; server does so in `OnLoad` (called by
    the platform on every load). Handler functions live in platform code.

## Next step

0. Fix review findings (DLL split review done):
   - DLL build script lacks imprint flags (`-ffp-contract=off`,
     `-ffreestanding`, `-fno-builtin`, `-fsigned-char`); web script lacks
     `-ffp-contract=off`, `-fno-builtin`, `-fsigned-char`.
   - `assert.h`: no null-handler check (despite decision above); pointer
     declared `extern` then `static` in modules; `game_server_platform.h`
     `on_unload` sets `_is_loaded = true`.
   - [DONE] Build script bugs (DLL failure propagation, `OUTPUT_FOLDER` arg
     now a real output folder, web script filename/robocopy, launch/env
     script typos, `scripts.imprint.md` list). Awaiting hand check.
   - Remaining script issues: missing closing quote in
     `win32_game_server_build.cmd:36` (`COMPILER_FLAGS ... --debug`);
     unquoted `if not exist %OUTPUT_DIR%` breaks on paths with spaces.
   - Stale outputs to delete: `web/game_client.wasm`,
     `build/web_client_debug/`.
   - Pointers into DLL stored in the block (dangle on reload): client
     send/peek/consume funcs (`game_server_clients.h`), disconnect handlers
     table, `http_file.content_type` view of DLL `.rdata`.
   - Naming of `_ASSERTION_HANDLER` struct/members vs snake_case convention.
1. Platform-side hot-reload: detect new DLL (recommended: poll timestamp),
   copy before load, unload/reload, rebind the three function pointers (and
   `OnLoad`), keep passing the same memory block. Re-init fallback on layout
   change left out of this step (not yet decided).
   Open: DLL copy/PDB naming scheme (build to fixed name, platform copies to
   numbered name, unique PDB); fix for dangling pointers (re-register in
   `OnLoad` vs indices/enums); layout-change detection.
2. Audit web client build script flags against the `game_common` imprint.
