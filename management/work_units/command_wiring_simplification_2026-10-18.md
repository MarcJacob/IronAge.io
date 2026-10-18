# Command wiring simplification

Input dates: 2026-10-03 (match commands), 2026-10-16 (client input layer). Started: 2026-10-18 (both moved out of backlog and merged; the former is in `archive/match_commands_simplification_2026-10-03.md`).

## Goal

Make it as easy as possible to wire a game feature to a command, itself wired to a modal control scheme on the client. Today adding a command touches ~9 files / steps (see `management/artisan_skills/add_command.md`) and a lot of client / UI boilerplate, and is the main source of missed-step errors.

## Scope

Game common (commands):
- Dedupe the sequence validator: `match_command_sequence_output_validated` should call `match_command_validity_check` instead of its own switch (keep the out-of-range type guard). Removes the switch that silently drops sequences when forgotten.
- Option: X-macro command list in `commands.h` driving the enum, `get_command_data_size` and validity / apply dispatch. Costs readability and deviates from the explicit style; decide after the survey.

Client / front:
- Client input event enum, payload structs and `game_client_input_*` functions duplicate the command definitions in game common.
- HUD / action bar wiring per command: table-driven (one row per action) instead of per-site edits.
- Front end modal system (`front/src/modes/`) integration: command -> mode -> UI as one declaration where possible.
- Build on `game_client_query_command_valid` (reuses the server validity check): UI visibility and enablement from server-side validity, dropping duplicated cost constants / predicates in TS. Changes when buttons appear.

Stays explicit (real design content): validity / apply functions, payload structs, the TS send wrapper.

## Tasks

1. [DONE, unverified in game] Sequence validator calls `match_command_validity_check` (type guard kept).
2. [DONE, unverified in game] Client input dedupe: per-command input events removed; `client_send_match_command(type)` wasm export reads the bare command struct from the input buffer (same layout as the validity query); `INPUT_EVENT_TYPE` keeps VIEWPORT_CONTROL and SET_TARGET_LOC only.
3. Drive TS enums / payload layouts from C++ (Game Data Object?): design pending.
4. Table-driven HUD / modal declaration.
5. Update `artisan_skills/add_command.md` (builder suggestions available from the step 1-2 report: 2 dispatch switches, no client payload steps, `send_match_command`, HUD in `settlement_bar_ui.ts`, RAISE_ARMY).
6. Server-validity-driven UI: last, needs its own design (button-level queries have no payload).

## Decisions

- Client does not filter command types (incl. SERVER_SPAWN_PLAYER); server-side protections handle it; special commands may be exposed in some contexts later.
- Client check on send: type < TYPE_COUNT and command size <= 64 (no length available).
- X-macro: skipped for now; revisit after step 3.

## Notes

- Related experiment (client UX session): raise location / roads on any player-owned tile (roads only through owned tiles). Validity change reaches the front end automatically; the pathing territory limit (`within_settlement_territory`, also passed by the preview's `query_path`) and the road origin rule need their own change.
- Sources: artisan report after adding `SPAWN_CARAVAN`; road front peer after adding `BUILD_ROAD`.
- Update `artisan_skills/add_command.md` when the process changes.
- Multi-file architecture change: survey and design before editing.

## Survey findings (done)

- Adding a command today: ~12 steps over 3 layers (game common enum / struct / size / validity / apply / 3 dispatch switches; client enum / payload struct / `game_client_input_*` / switch case; TS 2 enum mirrors / `write_*_payload` / HUD / mode).
- Silent failures: sequence validator switch (release `default` drops the rest of the sequence), size switch (size 0), validity / apply switches (no-op); TS enum mirrors and payload offsets unchecked.
- Client input payload structs duplicate command structs (pure field copies); only VIEWPORT_CONTROL and SET_TARGET_LOC are real input events.
- `game_data_object.h` (commit f1cbb07) is a demo: enumerates props of a packed struct (name / type / offset). No consumer. No `entity_guid` / `world_location` prop types.
- HUD buttons hand-coded in `index.html` + `settlement_bar_ui.ts`; `road_building.ts` / `army_raising.ts` share one shape.
- UI never uses validity to show / enable buttons; no TS cost constants exist yet.
- `artisan_skills/add_command.md` is stale (SPAWN_ARMY -> RAISE_ARMY; HUD in `settlement_bar_ui.ts`, not `game_ui.ts`). `game_client_input_spawn_caravan` declared, never defined.

## Next step

User checks tasks 1-2 in game; then design task 3 (role of Game Data Object: commands only or messages too; runtime schema from wasm vs generated file).
