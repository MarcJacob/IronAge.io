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

## Open design

- Start rule: headcount, ready-up, timer.
- Match end conditions (placeholder until victory phase).
- Multiple concurrent matches vs slot 0 hard-routing.
- Client disconnect during lobby / placement / match.

## Not included

- Catch-up / late join (`backlog/catchup_late_join`), persistent client
  identity (`backlog/persistent_client_identity`).

## Progress

Not started.
