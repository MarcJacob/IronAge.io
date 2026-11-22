# Campaign objectives + goals

Started: 2026-11-09. Campaign AI (P5 of the war extensions).

Multi-file architecture change (sim campaign logic, commands, client UI, AI).

## Scope

Campaigns become decision-making machines: they rank the objectives available to them and run the best one. Objectives live on a shared world-side board; a campaign's goals (player / AI assigned) bias that choice.

## Terminology

- Objective: something that exists in the world (on the board) and that campaigns can pursue. Former "candidate action".
- Goal: what is assigned to a campaign (max 8, constant `CAMPAIGN_MAX_GOALS`). Former "campaign objective".

## Design

- Objective board: shared by all campaigns, not visible to players (not a micro entity in the player-facing sense, no render / query plumbing).
- Objective types: conquer territory (settlement), defend territory, raid territory, engage enemy army (outside any campaign), engage enemy campaign, resupply (settlement / current location). More can be added later.
- Each type is a module: generator (when it appears), scorer (per campaign), executor (logic, a parameterized state in a state machine), validity / finished check.
- Spontaneous generation, from campaigns' surroundings: every settlement hostile to a campaign in the area generates a conquest objective; every friendly settlement a defense objective (threat to a territory raises defend weight, higher for relieving a siege).
- One objective per type + target (dedup). Objectives shared across campaigns; another friendly campaign already on it lowers the score for a new one. No claims / cooperation logic for now.
- Goal types: conquer territory, defend territory, destroy a specific enemy campaign. A goal forces its objective to exist (any distance) and massively favors it in that campaign's ranking. Goals can be added / removed at any time, including after the campaign begins. Ranking is fully automatic.
- Each campaign picks one current objective (best score). The current one gets a stickiness bonus (no flapping, esp. committed ones like a siege) but the campaign may switch out before it is done.
- Raiding is a tool the campaign uses on its own to replenish supplies / get some gold, via an independent weight.
- Resupply objectives + preconditions / score penalties on march objectives cover stocking up before departure, keeping the supply line open, and water-crossing / long-trip planning.
- Scores are integer, computed in the deterministic sim, re-evaluated on an interval. Objective generation / despawn runs on the same interval in index order, with hysteresis on despawn.
- Despawn: target invalid, objective accomplished, or no campaign has it in range and no goal forces it.
- "Destroy campaign" ends when the target campaign is gone for any reason; target location = its current position (moving target, re-path). Valid only against hostile or neutral campaigns, never own / friendly.
- Campaign ends (armies sent home) when its goal list becomes empty after having had one. Conquer goals are removed when accomplished; defend goals never auto-remove (explicit remove only). A campaign that has not started waits / gathers.
- Campaign-level states WAITING / GATHERING stay, then ACTIVE; MARCHING / ON_OBJECTIVE become the conquer objective's sub-state.
- `SET_CAMPAIGN_OBJECTIVE` is replaced by add / remove goal commands.
- Order of work: objectives first, goals later. Campaigns ignore user-set goals and choose objectives neutrally until the core objective types work.

## Tasks

- 1. [DONE, hand-tested] Objective board, type table (range / score / logic + spawn / despawn per type), ranking loop (WAIT + objectives, stickiness, branch-and-bound path culling). Types: CONQUER_SETTLEMENT, TRAVEL_TO_SETTLEMENT (scores progress toward the campaign's legacy goal; no goal = never travels). `CAMPAIGN_OBJECTIVE_*` renamed `CAMPAIGN_GOAL_*`. `CAMPAIGN_BEGIN` is a no-op.
- 2. [DONE, hand-tested] Defend = a conquer objective whose target is friendly to the campaign (no separate type), decided at score / run time via `campaign_ai_is_friendly` (owner only for now). Executor gives members DEFEND_TERRITORY once. Trip cost = path cost. A campaign never travels where its supplies could not last even when full.
- 3. Engage enemy army objective (army outside any campaign).
- 4. Engage enemy campaign objective.
- 5. Goals: goal storage (8), add / remove goal commands replacing SET, force + bias in ranking, client / AI adaptation, `campaign_start`, panel.
- 6. Situational objectives: resupply, raid for supplies.
- 7. Client goal editing UI; AI player rework on goals (stuck rule vs never-ending defend goals).

## Next step

Task 3 or task 5 (goals / AI player rework): you pick.
