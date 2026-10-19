# Basic army battle system

Started: 2026-10-15. Branch `feature/ArmyBattleSystem`, autonomous run (user unavailable; decisions made by manager).

## Goal

Armies catching armies create a Battle entity (static layout, no army list). Armies join by targeting it, are pinned until it ends. Simplest possible code.

## Spec

- Battle: per-side per-type man counts, duration, this tick's damage per side. Fixed size.
- Two clashing armies join immediately. Join adds men to side totals.
- Per tick: random damage each way; each army applies its share (army men / side men).
- Armies pinned while in battle. To the death; losers wiped out.
- Army movement speed lowered to ~1/3.
- Bonus: army attacks settlement -> defending levy army (10% pop) + siege battle; attackers win -> settlement taken.

## Decisions

- Battles cannot be targeted. Army reaching an army joins the side against it. Siege: targeting settlement or defending army joins attackers.
- `world_disband_army` returns men to a settlement; used when defenders win a siege (only the `siege_defenders` levy army).
- Selecting a battle shows attacker / defender men; balance bar via `draw_battle_balance` (render.ts, replaceable).
- Damage 1-10% of side men per tick, rounded up. Battle spawns at target location. Army speed 12 -> 4.
- Pinned armies cannot leave.
- New imprint: src/game_common/world_sim/world_sim.imprint.md (no existing imprint modified).

## Tasks

- [DONE] Core: Battle entity, join, tick damage, pinning, speed change (builder-verified by headless harness, not in real client)
- [DONE] Imprint in world_sim folder
- [DONE] Bonus: siege (flag = `siege_settlement` guid in battle state; verified win/loss in headless harness; full build passes)
- [DONE] Commits

## Open

None. Battle UI hand-tested OK (2026-10-16).

## Phase 2 (2026-10-19, unattended run; commit only, never push)

Phases (sequential commits by manager):
- A [client UI]: generic entity inspection panel (troops + settlement macro stats), new "advanced action panel" above action bar, army raising from settlement uses it; fix raising mode text.
- B [sim]: raising rework: N raised armies per settlement (number, not bool); army still counts in settlement pop (track raised %); spawns inside settlement, moves to target when done raising; right click retargets; origin kept. Client: path preview green (move) / red (attack target) for selected army.
- C [sim+UI]: Warband (current army) / Army (up to 64 warbands by entity index, same owner, join within 5 tiles, merge, detach, accompany friendly). Army panel: combined bar + warband list.
- D [sim+UI]: Siege entity (-5% pop/troops per growth tick), join/attack besiegers, Assault (defense x2) / Lift / detach; defender Sally (no bonus); defending-army spawn in own function. Disband action (inside home territory).
- E: svg icons crossed swords (battle), catapult (siege).

### Architecture gaps noted (for user)

(builders append here, terse)

- A: *front_src.imprint.md* lists the UI elements (hint, action bar) and not the properties panel nor the new advanced action panel / *entity_panels.ts*: imprint needs a user update.
- A: the raise mode still previews a path from the settlement to the hovered tile, though the army spawns at the tile (no walk today). Kept as is; it becomes accurate with phase B (spawn inside, walk out).
- A: *ui_element_advanced_action_panel.ts* imports the renderer (`PanelView`) from *ui_element_properties_panel.ts* (element -> element import). Could move to a shared *ui/panel_view.ts* if more panels come.
- B: raised men are claims on the home settlement, not removed. Population shrinking (growth) or a lowered troop ceiling can leave the settlement holding fewer men than its army claims; removals saturate (population never below 1, pool at 0). No reconciliation.
- B: pool draws only compare against the raising army's own counts (`pool - army count`), correct only while `SETTLEMENT_MAX_RAISED_ARMIES` is 1. More armies per settlement must subtract every raised army's claims.
- B: siege levies (20% of population) are taken from a population that still counts the settlement's raised men (they fight twice). To revisit in phase D.
- B: an army whose home settlement is lost (destroyed / captured) keeps its men, now counted nowhere; its later losses / settling take nothing from anyone.
- B: a raising army pulled into a battle loses its stored destination (battle_join stops it); after the battle it raises in place, wherever it stands.
- B: raised armies are derived (scan of all armies by home settlement), not stored: O(armies) per raise validity check and per settlement query.

### Phase A notes

- Panel content is one `PanelState` (title, stat rows, optional gauges heading, gauges); properties panel and advanced action panel both render it with `PanelView`.
- Per-entity-type states in *ui/entity_panels.ts* (`settlement_panel`, `army_panel`). Settlement stats: population, wealth, land output, tier (what the GDO has). Army stats: men, state (in battle / raising / ready).
- The settlement panel now stays visible while raising (the raise controls moved to the advanced action panel).
- Raise mode shows no validity text in the panel (feedback still only colors the path preview), to keep scope.

### Phase B notes

- Model: a raised army's men stay counted in its home settlement's population (levies) and local pool (other types); the army holds a claim. Rule: every man the army loses (battle) or takes away (settling) is removed from the home settlement (`world_army_remove_men_from_home`), while the home is valid and has the army's owner.
- Raised armies / men are derived, not stored (`world_settlement_raised`). `SETTLEMENT_MAX_RAISED_ARMIES = 1` constant (*world_sim_population.h*). Settlement GDO: `raised_armies`, `max_raised_armies`, `raised_men`, `raised_percent` (men of every type / population, rounded down).
- Raise limit reuses `COMMAND_ERROR_ENTITY_LIMIT_REACHED` (no new error flag, no TS mirror change).
- Army spawns at the settlement with `movement.destination` = clicked tile; the movement system skips raising armies (except to walk to a battle). Move and attack orders are now accepted while raising (stored, acted on once raised); settling still refused while raising, now with `COMMAND_ERROR_ARMY_RAISING`. Raise location still has to be in the settlement's territory.
- Levies are claimed while population > army levies + `RAISE_MIN_POPULATION_LEFT`; pooled types while pool > army count.
- Client: selected army previews the path to its current destination, green for a move, red while it has an attack target (optional `color` on `RENDER_CONFIG.path_preview`). Raised rows (`settlement_raised_rows`) in the settlement panel and the raise panel.

### Status

(manager updates: phase -> state)
