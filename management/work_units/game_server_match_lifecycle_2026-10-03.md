# Game Server Match Lifecycle

Started: 2026-10-03. Status: DONE (tested by user).

## Goal

Server stays up for days while matches start and end repeatedly. Players
connect into an open lobby, the lobby starts a match, the match plays out and
ends, the slot is reclaimed.

## Decisions

- Start rule: ~60 s countdown begins when the first player joins the lobby; match starts when it ends with whoever is present (AI fills the rest); never restarts once begun; resets if the lobby empties. Starts immediately when the lobby is full. Shown on the client.
- Player count comes only from the slot's `match_params->player_count`.
- Match end (placeholder): one player controls 80% of the world population (skipped for 1-player matches), or all humans left. MATCH_ENDED sent with winner id; connection stays open and clients return to a lobby.
- Disconnect: lobby frees the seat; in match the player slot stays, no input.
- Attach is first-fit and only to IN_LOBBY slots (@TEST). No lobby refusal (clients retry every tick).
- LOBBY_JOINED = countdown_ms ui32, lobby_player_count ui16, match_slot_index ui8, game_match_start_params. random_seed in it left as is.

## Not included

- Catch-up / late join (`backlog/catchup_late_join`), persistent client
  identity (`backlog/persistent_client_identity`).
- Placement phase: `start_location_choice_2026-10-03.md`. Player list / ready-up: `lobby_system_2026-09-27.md`.

## Known issues

- Client memory growth fixed (not compile-checked): fixed 32 MiB `render_tiles_mem` arena cleared by `game_client_end_local_match`, exported as `web_client_end_match`, called from the front end on MATCH_ENDED. Worlds above ~6.7M tiles trap on the existing ASSERTs. Verify by playing several matches on one connection and checking wasm memory size stays flat.
