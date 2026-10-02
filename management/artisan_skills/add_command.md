# ARTISAN SKILL: add a game command

Derive a new command from existing ones. Precedents: FOUND_SETTLEMENT (one guid),
SET_ENTITY_ATTACK_TARGET (two guids, right-click trigger), SPAWN_ARMY (HUD button).
Written 2026-10-03; re-verify symbols against code.

Path: frontend `input.ts` send_* -> input event -> client C++ handler queues a
`MATCH_COMMAND_TYPE` payload -> server validates -> `match_tick` applies.

Enum values go on the wire: only append. C++ uses tabs, TS uses 4 spaces.

## A. game_common

Files: *include/game_common/match/commands.h*, *src/game_common/match_commands.cpp*.

1. `MATCH_COMMAND_TYPE`: append a value before `TYPE_COUNT`.
2. Inside the packed region, copy the FOUND SETTLEMENT block: `command_data_<name>`
   struct (fixed-width fields, no padding, small), `command_<name>_validity_check`,
   `command_<name>_apply` declarations.
3. `get_command_data_size`: add the case.
4. match_commands.cpp: implement validity (early false: entity valid, type, ownership,
   capacity) and apply (re-run validity first, then mutate via the helpers in
   *game_common_internals.h*). Balance numbers as `constexpr` above the function.
5. Add a case to each of the three dispatch switches: `match_command_validity_check`,
   `match_command_apply`, `match_command_sequence_output_validated` (missing this one
   silently drops the rest of a sequence).

## B. Game client C++

Files: *include/game_client/game_client_backend.h*, *src/game_client/game_client.h*,
*src/game_client/game_client_input.cpp*.

6. `INPUT_EVENT_TYPE`: append a value.
7. Packed `input_event_payload_<name>` struct + `INPUT_EVENT_PAYLOAD_SIZE_GUARD`.
8. Declare `game_client_input_<name>` in game_client.h (client input section).
9. Implement it in game_client_input.cpp, copying `game_client_input_found_settlement`
   (queue push, drop if full, fill fields; no validation, the server validates).
10. Add the case to the `game_client_process_input_event` switch.

## C. Frontend TS

Folder: *src/platform/web_game_client/front/src/*.

11. backend.ts: append the same `INPUT_EVENT_TYPE` value, same order as C++.
12. input.ts: `send_<name>` copying `send_spawn_army` / `send_attack_target`
    (`begin_input_event`, little-endian `setUintXX` at packed struct offsets,
    `commit_input_event`).
13. Trigger, as the task says:
    - HUD button: *web/index.html* button in `#game_hud_selected_panel`; *game_ui.ts*
      (variable, listener in `init_game_ui`, visibility in `update_game_ui`). game_ui only
      signals; sending stays in input.ts.
    - Right-click: a branch in `send_context_order`.
14. State: `get_selected_entity_guid()`, `get_local_player_id()` (main.js),
    `Backend.query_entity(guid)`, `pick_entity_at(x, y, exclude_owner?)`.

## D. Checks

- C++ payload byte layout == `send_<name>` offsets; enum orders match.
- Grep the new enum name and confirm a hit in every place above.
- The enum value may already exist as a stub: check before adding.
- game_common randomness only via `match.main_rand_gen`.

## E. No precedent: stop and report

- Tick-time mechanics or world state the task does not specify.
- New entity types; changes to view / render state structs.
- New UI interaction kinds beyond one button or one `send_context_order` branch.
- Payloads with non-guid / non-coordinate data, variable length, or over 64 bytes.
- Commands not from a local player action.
- Cost, balance or design rules the task does not specify.
- Protocol, platform or server changes.
