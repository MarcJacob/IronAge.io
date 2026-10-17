# BACKLOG: Match commands simplification

Input date: 2026-10-03

## Item

Cut the ~9 boilerplate files / steps to add a command (see
`management/artisan_skills/add_command.md`). Options:

1. Dedupe the sequence validator: `match_command_sequence_output_validated`
   calls `match_command_validity_check` instead of its own switch (keep the
   out-of-range type guard). ~15 lines; removes the switch that silently drops
   sequences when forgotten. Low risk.
2. HUD button table in `game_ui.ts`: one row `{id, label, visible(view),
   send(guid)}` replaces the `index.html` button and 4 sites in `game_ui.ts`;
   buttons created in `init_game_ui`, updated in a loop in `update_game_ui`.
   Frontend only, ~40 lines. Low risk.
3. X-macro command list in `commands.h`: one `X(NAME, name)` line drives the
   enum, `get_command_data_size` and the validity / apply dispatch (relies on
   `command_<name>_validity_check` / `_apply` / `command_data_<name>` naming).
   Costs readability, deviates from the explicit style. Validity / apply
   declarations could leave the public header.
5. Backend export "is this command valid now" for the local player, so the HUD
   uses server-side validity and the duplicated cost constants / predicates in
   TS go away. New API surface; changes when buttons appear.

Validity / apply functions, payload structs and the TS send wrapper are real
design content and stay explicit.

## Why

Per-command boilerplate is the main cost of adding commands and the main source
of missed-step errors. Source: artisan report after adding `SPAWN_CARAVAN`.
