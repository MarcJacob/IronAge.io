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

User implements this himself.
- [DONE] Server-side placement phase (commit 62fb2e4); match slots code split into lobby and ongoing-match (`game_server_match_play.cpp`) files.
- [DONE, not compile-checked] Client placement: placement.ts, placement UI + countdown bar, click-to-pick sent from TS, dots (own green, others red->gray by distance up to 64 tiles, alpha 0.25), SERVER_PLACEMENT_PICK handled; MATCH_STARTED now has placement_countdown_ms @0 (join_tick @4, controlled_player_id @8, params @10); pick handler guarded (MATCH_ONGOING + in_placement) and missing break fixed; backend.ts enum fixed; include_game_common.imprint.md updated.
- Fixed by user: lobby countdown stuck at 0 (first LOBBY_JOINED sent before countdown started; resend on countdown start).
- Still open (user, later): pick validity check, buildable-aware random pre-placement; imprint/comment staleness in game_server.imprint.md and match_slots.h:11.
- Server review findings (superseded where marked done above): MATCH_STARTED lacks the placement countdown field; `game_server_match_slot_handle_message` missing break after CLIENT_PLACEMENT_PICK (falls into client_tick handler); pick handler has no slot-state / in_placement guard; pick validity check is a TODO and random pre-placement is not buildable-aware (an invalid spot still spawns); backend.ts MESSAGE_TYPE enum stale (SERVER_TICK now 5, SERVER_TICK_BUNDLE 6, CLIENT_TICK 7); stale imprint text (include_game_common, game_server.imprint.md split, match_slots.h:11 comment).
- Pre-placements reach clients: one SERVER_PLACEMENT_PICK per player right after MATCH_STARTED. Pick payload: player_id ui16 @0, x ui16 @2, y ui16 @4 (tiles).
- Client builder blocked on the countdown field; waiting on the user's call (server edit by builder vs. by user; pick sent from TS directly).
- Remaining: client side (alternative placement input mode, pick dots with own dot green, placement countdown from MATCH_STARTED, first SERVER_TICK switches to match mode); verify server details.
