# MATCH AI

AI players for matches. Server side only: the web client never calls it (its code is unity-compiled in but unused). Game mechanics it relies on: *game_mechanics.imprint.md* (same folder).

## Rules

- **Volatile.** Expect this folder to be rewritten often. Keep everything minimal, do not build on it as if stable.
- **Component of the match code.** It knows the match / world public headers; they know nothing of it. Only hooks: the unity include in *game_common_main.cpp* and the server's `ai_update` call. Unplug = remove both.
- **Reads the world directly, never writes it.** No observed copy. All world reads are confined to *match_ai_world.cpp* (views of settlements / armies, tile and project queries); other files only use those accessors, so a later parallel / copy-based version has one place to change.
- **Commands only through the validated path** (`push_validated_sequence`). Invalid commands are silently dropped. The AI may use randomness and local data (not part of the simulation).
- **Same behavior for every AI.** No per-AI parametrization. All tuning constants are in *match_ai_internal.h*.

## Cadence

- The server calls `ai_update` for every AI every tick, before `match_tick` (the world is then the previous tick's result: this is the "post-tick" operation, and the commands land in the very next tick).
- An AI decides every `AI_DECISION_PERIOD_TICKS` ticks, on `tick % period == player % period`: work per tick is about players / period decisions, whatever the player count.
- Inside a decision layers run on sub-cadences counted by `ai_player_state::decision_index` (zero-init friendly): reactive (campaign orders) every decision, threat / defense every 2nd, tactics (taxes, raising, buildings, founding, roads) every 3rd on a round-robin window of a few settlements, strategy (opinions, target, mode) every 5th.
- Per-decision budgets: owned entities looked at are capped (`AI_MAX_OWNED_*`), site search is `AI_SITE_SAMPLES` tries, orders are capped, and the command sequence is limited to the room left in the tick commands (a full budget just ends the decision).

## Design

- **State machine** (`ai_player_state::mode`): EXPAND -> BUILD_UP -> ATTACK -> BUILD_UP ... see *match_ai_strategy.cpp* header for the transitions.
- **Opinions** (`i16` per other player, negative = hostile): one update function in *match_ai_opinion.cpp*. The most hostile player in campaign range is the target player; its weakest nearby settlement is the target, our closest settlement the rally point.
- **Layers**: strategy (mode, target) -> tactics (economy: taxes / buildings / troop ceilings; expansion: founding, roads; military: raising, defense, campaign).

## Decisions

- No peace period: AIs may attack from day one. The only brakes are the expansion goal (EXPAND first), the attack need of the target, and 15 days of BUILD_UP between campaigns.

## Known gaps

- AIs end in permanent war with a lot of settlement hand-over; isolated AIs hoard treasury.
- No reachability check when founding (unreachable site strands the colonists); no retreat from hopeless fights.
- Late armies at the rally wait for a new gather instead of joining a siege; the target's nearby armies are ignored.
- Troop types are treated as plain numbers; levies cost peasants, so war cuts output.
- Testing: *.ai_imprint/* has a working scratch program and notes (map_gen is not on this branch).

## Layout

- *include/game_common/match/ai/match_ai.h*: public header (`ai_player_state`, modes, `ai_update`).
- *match_ai_internal.h*: constants, views, context, declarations. *match_ai_main.cpp*: cadence, scratch sequence, validated push, layer order (unity entry).
- *match_ai_world.cpp*, *_opinion.cpp*, *_strategy.cpp*, *_tactics_economy.cpp*, *_tactics_expansion.cpp*, *_tactics_military.cpp*.

## Adding a behavior

Write a small `ai_tactics_xxx(ai_context&)` in the right tactics file (read through accessors, push with `ai_push<payload>(ctx, TYPE)`, return when it gives nullptr), declare it in the internal header, call it from `ai_decide` in the main file. Need a new world read? Add an accessor to *match_ai_world.cpp*.
