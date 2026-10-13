# Game Server Match Lifecycle

Started: 2026-10-03

## Goal

Server stays up for days while matches start and end repeatedly. Players
connect into an open lobby, the lobby starts a match, the match plays out and
ends, the slot is reclaimed.

## Scope

- Open lobby: player joins via a special game message; lobby answers
  LOBBY_JOINED or a refusal.
- MATCH_JOINED becomes MATCH_STARTED (sent once the placement phase is over,
  before normal ticking).
- Lobby starts a match "whenever suitable" (rule to decide; replaces the
  2-connection headcount start).
- Match start: placement phase, see `start_location_choice_2026-10-03.md`.
- Match end and slot reclaim: arena reset, clients returned to the lobby.
- Extended by `lobby_system_2026-09-27.md` (player list, ready-up), a separate unit.

## Decisions

- Start rule: ~60 s countdown begins when the first player joins the lobby; match starts when it ends with whoever is present (AI fills the rest); never restarts once begun; resets if the lobby empties. Shown on the client screen.
- Match end (placeholder): one player controls 80% of the world population.
- Disconnect: lobby frees the seat; in match the player slot stays, no input.
- Slot 0 hard-routing already replaced.

## Not included

- Catch-up / late join (`backlog/catchup_late_join`), persistent client
  identity (`backlog/persistent_client_identity`).

## Progress

- [DONE] Slots run in parallel; arena reset and reuse on reset (commit 2ba631d). Client attach is first-fit, marked @TEST.
- Current state: start = 2 live clients (hard-coded test params), MATCH_JOINED sent at start, end = all humans left (MATCH_ENDED never sent), client has no lobby screen and ignores MATCH_ENDED.
- [DONE, not compile-checked] Server side: countdown, LOBBY_JOINED (0) / MATCH_STARTED (1) / MATCH_ENDED (2) with shifted enum, 80% end check, attach only to lobbies, bug fixes below. No lobby refusal (clients retry attach every tick).
- [DONE, not compile-checked] Client side: lobby_ui.ts (countdown, player count), MATCH_ENDED handling (reset state, hide canvas/HUD, notice).
- [DONE, not compile-checked] Immediate start when lobby is full; player count only from `match_params->player_count`; LOBBY_JOINED = countdown_ms ui32, lobby_player_count ui16, match_slot_index ui8, game_match_start_params; lobby UI "Lobby #N - Players x / y"; 80% check skipped for 1-player matches.
- Decision: random_seed in LOBBY_JOINED left as is (placement phase will make it moot).
- Tested by user: lobby, countdown, immediate start, match end all work.
- (old) To verify by hand: build server + wasm + `npm run build`; one client -> countdown -> match with AI; join/leave in lobby; 80% end -> notice -> lobby again.
- Known issues: client tile arrays for render state allocated per match start from backend memory, never freed (grows over repeated matches); stale imprint text in include_game_common.imprint.md (MATCH_ENDED as Kick, "Join Match"/"Match Joined") needs user edit.
- Next: user review/build; then placement phase (`start_location_choice`) or lobby_system.
- Tasks (original list):
  - Bug fixes: detach with unattached client (match_slot 255, out-of-bounds); `player_index <= player_count` should be `<`; attach assert reachable on non-lobby slots (loop tries WAITING/ENDED slots); AWAITING_CLEANUP is a dead end; "slot 0" log text.
  - Messages: LOBBY_JOINED (with lobby state), MATCH_STARTED rename, MATCH_ENDED sent; lobby refusal.
  - Countdown after headcount reached, sent to clients, shown on client screen.
  - Match end at 80% world population by one player; clients notified and returned to lobby; client resets local match state and hides canvas.
  - Late attach to a MATCH_ONGOING slot: stop allowing it (no MATCH_JOINED sent).
  - Placement phase: separate unit (`start_location_choice`).
