# Start location choice (placement phase)

Started: 2026-10-03. Part of `game_server_match_lifecycle_2026-10-03.md`.

## Goal

Before the match ticks, each player picks a start location; picks become spawns at match start.

## Decisions

- No new phase message: client is in placement mode between receiving MATCH_STARTED and the first SERVER_TICK.
- New CLIENT_PLACEMENT_PICK message; server stores the pick per player and rebroadcasts every valid pick to all players.
- Picks are not secret; can be changed any number of times until the game begins. No early end.
- Every player (non-choosers, AI) is pre-placed at a random valid spot, as if they had picked it.
- Placement countdown is separate from the lobby countdown; it comes with MATCH_STARTED and is only indicative. The client stays in placement mode until the first SERVER_TICK, with input in an alternative mode: clicking anywhere on the map changes the pick.
- Invalid picks are discarded silently (as with invalid input commands). For now invalid = non-buildable tile (water, coastline, mountain); more conditions later.
- Client UI: countdown, a dot per player's pick, own dot green (animated later).
- Map comes from the standard world state init for now.

## Open design

- Message layouts (packed structs, cross-check JS readers).

## Progress

User implements this himself. Not started.
