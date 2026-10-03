# BACKLOG: Match commands simplification

Input date: 2026-10-03

## Item

Reduce the number of files / steps needed to add a new command (currently ~9
boilerplate files for a guid-only command). Ideas to pick through:

1. Dedupe the sequence validator: `match_command_sequence_output_validated` calls
   `match_command_validity_check` instead of its own switch. Keep the
   out-of-range type guard. ~15 lines, removes one dispatch switch (the one
   that silently drops sequences when forgotten).
2. HUD button table in `game_ui.ts`: one row `{id, label, visible(view), send(guid)}`
   replaces the `index.html` button and 4 sites in `game_ui.ts` (variable,
   listener, null guard, visibility). Buttons created from the table in
   `init_game_ui`, updated in a loop in `update_game_ui`. Frontend only, ~40 lines.
3. X-macro command list in `commands.h`: one `X(NAME, name)` line drives the
   enum, `get_command_data_size` and the validity / apply dispatch, relying on
   the `command_<name>_validity_check` / `_apply` / `command_data_<name>` naming.
   Preprocessor only. Costs some readability and deviates from the explicit style.
   Validity / apply declarations could also leave the public header.
5. Backend export for "is this command valid now": the WASM backend exposes a
   validity query for the local player and a command, so the HUD shows buttons
   from server-side validity and the duplicated cost constants / predicates in TS
   go away. New backend API surface; changes when buttons appear (e.g. capacity
   checks would also hide the button).

## Description

Source: artisan report after adding `SPAWN_CARAVAN` (see
`management/artisan_skills/add_command.md` for the current per-command path).
Validity / apply functions, payload structs and the TS send wrapper are real
design content and stay explicit. 1 and 2 are low risk; 3 and 5 are style /
design calls.

## Why

Per-command boilerplate across game_common, client C++ and the frontend is the
main cost of adding commands, and the main source of missed-step errors.
