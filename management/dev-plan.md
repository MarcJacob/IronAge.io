# IRONAGE.IO - DEVELOPMENT PLAN

Development plan and technical architecture for IronAge.io: Game Server, web Game
Client, and the shared GameCommon simulation core. See ../design_doc.md for the game
design itself.

Approach: build a thin, working, end-to-end slice through every component first (an
"architecture skeleton") before building any one component out fully, so there's a real
cross-component testing loop from early on.

## Technical Architecture

### GameCommon (`include/game_common/`)

The full game simulation, shared source compiled into both the Game Server and the
Client. No OS calls, no rendering, no audio, no networking, no system allocator, no
system math library.

- Deterministic lockstep: runs identically on server and every client. Fixed timestep.
  No system libm beyond native instructions (`sqrt` is safe/correctly-rounded;
  `sin`/`cos`/`log`/`pow` etc. are not guaranteed identical across libm builds - use a
  shared vendored implementation, or fixed-point if float drift becomes a problem).
- Memory: requests memory from its host via a callback, up to a host-declared cap, and
  sub-allocates internally. Never calls a system allocator. How the host services
  requests (pre-reserved block vs. on-demand growth) is a host decision.
- Exposes a deterministic state-snapshot function (fixed layout, no padding/pointers).
  Used for: determinism testing (diff native vs. wasm snapshots), late-join state sync,
  debug/test snapshots.
- No rendering/audio - only exposes state; each host renders/plays it however it wants.

### Platform (`src/<platform>/`)

Host-specific glue: process startup, memory (reserving/growing what it gives
GameCommon), logging/file/network/thread callbacks, tick loop, all presentation.

- **Server Platform** (`src/win32/`): one large up-front memory block sized for N
  parallel matches, sub-allocated to GameCommon instances. Owns networking. Presentation
  is a console for now.
- **Client Platform** (web, JS + `wasm32-unknown-unknown` GameCommon): grows wasm memory
  in chunks on demand (adapts to weaker machines). WebSocket to the Game Server for
  input. Renders via two layered canvases: a blit canvas (pixels from GameCommon/
  Platform, for expensive-to-compute stuff like world tiles) and a JS canvas on top
  (sprites/text/UI/input capture).

### Game Server / Client

- **Game Server**: Server Platform + one or more GameCommon match instances + connection
  and match-lifecycle management + networking.
- **Client**: Client Platform + the two-canvas renderer. Shipped to the browser by the
  Game Server over plain HTTP during early development.

### Toolchain

- GameCommon is header-included (unity build) into each host's main file, not built as a
  separate library. CMake builds two host targets: native Game Server exe, and Client
  as `wasm32-unknown-unknown` via Clang (separate build tree + toolchain file).
  Freestanding, no Emscripten.
- Freestanding consequences: hand-write `memcpy`/`memset`/`memmove`; disable exceptions/
  RTTI (`-fno-exceptions -fno-rtti`); no system math lib (see determinism above).
- Emscripten not used: its value (hosted libc/libc++, filesystem emulation, WebGL,
  pthreads, BSD-socket-shaped networking) doesn't apply here - no browser wasm target
  gets real socket access regardless of toolchain, no WebGL planned, no STL-heavy legacy
  code to port. Reconsider only if porting a large hosted-environment library, or moving
  to WebGL/3D.

### Networking

uWebSockets, server-side only. Early dev phase: Game Server also serves the client
bundle over plain HTTP, and relays input between connected clients on a fixed tick
schedule for lockstep. Master server (matchmaking, persistent cross-match scoring)
deferred; direct frontend-to-server connection stays available as a dev/self-host mode
after it exists too.

### Shared conventions

- `include/core/`: common types/macros (`std_types.h`, `assert.h`), via `include/core.h`.
- `include/game_common/`: GameCommon's public interface.
- `include/game_server/`: Server-Platform <-> Game Server interface, match-lifecycle.
- Client-side Platform header location: TBD when that work starts.

## Development Phases

High-level sequencing, not a scope commitment. Mark `[WIP]`/`[DONE]`, add sub-items as
tasks are broken down further.

1. **Architecture Skeleton** - thin end-to-end slice, no real game rules yet. Expected
   result: browser shows a visible element changing in sync, driven by a lockstep-ticked
   GameCommon instance, relayed through the Game Server, rendered via both canvases.
   - [DONE] Remaining initial project setup: CMake targets for the native win32 exe and
     the `wasm32-unknown-unknown` client (both including GameCommon), plus a minimal JS
     harness loading the `.wasm` and calling one trivial exported function.
   - [DONE] GameCommon skeleton: host memory-request interface, init/tick entry points,
     trivial fixed-timestep sim, snapshot function. Verified natively first.
     - [DONE] Public header (`include/game_common/game_match.h`): match create, tick(input),
       dump.
     - [DONE] Internal arena allocator (`include/core/memory.h`), sub-arenas from a parent.
     - [DONE] Trivial sim state (tick counter + one entity moving to a target), fixed
       timestep, no float libm.
     - [DONE] Snapshot: field-by-field dump stream, fixed layout, no pointers/padding.
     - [DONE] Native test in win32 main: init, 200 empty ticks, dump.
   - [DONE] Determinism self-check: same input -> native + wasm snapshots -> automated
     byte-diff.
     - [DONE] Shared scripted scenario in GameCommon (create, N scripted ticks, dump).
     - [DONE] Wasm export running the scenario in a static buffer, exposing snapshot ptr/size.
     - [DONE] Native run writing the snapshot to a file (working directory).
     - [DONE] Test page (`src/client_web/web/determinism_test.html`): runs the wasm scenario;
       file picker loads the native snapshot; byte-diffs, shows PASS/FAIL + first
       differing offset.
   - [WIP] Server Platform, headless: up-front memory block servicing GameCommon's requests,
     tick loop driving one instance, no networking yet.
     - [DONE] Up-front memory block (4 GiB), arenas sub-allocated per match.
     - [DONE] Platform loop measuring delta time (QPC) and passing it to server tick.
     - Server tick: fixed-rate accumulator driving a live match (replace test scenario +
       shutdown).
   - [WIP] Client Platform, standalone: local tick loop with dummy input, two-canvas
     renderer showing GameCommon-driven state - no networking yet.
     - [DONE] Wasm exports: begin match, set input target, tick, render-state readout.
     - JS fixed-rate loop (rAF + accumulator, capped catch-up).
     - JS canvas: draw entity, mouse click sets target.
     - Blit canvas.
   - Networking end-to-end: server serves the client bundle over HTTP and relays input
     via WebSocket on a fixed schedule; client connects and replaces its dummy input
     with the relayed stream.
   - End-to-end proof: two browser tabs against the same server show identical
     GameCommon-driven state changing in sync.

2. **World & settlements simulation** through **11. Ops & hardening** - phases from the
   previous plan version (world/settlements, trade, war, diplomacy, victory & scoring,
   persistence & master server, ops & hardening) still look like roughly the right
   sequence, but haven't been re-specified against the architecture above yet. Revisit
   phase by phase in a future pass.
