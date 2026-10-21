# Alpha: war economy (troops, recruitment, upkeep)

Scope widened 2026-10-20: covers the war economy broadly (recruitment, upkeep, raising / disbanding mechanics), on top of the economy from `basic_world_economy`. File name kept.

Started: 2026-10-18. Design: `management/design_doc.md`, Alpha > War & conquest > Troop recruitment & Raising, Upkeep & Economy.

## Scope

Troop types (four: levies, men-at-arms, horsemen, knights), raising armies next to a settlement (immobile while raising), local troops pool per settlement with ceilings.
Deferred: barracks / keep pools, upkeep, desertion / disbanding when out of gold, gold costs, disband button, cancel.

## Decisions

- Four troop types. Remove `archers` (comp, battle `UNIT_TYPES`, GDOs). All `comp` ints become ui16.
- No economy yet: recruitment is free. Leave room for cost / upkeep hooks (static cost fn called from validity + apply).
- No buildings yet: local troop max = % of settlement population, rounded down. Men-at-arms 2%, horsemen 1%, knights 0.5%.
- Pool above max is kept when population drops; troops are lost only if the player manually lowers the ceiling below the count (UI must handle that case).
- Raising: `RAISE_ARMY` extended with target counts per type. Army walks to `raise_location`, stays there raising, refuses move orders until done. Second command edits targets of a raising army. Third command stops raising (ready immediately). No cancel.
- Raising is suspended while the army is in a battle, resumes after if it survives. Raising armies stay targetable.
- UI: new "Entity properties panel" at the center bottom, general info about the selection.
  - Settlement selected: gauges current / max local troops per type, one color per type.
  - ArmyRaising mode: panel shows per type gauges current / target / max, target is a mouse slider; max = home settlement's.
  - ArmySelected mode: same controls when the army is inside its home settlement territory.
  - Both modes share UI element code.

- Pool: per type, grows +1 every 10 ticks (staggered per settlement) toward effective ceiling, free.
- Ceiling is player-set per type, stored raw (may exceed max, never clamped in storage). Effective ceiling = min(ceiling, max). UI shows the effective value. New settlements: ceiling = max (raw default large / saturated).
- Lowering the ceiling below the current pool count removes the excess troops (UI warns). Set-ceiling command exists in task 1; UI in task 3.
- Raising rates: men-at-arms / horsemen / knights pool -> army a few men per tick; levies 1 man per tick from population.
- Levy max = population - 10 (settlement cannot be depopulated by raising). No 100-population minimum anymore.

- Builders were allowed to run the full build (scripts\win32_build_all.cmd) in the user's absence; nothing was run or hand-tested.
- Worker decisions: raising army spawns empty and walks to `raise_location` first; pool draw 3/tick per pooled type, levies 1/tick while pop > 10; `raising` flag explicit; move / attack / found-settlement orders rejected while raising (`ARMY_RAISING`); `NO_RAISE_TARGET` when all targets zero; empty raising army spared by dead-army check unless damaged; pool slider at max sends raw 0xFFFF.
- ArmySelected: gauges always shown, sliders and "Stop raising" only while raising (no territory check). Idle armies: max = max(raise_targets, current) = original counts.
- Final changes: army spawns at `raise_location` (no walk); siege defenders = 20% of population as levies + whole local pool, returned on disband; `GROWTH_TICK_PERIOD` = 50 (pop + troop pool growth); battle round interval 20 ticks.

## Tasks

1. [DONE] Settlement troop pool, ceilings, growth tick, set-ceiling command, archers removal, ui16 (commits 1ebab83, 74fb766).
2. [DONE] Raising state, extended RAISE_ARMY, edit-targets + stop-raising commands (fcd8451, a234343).
3. [DONE] Entity properties panel, gauges, sliders, mode wiring (builder, untested in browser).

## Open questions

- Not yet hand-tested at all (see checklists in worker reports: raise, refuse orders, pool wait, battle suspend, ceiling lowering).

## Depends on (later parts)

`basic_world_economy_2026-10-18.md`.

## Next step

User hand-test and review. Remaining unit scope (disband button, upkeep, building-gated pools, gold costs, troop type stats / matchups, cavalry speed) waits on other Alpha units.
