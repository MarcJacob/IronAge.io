# ARTISAN SKILL: add a game command

Derive a new command from existing ones. Precedents: FOUND_SETTLEMENT (one guid),
SET_ENTITY_ATTACK_TARGET (two guids, right-click trigger), RAISE_ARMY (HUD button +
mode, guid + location), BUILD_ROAD (mode, guid + two locations).
Rewritten 2026-10-18 after the command table and GDO pipeline; re-verify symbols against code.

Path: TS `send_command("NAME", {...})` builds the command in a Game Data Object (GDO)
in the wasm client -> client queues it -> server validates -> `match_tick` applies.
No command byte offsets exist in TS.

Enum values go on the wire: only append. C++ uses tabs, TS uses 4 spaces.

## A. game_common

Files: *include/game_common/match/commands.h*, *src/game_common/match_commands.cpp*.

1. `MATCH_COMMAND_TYPE`: append a value before `TYPE_COUNT`.
2. Inside the packed region, copy a precedent block: `command_data_<name>` struct
   (fixed-width fields, no padding, small) and the `command_<name>_validity_check` /
   `command_<name>_apply` declarations. Both take `const match_command_header&` and read
   their struct with `get_command_data<command_data_<name>>()` as first line.
3. Right below the struct: `GAME_DATA_OBJECT_DEF_BEGIN` block, one `GAME_DATA_PROPERTY` per
   member. Property name == struct member name. Types: `ENTITY_GUID`, `WORLD_LOCATION`, ints.
   No 64-bit types. Must fit the 128-byte GDO buffer.
4. Add a row to `MATCH_COMMAND_TABLE` (name = bare enum identifier, type, struct size,
   validity, apply). Rows are in enum order; a missing or misordered row fails a static_assert.
   Never store row or function pointers anywhere (not stable across hot reloads).
5. match_commands.cpp: implement validity (early false: entity valid, type, ownership,
   capacity) and apply (re-run validity first, then mutate via the helpers in
   *game_common_internals.h*). Balance numbers as `constexpr` above the function.

## B. Web client platform

File: *src/platform/web_game_client/web_game_client_input_commands.cpp*.

6. Add a `case` for the command in `gdo_buffer_create` (copy a precedent).

## C. Frontend TS

Folder: *src/platform/web_game_client/front/src/*.

7. At the call site (mode, `play_common.ts` or HUD handler) call `send_command("NAME", {...})`
   and, if the UI needs it, `is_command_valid("NAME", {...})` from *input.ts*. Property
   names equal the struct member names. A value is a number, or `{x, y}` for vec2 /
   world location. A wrong name logs the command's property list and nothing is sent.
8. Trigger, as the task says:
   - HUD button: *web/index.html* button in `#game_hud_settlement_bar`; *ui/settlement_bar_ui.ts*
     (variable, listener, visibility); the owning mode passes the callback in `on_entry` and
     `on_resumed` (*modes/settlement_selected.ts*).
   - Right-click order on the selected entity: the owning mode sends it in its `on_context_menu`
     (*modes/army_selected.ts*) with `send_command` and `Render.page_to_tile`. No input events
     exist for commands; `INPUT_EVENT_TYPE` only has `VIEWPORT_CONTROL`.
   - Interactive mode: copy *modes/road_building.ts* or *modes/army_raising.ts*.
9. State: `get_selected_entity_guid()`, `get_local_player_id()` (main.js),
   `Backend.query_entity(guid)`, `pick_entity_at(x, y, exclude_owner?)`.

## D. Checks

- TS property names match the struct member names; command name matches the table row.
- Grep the new enum name and confirm a hit in every place above.
- The enum value may already exist as a stub: check before adding.
- game_common randomness only via `match.main_rand_gen`.

## E. No precedent: stop and report

- Tick-time mechanics or world state the task does not specify.
- New entity types; changes to view / render state structs.
- New UI interaction kinds beyond one button, one context-click branch in a mode or a mode.
- Payloads with non-guid / non-coordinate data, variable length, over 128 bytes (GDO buffer)
  or 64-bit properties.
- Commands not from a local player action.
- Cost, balance or design rules the task does not specify.
- Protocol, platform or server changes.
