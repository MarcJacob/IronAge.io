# MATCH AI

AI players for matches. Server side only: the web client never calls it (its code is unity-compiled in but unused).

## Rules

- **Volatile.** Expect this folder to be rewritten often. Keep everything minimal, do not build on it as if stable.
- **Component of the match code.** It knows the match / world public headers; they know nothing of it. Only hooks: the unity include in *game_common_main.cpp* and the server's calls (below). Unplug = remove both.
- **Observation only.** Decision code reads *ai_observed_world_state* (a copy, no pointers into the world) and never the world state. This is so AI can later run parallel to the next world tick.
- **One shared observation** for all AI players (full information game). Fixed capacities in the public header; zero is an empty world.
- **Not deterministic by design.** Not part of the world simulation, so decisions may use any randomness or local data. The commands they emit still go through the normal validated command path.
- **Same behavior for every AI.** No per-AI parametrization.

## Layout

- *include/game_common/match/ai/match_ai.h*: public header (observed state, ai_player_state, the two hook functions).
- *match_ai_main.cpp*: ai_output_commands (decision cadence, scratch sequence, validated push). Unity entry of the folder.
- *match_ai_observe.cpp*: ai_observe_world, world to observed state.
- *match_ai_behaviors.cpp*: one small function per behavior, plus ai_decide which lists them.

## Hook

The server (match_slots) allocates the observed state and the ai_player_state array when the match begins. After each match_tick it calls ai_observe_world.
Before each tick it calls ai_output_commands for every AI player, which pushes a validated sequence into the next tick's commands (like a human's). Invalid commands are dropped.

## Adding a behavior

- Need new data? Add a field to the observed structs (header) and fill it in match_ai_observe.cpp.
- Write *ai_behavior_xxx(ai_context&)* in match_ai_behaviors.cpp: read ctx.observed, push commands with ctx.commands.push_command, return.
- Add one line to ai_decide.
