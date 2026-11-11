# Campaign objectives + goals

Started: 2026-11-09. Part of P5 of `alpha_war_extensions_2026-10-18.md` (campaign AI). Related: `army_supply_redesign_2026-11-07.md` (C is absorbed here, D stays there), `match_ai_2026-10-22.md`, `army_autonomy_2026-11-08.md`.

Multi-file architecture change (sim campaign logic, commands, client UI, AI).

## Scope

Campaigns become decision-making machines: they rank the objectives available to them and run the best one. Objectives live on a shared world-side board; a campaign's goals (player / AI assigned) bias that choice.

## Terminology

- Objective: something that exists in the world (on the board) and that campaigns can pursue. Former "candidate action".
- Goal: what is assigned to a campaign (max 8, constant `CAMPAIGN_MAX_GOALS`). Former "campaign objective".

## Design (user)

- Objective board: shared by all campaigns, not visible to players (not a micro entity, no GDO / render / query plumbing; sim-side pool).
- Objective types: conquer territory (settlement), defend territory, raid territory, engage enemy army (outside any campaign), engage enemy campaign, resupply (settlement / current location). More can be added later.
- Each type is a module: generator (when it appears), scorer (per campaign), executor (logic, a parameterized state in a state machine), validity / finished check.
- Spontaneous generation, from campaigns' surroundings: every settlement hostile to a campaign in the area generates a conquest objective; every friendly settlement a defense objective; others to come up with (threat to a territory raises defend weight, higher for relieving a siege).
- One objective per type + target (dedup). Objectives shared across campaigns; another friendly campaign already on it lowers the score for a new one. No claims / cooperation logic for now.
- Goal types: conquer territory, defend territory, destroy a specific enemy campaign. A goal forces its objective to exist (any distance) and massively favors it in that campaign's ranking. Goals can be added / removed at any time, including after the campaign begins. Ranking is fully automatic (player priority system maybe later).
- Each campaign picks one current objective (best score). The current one gets a stickiness bonus (no flapping, esp. committed ones like a siege) but the campaign may switch out before it is done. Later: dispatch armies to several objectives at once (brings back a leader army).
- Raiding is a tool the campaign uses on its own to replenish supplies / get some gold, via an independent weight.
- Absorbs supply task C: stock up before departure, keep the supply line open, water-crossing / long-trip planning = resupply objectives + preconditions / score penalties on march objectives.
- Scores are integer, computed in the deterministic sim, re-evaluated on an interval. Objective generation / despawn runs on the same interval in index order, with hysteresis on despawn.
- Despawn: target invalid, objective accomplished, or no campaign has it in range and no goal forces it.
- "Destroy campaign" ends when the target campaign is gone for any reason; target location = its current position (moving target, re-path). Valid only against hostile or neutral campaigns, never own / friendly.
- Campaign ends (armies sent home) when its goal list becomes empty after having had one. Conquer goals are removed when accomplished; defend goals never auto-remove (explicit remove only). A campaign that has not started waits / gathers.
- Campaign-level states WAITING / GATHERING stay, then ACTIVE; MARCHING / ON_OBJECTIVE become the conquer objective's sub-state.
- `SET_CAMPAIGN_OBJECTIVE` is replaced by add / remove goal commands.

## Tasks

- S. [DONE] Read-only survey (findings below).
Order (user): objectives first, goals later. Experiment: campaigns ignore user-set goals and choose objectives neutrally; goals come after the core objective types work.

