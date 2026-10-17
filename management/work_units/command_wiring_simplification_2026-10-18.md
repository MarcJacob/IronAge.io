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

Not broken down yet.

## Notes

- Related experiment (client UX session): raise location / roads on any player-owned tile (roads only through owned tiles). Validity change reaches the front end automatically; the pathing territory limit (`within_settlement_territory`, also passed by the preview's `query_path`) and the road origin rule need their own change.
- Sources: artisan report after adding `SPAWN_CARAVAN`; road front peer after adding `BUILD_ROAD`.
- Update `artisan_skills/add_command.md` when the process changes.
- Multi-file architecture change: survey and design before editing.

## Next step

Survey (builder, read-only): every place touched when adding a command today, game common to front; then propose a design.
