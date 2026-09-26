# IRONAGE.IO - DEVELOPMENT PLAN

Development plan and technical architecture for IronAge.io: Game Server, web Game
Client, and the shared GameCommon simulation core. See ../design_doc.md for the game
design itself.

Approach: build a thin, working, end-to-end slice through every component first (an
"architecture skeleton") before building any one component out fully, so there's a real
cross-component testing loop from early on.

## Current Status

Phase 1 (Architecture Skeleton), networking end-to-end: everything up to and including the
WebSocket layer is done and tested. What remains is the game protocol on top of it.

- Working: web client bundle served over HTTP; `/ws` upgrades to WebSocket (frame codec, ping
  keepalive, `Origin` check); upgraded connections are `GAME_CLIENT`s exchanging binary
  `game_message_header` messages; browser-side tests (`src/websocket_tests.js`) all pass.
- Temporary: `game_server_test_echo_game_client` (`game_server_main.cpp`) echoes game
  messages, and the web client page still runs a local match while only opening a websocket.
  Both get replaced by the wire protocol.
- Next: wire protocol v0 (phase 1 items below).
- Testing loop: build the server and the web client (the wasm build deploys the bundle to
  `game_server_resources/web_root`), restart the server (files are preloaded at startup), open
  `http://localhost:8000/`, click "Run WebSocket tests".
- Where things live: `src/game_server/` (server; clients in `game_server_clients.*`, web server
  in `web_server/`), `src/platform/win32_game_server/` (platform, network threads),
  `src/client_web/` (web client), `include/game_common/` (shared simulation and messages).
  `*.imprint.md` files document each folder.

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

- **Server Platform** (`src/platform/win32_game_server/`): one large up-front memory block
  sized for N parallel matches, sub-allocated to GameCommon instances. Owns networking.
  Presentation is a console for now.
- **Client Platform** (web, JS + `wasm32-unknown-unknown` GameCommon): grows wasm memory
  in chunks on demand (adapts to weaker machines). WebSocket to the Game Server for
  input. Currently renders through a JS canvas (sprites/text/UI/input capture); a
  blit canvas for expensive-to-compute pixel content is deferred.

### Game Server / Client

- **Game Server**: Server Platform + one or more GameCommon match instances + connection
  and match-lifecycle management + networking.
- **Clients** (`game_server_clients.*`): every inbound connection is a `game_server_client`.
  It starts UNKNOWN, is claimed by a sub-component (the web server) as NON_GAME_CLIENT, and
  becomes GAME_CLIENT once upgraded. Game code talks to GAME_CLIENTs through send / peek /
  consume message functions provided by the sub-component.
- **Web server** (`src/game_server/web_server/`): preloaded static files over HTTP, http ->
  websocket upgrade, websocket framing.
- **Client**: Client Platform + JS canvas renderer. Shipped to the browser by the
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

Hand-rolled, no third-party libraries. The Server Platform only provides non-blocking
byte-stream connections (Winsock on win32) through a pull-based interface. HTTP and
WebSocket handling live in Game Server code over that interface (portable, testable
natively). TLS (wss) via reverse proxy at deployment time. Early dev phase: Game Server also
serves the client bundle over plain HTTP, and relays input between connected clients on a
fixed tick schedule for lockstep. Master server (matchmaking, persistent cross-match
scoring) deferred; direct frontend-to-server connection stays available as a dev/self-host
mode after it exists too.

### Shared conventions

- `include/core/`: common types/macros (`std_types.h`, `assert.h`), via `include/core.h`.
- `include/game_common/`: GameCommon's public interface, and the network message structures
  shared with the client (`game_messages.h`).
- `include/game_server/`: Server-Platform <-> Game Server interface, match-lifecycle.
- Client-side Platform header location: TBD when that work starts.

## Development Phases

High-level sequencing, not a scope commitment. Mark `[WIP]`/`[DONE]`/`[NEXT]`; keep finished
work to a line or two.

1. **Architecture Skeleton** - thin end-to-end slice, no real game rules yet. Expected
   result: browser shows a visible element changing in sync, driven by a lockstep-ticked
   GameCommon instance, relayed through the Game Server, rendered via the current JS
   canvas (with the blit canvas deferred).
   - [DONE] Project setup: CMake targets for the native win32 server and the wasm32 client
     (both including GameCommon), JS harness loading the `.wasm`.
   - [DONE] GameCommon skeleton: match create / tick(input) / dump, arena allocator, trivial
     sim (one entity moving to a target), deterministic snapshot. Native and wasm snapshots
     are byte-diffed by `determinism_test.html`.
   - [DONE] Server platform + headless server: 4 GiB up-front block, match slots with a
     lifecycle and a memory arena each, ongoing matches ticked on a fixed schedule from the
     platform's ms timestamp.
   - [DONE] Client platform, standalone: wasm exports, JS fixed-rate loop, JS canvas with
     click-to-set-target.
   - [DONE] Win32 networking: listen / reception / send threads over ring buffers and a
     connection table behind a pull-based platform interface (all-or-nothing sends, clean
     shutdown). Platform file access under `GAME_SERVER_RESOURCES_DIR`.
   - [DONE] Logging: platform `log` / `logf` with `LOG_TYPE`, colored win32 end points,
     `server.log` / `server.logf` with component prefixes.
   - [DONE] Web server: preloaded static files over HTTP (structured request parse, GET /
     HEAD, error responses, timeouts), sized string helpers (`include/core/string.h`).
   - [DONE] Client ownership on the game server: clients table (UNKNOWN -> NON_GAME_CLIENT ->
     GAME_CLIENT), disconnect handlers, http -> websocket upgrade handing over leftover bytes
     without a copy, send / peek / consume message functions.
   - [DONE] WebSocket: SHA-1 + base64 (`include/core/math.h`), handshake with `Origin` check,
     frame codec, ping keepalive, framed sending. Browser tests (`src/websocket_tests.js`):
     all passing.
   - [NEXT] Game protocol v0 over the WebSocket layer, replacing the temporary echo hook.
     Design pass pending (messages, match attachment, tick relay, late join).
     - Goal: two browser tabs connected to the same server show synchronized, observable
       state, each tab controlling its own separate entity. This is the phase's end-to-end
       proof.

2. **World & settlements simulation** through **11. Ops & hardening** - phases from the
   previous plan version (world/settlements, trade, war, diplomacy, victory & scoring,
   persistence & master server, ops & hardening) still look like roughly the right
   sequence, but haven't been re-specified against the architecture above yet. Revisit
   phase by phase in a future pass.

## Backlog

Tasks not currently part of the plan that need to be added to it at some point.

- Client render interpolation between ticks (smooth movement).
- Blit canvas for expensive-to-compute pixel content; pure-JS rendering is sufficient for now.
- Logging takes a string view only (no variable arguments) on the server and server platform;
  formatting happens in platform-independent code through an in-house string format
  implementation (numbers, string views).
- Persistent client identity / reconnection: a client outlives its connection (OFFLINE and
  CONNECTION_LOST states, reconnection grace time).
- Win32 net: timeout to close a connection whose peer stays alive but stops reading (it is
  never closed once in SERVER_CLOSED).
- Platform call to list the files in a folder relative to resources, so the server discovers
  the files under `web_root` instead of taking a list in the init params.
- Web server: optional automatic reload of a preloaded file when it changed on disk since
  it was loaded.
- Web server: keep frequently-used files always loaded ("cached"), load rarely-requested or
  large ones on demand.
- WASM client backend: log messages (typed, like the server's `LOG_TYPE`) straight to the JS
  frontend.
