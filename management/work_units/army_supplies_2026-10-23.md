# Army supplies (upkeep as flow)

Started: 2026-10-23. Replaces the treasury -> void army upkeep (see `alpha_troops_recruitment_2026-10-18.md`). Mechanics: `game_mechanics.imprint.md`; notes: `src/game_common/world_sim/.ai_imprint/army_upkeep_and_treasury.md`.

## Scope

- Consumption is the only sink in the economy; trade profit and production are the sources.
- Armies carry a wealth stock ("supplies") filled from the treasury, consumed daily by the soldiers.
- Shortage causes desertion. Treasury never goes negative (no debt).

## Decisions

- Daily consumption = sum(soldiers x per-type rate); rates are the `upkeep_*` constants. Linear: no superlinear scaling, no recruitment gold cost.
- Refill: inside territory the owner owns only; `army_supply_refill_days` (2) days of consumption per day, up to capacity `army_supply_capacity_days` (10) days. Capacity of a raising army uses its raise targets.
- Shortage: unfunded share of consumption = desertion. Deserters return to the home settlement (`world_army_desertion`), vanish if it is gone. A raising army that runs short stops raising. An army that loses its last man is destroyed.
- Raise cost: fixed one day of the targets' consumption, charged at raise (no reserve slider). Error `COMMAND_ERROR_NOT_ENOUGH_TREASURY`.
- Pool soldiers: still consume locally; paid from the treasury as "soldier pay" into settlement income (separate from "worker pay"). Settlement may shoulder part of the burden later.
- Soldiers in armies do not consume at home.
- Supplies visible for all armies (no limited-information mode).
- Armies outside own borders are not supplied; ways to pay them abroad are designed later.
- Homecoming (return home / disband): stock split treasury / home settlement by a parameter (50/50 placeholder).
- Army-to-army looting: victor takes (part of) a defeated army's stock.
- Water: embark cost from the army's supplies + consumption multiplier at sea (details deferred; cannot embark if unaffordable).
- Settlement looting deferred: needs a lootable value formula (trade pools, population, buildings); may later drive a settlement's trade value.
- AI: only skips raises it cannot pay. Rework deferred (backlog `ai_tuning`).

## Tasks

1. [DONE, unbuilt] Supplies stock, consumption, refill, desertion, soldier pay, raise cost; old upkeep sink removed.
2. [DONE, unbuilt] UI: supplies gauge (red/yellow/green), consumption, days left, low-supplies warning, desertion row, raise "Cost:" row, troop gauges show losses.
3. Homecoming transfer, army-to-army looting.
4. Embark cost.

## Open questions

- A raising army whose home cannot supply men can stay raising forever.
- Siege defender armies spawn with 0 supplies and desert at the next daily tick if the treasury is empty.
- Possible additions: lifetime loss count per army; distance-based chance for deserters to return.
- `trade_caravans_2026-10-22.md` has pre-reorganization paths (stale).

## Next step

Tasks 1-2 tested by user. Task 3 and 4 when wanted.
