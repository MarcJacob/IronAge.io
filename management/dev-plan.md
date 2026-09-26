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
  input. Currently renders through a JS canvas (sprites/text/UI/input capture); a
  blit canvas for expensive-to-compute pixel content is deferred.

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

Hand-rolled, no third-party libraries. The Server Platform only provides non-blocking
byte-stream connections (Winsock on win32) through a pull-based interface. HTTP and
WebSocket handling live in Game Server code over that interface (portable, testable
natively). TLS (wss) via reverse proxy at deployment time. Early dev phase: Game Server also serves the client
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
   GameCommon instance, relayed through the Game Server, rendered via the current JS
   canvas (with the blit canvas deferred).
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
   - [DONE] Server Platform, headless: up-front memory block servicing GameCommon's requests,
     tick loop driving one instance, no networking yet.
     - [DONE] Up-front memory block (4 GiB), arenas sub-allocated per match.
     - [DONE] Match slots: fixed-size slots with lifecycle state, arena per slot.
     - [DONE] Start a match in a slot (create, start, state -> MATCH_ONGOING).
     - [DONE] Platform passes an integer monotonic timestamp (ms) to server tick.
     - [DONE] Server tick: per ongoing slot, tick on a fixed schedule, stubbed input.
   - [DONE] Client Platform, standalone: local tick loop with dummy input, pure-JS
     renderer showing GameCommon-driven state - no networking yet. The blit canvas is
     deferred until it is useful.
     - [DONE] Wasm exports: begin match, set input target, tick, render-state readout.
     - [DONE] JS fixed-rate loop (rAF + accumulator, capped catch-up).
     - [DONE] JS canvas: draw entity, mouse click sets target.
   - Networking end-to-end: server serves the client bundle over HTTP and relays input
     via WebSocket on a fixed schedule; client connects and replaces its dummy input
     with the relayed stream.
     - [DONE] Platform net interface (`game_server_platform.h`): pull-based byte streams
       (new / closed connection queries, send, receive, close).
     - [WIP] Win32 implementation: network I/O off the tick thread; the net_ functions
       only touch buffers.
       - [DONE] SPSC ring buffer (item count, Interlocked).
       - [DONE] Listen thread: bind / listen / accept -> new-connections ring.
       - [DONE] Connection table (fixed size), handle = table index (reused). State field is
         the ownership handoff; no connection event rings, no CAS. Per-connection recv /
         send byte rings.
         - States EMPTY / CONNECTED / OPEN / PEER_CLOSED / SERVER_CLOSED / CLOSED /
           ENDED; listen thread EMPTY -> CONNECTED; game server acknowledgement
           CONNECTED -> OPEN (unconditional) and CLOSED -> ENDED; main thread ENDED ->
           EMPTY cleanup.
         - CONNECTED connections are not polled: the OS buffers incoming data until
           acknowledged.
         - Full lifecycle verified with test code + repeated browser connections.
       - [DONE] Reception thread: WSAPoll (5 ms) over OPEN connections with free ring space,
         recv -> ring. Exclusive owner of closing sockets and of OPEN -> PEER_CLOSED,
         PEER_CLOSED -> CLOSED (once ring drained), SERVER_CLOSED -> CLOSED.
         Listen thread stays separate from it (decided).
       - [DONE] `net_` query new / closed connections, `net_receive_bytes`.
       - [DONE] `net_close_connection`: writes SERVER_CLOSED unconditionally, reception thread
         closes the socket -> CLOSED once the send thread has flushed the send ring (send
         thread keeps sending in SERVER_CLOSED; a hard send error drops the data). Assumes
         the game server only passes live handles. Peer that stays alive but stops reading
         is never closed: needs a timeout.
       - [DONE] Send thread: WSAPoll (POLLWRNORM) over OPEN connections with buffered data,
         peek -> send -> discard what went out. Never closes sockets; `send_busy` handshake
         with the reception thread (state leaves OPEN, then close waits for `send_busy`
         to clear). Connection sockets are non-blocking. `net_send_bytes` is
         all-or-nothing into the send ring. Verified with a hardcoded HTTP response to a
         browser.
       - [DONE] Clean shutdown: listen socket created in start and closed in stop to unblock
         accept; reception thread closes all open sockets on exit; all threads joined, then
         WSACleanup.
       - [DONE] Ring buffer: fixed straight-read / straight-write ignoring the cursors; added
         `peek` / `discard`.
     - [DONE] Platform `read_file` (+ file size) in "server resources storage" (win32:
       `GAME_SERVER_RESOURCES_DIR`, set by CMake, default `<repo>/game_server_resources`).
     - [DONE] Server: HTTP connection table (fixed max) + per-connection request buffer,
       chunked response sending (now `web_server/web_server_http.cpp`).
     - [DONE] Server: static HTTP serving of the client bundle (GET only, keep-alive, MIME
       types). `deploy_web_client.bat` copies the bundle into `game_server_resources/web_root`.
       - [DONE] First version: any file under `web_root`, read on request. Verified in a
         browser.
       - [DONE] Redesign: files listed in `init_params.web_files` are preloaded into named
         buffers at init (fatal if any fails, total size budget); requests only match
         those names, `/` -> `index.html`, else 404. No file system access at request
         time. Tested in a browser.
     - [DONE] Improvement to the logging system on win32 platform and game server.
       - Single platform `log` / `logf` taking a `LOG_TYPE` (`include/core/std_types.h`).
       - Win32: `win32_stdout` / `win32_stderr` end points (color by type), component-
         prefixed `WIN32 (<component>)` logging, net component logs as `NET`.
       - Game server: `server.log` / `server.logf`, `GAME SERVER (<component>)` prefix.
     - [WIP] Server: connection ownership moves from the web server to the game server.
       - [DONE] Clients table on the game server: fixed size, one entry per platform
         connection, discriminated union (client type + type-specific data). Register / lost /
         lookup by client handle, per-type disconnect handlers, wired in `game_server_tick`
         (closed connections first, then new).
       - [DONE] New connections start UNKNOWN. On first bytes, detect HTTP; anything else
         is rejected (closed) for now.
         - [DONE] Per-tick function reads a small local buffer per unknown client and asks
           each subsystem whether it understands the bytes; the web server claims the client
           (sets type, allocates its web client state, takes the bytes).
         - [DONE] Unknown clients still unrecognized after 1 s are dropped (`connected_at_ms`).
         - [DONE] Web server disconnect handler releases the web client state.
       - [DONE] HTTP client type: HTTP connection state is the HTTP variant of the web client
         union; web server works on a client entry instead of owning connections. Verified in
         a browser.
       - [DONE] Client type changes on upgrade: HTTP -> WEBSOCKET (game client).
         - [DONE] Web client goes IN_UPGRADE_WEBSOCKET -> ACTIVE_WEBSOCKET once the handshake
           response is sent (checked before any further request parsing).
         - [DONE] Bytes received after the request head carry over to the websocket client (http
           and websocket reception buffers share the same offset, no copy).
         - [DONE] Promote the game server client to GAME_CLIENT, with websocket-framed send /
           peek / consume functions.
         - [DONE] Game client send / peek / consume dispatched through the client's
           `game_client` function pointers (a peeked message stays valid until consumed).
         - [DONE] Disconnect handlers registered as a list, each checking ownership through
           `connection_context`.
       - Leaves room for other types later (master server, administration, non-browser
         clients).
     - [DONE] Server: web server component (formerly "http server") split into
       `src/game_server/web_server/`: `web_server.h` / `.cpp` (common behavior),
       `web_server_http.cpp` (http, including the http -> websocket upgrade),
       `web_server_websocket.cpp` (websocket frame code, to come). Functions prefixed
       `web_server_`, `web_server_http_`.
     - [DONE] Server: HTTP request handling refactor.
       - [DONE] Request line parse (method, target without query, version 1.1), complete-head
         detection, one request consumed at a time, GET / HEAD file serving, 501 / 405 / 400 /
         505 / 431 responses sent before closing, idle timeout.
       - [DONE] Sized string helpers (`include/core/string.h`: view, static string) used by the
         request parse and the response head.
       - [DONE] Header parse: each header field parsed individually, pointers into the request
         buffer; request disposed after it has been handled. Case-insensitive lookup by name.
       - [DONE] Routing into branches: static file GET, WebSocket upgrade on `/ws`.
     - [WIP] Server: WebSocket handshake (SHA-1 + base64), frame codec (masked client frames),
       ping / pong / close, partial frames.
       - [DONE] SHA-1 + base64 encode, in `include/core/math.h` (`ia_sha1`,
         `ia_base64_encode`). Done first, out of order. Verified against test vectors.
       - [DONE] Upgrade branch: validate headers, 101 response (tested client-side), web client
         moves to ACTIVE_WEBSOCKET without being dropped once the response is sent.
       - Validate `Origin` on the upgrade.
       - [WIP] Frame codec (masked client frames, ping / pong / close, partial frames) in
         `web_server_websocket.cpp`, plus framed sending through the client sending buffer.
         Binary echo verified in a browser.
         - Remaining test cases: message order, burst, invalid frames (close codes 1002 /
           1003 / 1007 / 1009), client close, idle drop.
         - Server ping keepalive for websocket clients.
     - Wire protocol v0 (binary, explicit encode/decode, shared header): join/welcome,
       input, per-tick command list.
     - Server: connection <-> slot, per-tick command log, broadcast, late-join by replay.
     - Client: JS WebSocket moves bytes only; wasm parses, queues tick commands, ticks
       only when a tick's commands have arrived.
     - Two-tab proof: identical state in sync (incl. second tab joining late).
   - End-to-end proof: two browser tabs against the same server show identical
     GameCommon-driven state changing in sync.

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
