# Server test mode, authentication, lobby list

Started: 2026-10-25. Scope grew: also covers connection flow (auth -> lobby list -> join). Supersedes the lobby-control part of `lobby_system_2026-09-27.md` and the auth part of `server_dev_mode_2026-10-03.md` (both stay open for remaining scope).

## Scope

Server + platform-independent client state (`src/game_client`, wasm) + client backend reader. Client UI/UX is a later step (`web_ui_static_modules_2026-10-23.md`).

## Wire protocol (decided)

- `REQUEST_AUTHENTICATION` (client -> server): optional client name, not zero-terminated (string-view friendly), max length 32. Empty -> server assigns `Anon_XXXXX` (X = random number from platform entropy). Duplicate names: nothing special.
- `AUTHENTICATED` (server -> client): confirmation only; may carry an authentication key for future quick-reconnect (stored in browser later). Carries no server-wide info / test flag.
- `SERVER_LOBBY_LIST` (server -> client): sent regularly once authenticated; list of available lobbies + characteristics, perhaps embedding the same lobby-state struct used inside a lobby.
- `REQUEST_JOIN_LOBBY` (client -> server): server tries to attach the client; refused if lobby full.
- `JOIN_LOBBY_RESPONSE` (server -> client): always sent; answer + error code with the reason (client shows it to the player).
- `SIGNAL_LEAVE_LOBBY` (client -> server): client leaves; its slot is freed.
- `LOBBY_CLIENT_KICKED` (server -> client): used to boot players above a lowered player cap. No interim UI handling needed.
- On success the client receives lobby state messages as today; behavior inside the lobby unchanged except test-mode controls.

## Test mode (decided)

- Server CLA launch parameter AND a per-lobby flag; for now every lobby follows the server's flag.
- Everyone in a test lobby has admin controls (no per-player permissions). Normal security otherwise: authentication required before joining ANY lobby (may be a client state / flag); input validated; a client can only act on its own membership (no kicking others by request); admin commands only from clients inside a test lobby. Only duplicate names are harmless.
- Test lobby never starts on its own; started only by a start command.
- Admin commands: start match; set player count and map size (hard max in code), changeable at any time; set seed (stopgap until pre-made maps; map-choice command later).
- Players above a lowered cap are kicked and return to the main menu (comes with UI overhaul; interim client behavior to decide).
- No add / remove AI command: AI fills every slot not taken by a human.
- Non-test lobbies keep the automatic countdown.

## Client architecture (decided)

- Platform-independent client state (`src/game_client`) is the ultimate handler: tracks which state the client is in (connecting, authenticating, lobby list, in lobby, in match...) and sets up resources as needed.
- UI will be built in the single `index.html` by showing / hiding panels: name entry -> login -> lobby list with join buttons -> lobby (+ admin controls in test mode).

## Tasks

1. [DONE, unbuilt] Protocol + server: CLA `-test_mode=1`, per-lobby flag, auth, lobby list (1/s to authenticated clients outside a slot), join / leave, test-lobby behavior, 4 admin messages, kick on cap lowering. Server needs full restart (init params layout changed).
2. [DONE, unbuilt] Client state (`CLIENT_CONNECTION_STATE`, `game_client.h` connection block), wasm exports (`web_game_client_connection.cpp`), `backend.ts` readers / builders. Temporary path: `main.ts` auto-authenticates, `LobbyBase` auto-joins first non-full lobby (and rejoins after a match).
3. [DONE, unbuilt] Client UI as the pilot of `web_ui_static_modules`: login, lobby list, lobby (+ admin controls) modules, `lobby_base.ts` as driver, temporary auto-join removed, keydown input fix. Match end returns client to the lobby list. Dead `ui/lobby_ui.ts` to delete (user).

## Decisions (builder pass)

- Lobby id = slot index (4 slots = 4 lobbies). Limits in `game_messages.h`: players 1-32, map size 1-8 regions per side.
- Lowering the cap first moves a human above it to a free seat below, kicks only if none.
- Leaving after match start behaves like a disconnect. Names: control chars become `?`; second auth request ignored.
- Confirmed by user: max map size stays 8x8 (memory to be tested); cap lowering moves before kicking.
- Rename "client connection" to "client state" (broader concerns than the connection flow). Strings: core string library (views / static strings), NOT zero-terminated on the wire; limited lengths (name max 32). Follow-up pass delegated 2026-10-25.
- Follow-up pass done (unbuilt): `CLIENT_STATE` rename (`web_game_client_state.cpp`, `get_client_state`); names use core `ia_static_string` as `player_name` (36 bytes: 32 chars + u32 length at +32, not zero-terminated); `AUTHENTICATED` 52 bytes. Alternative on the table: u8 length + 32 bytes.
- Player names in match [DONE, unbuilt]: `MATCH_STARTED` (the actual message name in code) has `player_count` x `player_name` appended after its fixed payload; humans by login name, others "CPU N"; client stores them, lookup `Backend.get_player_name(idx)`. Properties panel shows owner as grey subtitle (settlements / armies; workers and caravans none); army title "Army".
- Notes: `src/game_server/.ai_imprint/connection_flow_and_lobbies.md`.

## Open questions

- Does an 8x8 map fit client 64 MiB / server 128 MiB slot memory? (user to test; builder suggests max 6 if not)
- Client has no path back to "connecting" after connection loss.
- Kicked client auto-rejoins via the temporary path (goes away with UI).

- How CLA reach the hot-reloadable server code; how lobbies map to existing match slots (slot 0 is hard-routed today).
- Client UI keydown fix: handler (`input.ts:245`) swallows keys while an `<input>` has focus.

## Next step

All tasks built and hand-tested by user (2026-10-25). Unit complete. Remaining open items: 8x8 map memory test, reconnect after connection loss, delete dead `ui/lobby_ui.ts`.
