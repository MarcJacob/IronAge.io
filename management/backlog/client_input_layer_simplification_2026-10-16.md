# Client input layer simplification

Input date: 2026-10-16

## Item

The client input event enum, payload structs and `game_client_input_*` functions duplicate the
command definitions in game common. Simplify so a new command needs less client boilerplate.

## Why

Raised by the road front peer after adding `BUILD_ROAD`. The new `game_client_query_command_valid`
(reusing the server validity check) is a first step in that direction. Related: `match_commands_simplification_2026-10-03.md`.
