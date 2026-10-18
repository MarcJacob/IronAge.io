# ARTISAN SKILL: add a match command (game common)

Derive a new match command from existing ones: the command itself, its validity feedback and
its apply. Game common only.

Precedents: FOUND_SETTLEMENT (one guid), SET_ENTITY_ATTACK_TARGET (two guids),
SET_ENTITY_MOVE_TARGET (guid + location), RAISE_ARMY (guid + location, population cost,
feedback), BUILD_ROAD (guid + two locations, path reasons, feedback).
Rewritten 2026-10-18; re-verify symbols against code.

Path: a command is built in a Game Data Object (GDO) -> queued -> validated by the server
(`match_command_sequence_output_validated`) -> applied by `match_tick`. The validity check can
also be run outside the server (previews, UI) with a feedback structure.

Enum values go on the wire: only append. C++ uses tabs.

Files: *include/game_common/match/commands.h*, *src/game_common/match_commands.cpp*.

## Steps

1. `MATCH_COMMAND_TYPE`: append a value before `TYPE_COUNT`.
2. Inside the packed region, copy a precedent block: `command_data_<name>` struct
   (fixed-width fields, no padding, small) and the `command_<name>_validity_check` /
   `command_<name>_apply` declarations. Both functions take `const match_command_header&`
   and read their struct with `get_command_data<command_data_<name>>()` as first line.
3. Right below the struct, a `GAME_DATA_OBJECT_DEF_BEGIN` block, one `GAME_DATA_PROPERTY` per
   member. Property name == struct member name (commands are filled in by property name).
   Types: `ENTITY_GUID`, `WORLD_LOCATION`, ints up to 32 bits. No 64-bit types. The GDO must
   stay under 128 bytes.
4. Add a row to `MATCH_COMMAND_TABLE` (name = bare enum identifier, type, struct size,
   validity, apply). Rows are in enum order; a missing or misordered row fails a static_assert.
   Never store row or function pointers anywhere (not stable across server hot reloads).
5. match_commands.cpp, validity: early false (entity valid, type, ownership, capacity), with
   the last parameter `match_command_validity_feedback* feedback`:
   - feedback null (server path): return false at the first error.
   - feedback given: record every way it fails with the helpers (`command_add_error`,
     `command_add_path_error`): error flag bit (`COMMAND_ERROR_*`) plus detail values
     (e.g. `population_needed`). Set only what the check cares about; no flag set == valid; a
     false result with no flag becomes `UNSPECIFIED`.
   - New kind of error: add a bit to `COMMAND_ERROR_*` (or a field to the feedback struct, which
     has a GDO). Report the new bit in the final report: users of the feedback keep a mirror.
   - Feedback is output only and never influences the simulation.
6. match_commands.cpp, apply: re-run validity first (with `nullptr`), then mutate via the
   helpers in *game_common_internals.h*. Balance numbers as `constexpr` above the function.
   If the command needs new world simulation mechanics, read
   `add_world_simulation_mechanic.md`.

## Checks

- Struct member names == GDO property names.
- Table row in enum order, name == enum identifier.
- Grep the new enum name and confirm a hit in every place above.
- The enum value may already exist as a stub: check before adding.
- Randomness in a command's apply or in world mechanics: see `add_world_simulation_mechanic.md`.

## No precedent: stop and report

- Tick-time mechanics or world state the task does not specify.
- New entity types; changes to view / render state structs.
- Payloads with non-guid / non-coordinate data, variable length, over 128 bytes or 64-bit
  properties.
- Commands not from a local player action.
- Cost, balance or design rules the task does not specify.
- Protocol, platform or server changes.
