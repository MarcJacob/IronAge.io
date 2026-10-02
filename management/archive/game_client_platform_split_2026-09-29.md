# WORK UNIT: Game Client / Platform Split

Started: 2026-09-29

Goal: correct the web client's architecture to match the Game Server's pattern -
a platform-independent Game Client backend (`src/game_client/`, public contract in
`include/game_client/`) used as a service by a platform layer
(`src/platform/web_game_client/`), instead of the backend living inside the web
platform folder.

## Tasks

- [x] Reconcile naming between the public contract and the internal
  implementation - settled on the shorter `game_client`/`game_client_*`
  names throughout (public header, internal impl, platform layer all agree
  now).
- [x] Implement the accessor functions the public header declares
  (render-state getter, local-match-params getter, net-message-buffer
  getter/size, net-output-buffer getter/size) - compiles and links.
- [x] Decide fate of the old WASM test-mode code - cut deliberately, a
  proper client-side testing system will replace it later.
- [x] Fix the TS side to match the current C++ API: `backend.ts`'s
  `read_render_state` now reads the real (smaller, `zoom_level`-free)
  `client_render_state` layout; `refresh_render_state()` replaced by
  `start()`/`tick(delta_time)` wrapping the new `web_client_start`/
  `web_client_tick` exports, wired up in `main.ts`; `debug.ts` no longer
  displays zoom_level.
- [x] Match info / controlled player id gap closed differently than planned:
  rather than adding dedicated WASM exports, `controlled_player_id` and
  `world_size` were added directly to `client_render_state` (public header)
  and filled in by `game_client_rebuild_render_state` (from
  `backend.controlled_player_id` and `match.start_params->world_dimensions`)
  - already exposed to JS via the existing `client_get_render_state` export,
  no new plumbing needed. `backend.ts`'s `read_match_info()` /
  `read_controlled_player_id()` are gone; `main.ts` forces one immediate
  `Backend.tick(0)` on MATCH_JOINED so the render state is populated before
  `init_render`/`init_input` need it, then reads both off that. Also fixed
  a real (separate) bug found along the way: `client_viewport_state.world_size`
  was never set anywhere, so viewport-clamping was silently broken since the
  split - now set in `game_client_begin_match_with_params`.
  `Core.MatchInfo` (now unused) removed from `core.ts`.
- [x] Runtime bugs found while getting an actual match to run end-to-end
  (compiling/linking wasn't enough - see Progress for the full list). Client
  now connects, joins a match, renders, and sends input back without the
  server dropping it.
- [ ] Resume the Generalized Input Events work (paused mid-implementation -
  see `client_input_2026-09-27.md`); the code/bytes split
  (`wasm_client_input.cpp` vs. backend-side input handling) should now have
  an obvious home.

## Progress

- Renamed `src/client_web/` (backend logic) -> `src/game_client/`, its public
  contract split out to `include/game_client/game_client_backend.h`.
- Renamed `src/platform/wasm_web_client/` -> `src/platform/web_game_client/`,
  and moved `src/client_web/front/` + `src/client_web/web/` in under it
  (frontend TS/JS and static web files now live with the platform they
  belong to, not the backend).
- `CMakeLists.txt` / `deploy_web_client.bat` updated to the new
  `web_game_client` path (confirmed correct).
- Split WASM input-export wrappers into their own file
  (`wasm_client_input.cpp`), separate from `wasm_client_main.cpp` - still
  wraps the old per-feature functions (`client_apply_viewport_input`,
  `client_input_set_target_loc`), generalized code/bytes format not
  migrated yet.
- Since the first pass of this analysis: internal `game_client` struct
  reworked with a real latent input model - `game_client_input_state`
  (movement vector, desired zoom level, zoom target) is set once per input
  call and applied continuously in `game_client_tick` via
  `game_client_apply_viewport_input(backend, delta_time)`, rather than
  combining "set" and "apply" in one call. Duplicate `input` member,
  use-before-definition of `client_viewport_state`, the `move_time` bug and
  both include-path typos from the first pass are all resolved.
- New platform-side files (`wasm_client.h`, `wasm_client_main.cpp`,
  `wasm_client_input.cpp`) call through to the public header, which now
  matches the internal implementation 1:1 (`game_client` struct,
  `game_client_init`/`_tick`/`_get_render_state`/`_get_net_msg_buffer(_size)`/
  `_get_net_msg_output(_size)`/`_process_game_message`/
  `_input_set_target_loc`/`_set_viewport_input`).
- 2026-09-29: full build compiles and links. Remaining tasks are cleanup /
  follow-through, not build breakage.
- 2026-09-30: four runtime bugs found getting the build to actually run
  end-to-end, in the order hit:
  1. `CLIENT_MEMORY_BLOCK` didn't budget for `sizeof(game_client)` itself
     before the two sub-arenas - `game_client_init()` silently overran and
     returned null. Fixed by sizing `CLIENT_MEMORY_SIZE` generously (`MiB(128)`).
  2. `web_client_state`'s `game_client* backend = nullptr;` default member
     initializer made its default constructor non-trivial, and this
     `-Wl,--no-entry` build calls `__wasm_call_ctors()` at the top of
     *every* exported call instead of once at startup - silently resetting
     `WEB_CLIENT.backend` to null after the first call. Fixed by dropping
     the initializer (struct is trivial again; `web_client_start()` already
     zeroes it explicitly). Worth checking other globals in this build for
     the same non-trivial-initializer trap.
  3. `game_client_begin_match_with_params` cleared + re-copied `params`
     into `local_match_mem`, but the caller had already copied params into
     that same arena and passed a reference in - the second clear zeroed
     them (world dimensions included) first. Looked like "match won't
     start" / "canvas stays 0x0". Fixed by dropping the redundant
     clear+copy.
  4. `game_client_get_net_msg_output_size()` returned the buffer's fixed
     capacity (1024) instead of `net_output_msg_size` (actually-pending
     size, usually 0) - client sent a full stale 1024-byte frame after
     every message, server rejected the size mismatch with close code
     1007. Fixed to return the real field. Also found
     `game_client_output_client_tick_message()` had been dropped from the
     `SERVER_TICK` handler during the split (declared, defined, never
     called) - restored the call so the client sends input back at all.
- End of day 2026-09-30: client connects, joins a match, renders, and
  round-trips input without the connection dropping. Functionally done;
  only the Generalized Input Events handoff below remains open.

## Notes / Decisions

- Rationale for the split: the Game Client backend is meant to be a
  platform-independent "service" (like the Game Server), fed input events
  and network messages by whichever platform hosts it (web/WASM today,
  maybe native later) and asked for render state in return - not a
  WASM-specific thing living under `src/client_web/`.
- This blocks the Generalized Input Events task from `client_input_2026-09-27.md`:
  that work is paused here, to resume once this split is compiling.