- 1a. [DONE, pending hand test] Objective board (micro entities, `world_entity_campaign_objectives.h`, `world_sim_entity_campaign_objective.*`), type table in `world_system_campaign_objectives.cpp` (range / score / logic + spawn / despawn per type), ranking loop (WAIT + objectives, stickiness, branch-and-bound path culling), `world_system_campaigns.cpp` rewritten. Types: CONQUER_SETTLEMENT, TRAVEL_TO_SETTLEMENT (scores progress toward the campaign's legacy goal; no goal = never travels). Campaign ends on disband / no members only. Params `campaign_ai_parameters` in `game_parameters.h`, all placeholders. Legacy `CAMPAIGN_OBJECTIVE_*` renamed `CAMPAIGN_GOAL_*` (field / GDO names unchanged). `CAMPAIGN_BEGIN` is a no-op, Begin button removed.
- 2. [DONE, hand-tested] Defend = a conquer objective whose target is friendly to the campaign (no separate type / role system), decided at score / run time via `campaign_ai_is_friendly` (owner only; allies / vassals / overlords go there later). Per-objective conquer data (attacker / defender campaign + troop counts) refreshed every tick; `committed` stays the generic count for despawn. Friendly score: INVALID unless attackers > 0, else `defend_base` + per-attacker-campaign + `defend_siege_relief_bonus`, minus trip penalty; "others" penalty counts same-side only. Executor gives members DEFEND_TERRITORY once. Placeholders: defend_base 1200, defend_per_attacker_campaign 300, defend_siege_relief_bonus 800. Dropped for now: "nearby enemy campaign" scoring, strength-ratio scoring (troop totals tracked, unused).
  - Also done: travel scores goal progress (two-pass ranking removed); trip cost = path cost with scan radius first, branch-and-bound ranking (exact); a campaign never travels where its supplies could not last even when full; WAIT comfort clamped to carry capacity. Balance params changed in `game_parameters.h` (upkeep_men_at_arms 3→2, upkeep15→6, tradepost_wagon_max_increase 2→6, all carry_* doubled): affects army_raising_cost, supply purchase cost, caravan load. Any wagon still halves army speed (stopgap).
- 3. Engage enemy army objective (army outside any campaign). To do.
- 4. Engage enemy campaign objective. To do.
- 5. Goals: goal storage (8), add / remove goal commands replacing SET, force + bias in ranking, client / AI adaptation, `campaign_start`, panel. To do.
- 6. Situational objectives: resupply, raid for supplies. To do.
- 7. Client goal editing UI / board debug view maybe; AI player rework on goals (stuck rule vs never-ending defend goals). To do.

## Survey findings

- Data: `world_entity_campaigns.h`, micro entity (parallel arrays, 64 per region). One 16-byte `objective` (CONQUER_SETTLEMENT only; `radius` / `state` / `location` unused), states WAITING / GATHERING / MARCHING / ON_OBJECTIVE, `state_data.marching.guide_army`. No member list (armies point at campaign). GDO exposes objective scalars only; an array of 8 needs flat parallel properties.
- Sim: `world_system_campaigns.cpp`. Generic: joiner takeover, centroid / count, end-when-empty, lost-army replacement, march + guide / accompany, end-and-send-home. Objective-specific: `CAMPAIGN_OBJECTIVE_DEFS` table (target location, accomplished, on-objective logic incl. assault decision).
- Commands: `CAMPAIGN_JOIN` (founds if no campaign), `SET_CAMPAIGN_OBJECTIVE` (replaces wholesale), `CAMPAIGN_BEGIN`, `CAMPAIGN_DISBAND` in `commands.h` / `match_commands.cpp`.
- Client: panel is read-only, objective shown as one text; `campaign_start.ts` sends one objective; no objective editing UI.
- AI hard-wires the single-objective flow; `ai_campaign_is_stuck` (80 days) conflicts with never-ending defend goals.
- Reusable for objectives: army orders (STATION, ATTACK, DEFEND_TERRITORY, RAID, GO_HOME, ACCOMPANY), engagement / siege API, raid looting, direct purchase + caravans to members. Not reusable yet: hostile-army / caravan scans are static and army-centric in `world_system_army_standing_orders.cpp`; no campaign-level supply aggregate; no territory threat query.
- Quirks: `CAMPAIGN_BEGIN` relies on the MARCHING case re-picking the guide; `state_data` not copied by `single::copy_from`; empty-campaign destroy sends no GO_HOME; stale file-name comments (`world_system_standing_orders.cpp`).

## Open

- Whether `world_entity_is_hostile` handles campaigns is unverified (needed for destroy-campaign / engage-campaign).
- `military_design.md` Campaign section lists Raid regions as an objective; here raid is an objective type generated on the board (user to align the doc).
- Behavior of a started campaign with no goals: acts on spontaneous objectives above a minimum score (param).
- Score formulas, interval, spawn radius, switch margin, stickiness, goal bias values (design with the user per task).

- Pathing perf rework (separate task): one bounded search from the campaign to all objectives instead of per-candidate searches.
- Tune placeholder weights (travel_progress_weight, penalties after path-cost switch); maybe a tighter lower bound (plains cost).
- AI still sets goals + sends CAMPAIGN_BEGIN; 80-day stuck rule behaves differently now.
- `backend.js` stale comment (built output, regenerates).

## Next step

Task 3 (engage enemy army outside any campaign), or task 5 (goals) / AI player rework: user observed remaining supply / behavior issues are mostly the player AI ignoring goals. User picks.
