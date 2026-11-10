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

- 1a. Objective board + type-module interface + ranking loop; conquer settlement ported as the first objective type (spawned by the generator, large spawn radius). Goals ignored: `SET_CAMPAIGN_OBJECTIVE` / the campaign `objective` field stay as they are but are unused by ranking. Campaign no longer ends on accomplishment (ends on disband / no members only). To do.
- 2. Defend objective (friendly settlement, against an incoming / present force; higher weight for relieving a siege). To do.
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

## Next step

User approves the 1a builder prompt, then dispatch.
