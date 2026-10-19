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

### Phase A notes

- Panel content is one `PanelState` (title, stat rows, optional gauges heading, gauges); properties panel and advanced action panel both render it with `PanelView`.
- Per-entity-type states in *ui/entity_panels.ts* (`settlement_panel`, `army_panel`). Settlement stats: population, wealth, land output, tier (what the GDO has). Army stats: men, state (in battle / raising / ready).
- The settlement panel now stays visible while raising (the raise controls moved to the advanced action panel).
- Raise mode shows no validity text in the panel (feedback still only colors the path preview), to keep scope.

### Status

(manager updates: phase -> state)
