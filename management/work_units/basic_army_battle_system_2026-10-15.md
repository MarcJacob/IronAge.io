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

- Not tried in the real client / server; battle UI panel and balance bar built (tsc) but never run in a browser.

## Next

User review.
