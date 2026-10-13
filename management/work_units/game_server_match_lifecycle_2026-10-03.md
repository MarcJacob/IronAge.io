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

- Start rule: headcount (as now) plus a countdown, shown on the client screen.
- Match end (placeholder): one player controls 80% of the world population.
- Disconnect: lobby frees the seat; in match the player slot stays, no input.
- Slot 0 hard-routing already replaced.

## Not included

- Catch-up / late join (`backlog/catchup_late_join`), persistent client
  identity (`backlog/persistent_client_identity`).

## Progress

- [DONE] Slots run in parallel; arena reset and reuse on reset (commit 2ba631d). Client attach is first-fit, marked @TEST.
- Current state: start = 2 live clients (hard-coded test params), MATCH_JOINED sent at start, end = all humans left (MATCH_ENDED never sent), client has no lobby screen and ignores MATCH_ENDED.
- Tasks:
  - Bug fixes: detach with unattached client (match_slot 255, out-of-bounds); `player_index <= player_count` should be `<`; attach assert reachable on non-lobby slots (loop tries WAITING/ENDED slots); AWAITING_CLEANUP is a dead end; "slot 0" log text.
  - Messages: LOBBY_JOINED (with lobby state), MATCH_STARTED rename, MATCH_ENDED sent; lobby refusal.
  - Countdown after headcount reached, sent to clients, shown on client screen.
  - Match end at 80% world population by one player; clients notified and returned to lobby; client resets local match state and hides canvas.
  - Late attach to a MATCH_ONGOING slot: stop allowing it (no MATCH_JOINED sent).
  - Placement phase: separate unit (`start_location_choice`).
