# Placement pick validity

Started: 2026-10-14 (from backlog, raised 2026-10-13). Follow-up to `start_location_choice_2026-10-03.md`.

## Goal

Server rejects invalid placement picks; random pre-placement prefers valid spots.

## Decisions

- Game common `world_is_valid_settlement_location` (world_entities.cpp, `SETTLEMENT_MIN_DISTANCE` = 16 in world.h): in bounds, plains or hills, tile unclaimed or owned by the requester, no settlement within 16 tiles. Reused by the found-settlement command.
- Distance between players' picks is checked in the server only (`match_is_valid_pick`), not in game common.
- Invalid picks discarded silently. Random pre-placement: up to 32 tries per player, then the last location is accepted.
- `command_server_spawn_player_validity_check` left untouched.

## Tasks

- [DONE] Validity function in game common.
- [DONE] Server pick validation (incl. distance to other picks) + pre-placement retry loop. Reviewed by user.
- [DONE] Check added to found-settlement command validity and apply (apply is not preceded by a fresh validity check: commands are validated on receipt, then applied; apply re-checks silently).

## Next

None. Unit complete, reviewed by user.
