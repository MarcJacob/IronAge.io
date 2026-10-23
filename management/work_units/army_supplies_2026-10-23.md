# Army supplies (upkeep as flow)

Started: 2026-10-23. Replaces the treasury -> void army upkeep (see `alpha_troops_recruitment_2026-10-18.md`). Survey: `src/game_common/world_sim/.ai_imprint/army_upkeep_and_treasury.md`.

## Scope

- Consumption is the only sink in the economy; trade profit and production are the sources.
- Armies carry a wealth stock ("supplies") filled from the treasury, consumed daily by the soldiers.
- Shortage causes desertion. Treasury never goes negative (no debt).

## Decisions

- Daily consumption = sum(soldiers x per-type rate); rates reuse the `upkeep_*` constants. Linear: no superlinear scaling, no recruitment gold cost (both dropped for now).
- Refill: daily, only while the army is inside territory its owner owns; pulls min(target - stock, treasury).
- Supply capacity: N days of the army's own consumption (parameter, ~10). Specialized capacity (troop types) maybe later.
- Shortage: unfunded fraction of the day's consumption becomes desertion; deserters vanish.
- Homecoming (army returns home / disbands): stock split treasury / home settlement by a parameter (50/50 placeholder).
- Army-to-army looting: victor takes (part of) a defeated army's stock.
- Water: embark cost paid from the army's supplies plus a consumption multiplier at sea (details deferred; an army that cannot afford embarking cannot embark).
- Armies outside own borders are not supplied; ways to pay them abroad are designed later, after the basic mechanics.
- Settlement looting deferred: needs a lootable value formula (trade pools, population, buildings); may later also drive a settlement's trade value.
- Pool soldiers (in a settlement) consume locally as now, and are paid from the treasury as wages into the settlement's pending income (same flow as worker wages). Settlement may shoulder part of the burden later.
- Soldiers in armies (raising or raised) do not consume at home; they are supplied by the army.
- Initial reserve: slider in the raise UI (bounded by the current treasury); amount is taken from the treasury immediately when the raise command applies and put into the army. Supply system is active from the moment the army exists (including while raising).
- No pay-control toggle / refill-target slider for now.
- AI ignored for now (see backlog `ai_tuning`).

## Tasks

1. Supplies stock, daily consumption, refill, desertion on shortage; remove old upkeep sink.
2. UI: supplies gauge + days remaining on the army, consumption figure, shortage warning.
3. Homecoming transfer, army-to-army looting.
4. Embark cost.

## Open questions

- Capacity of a raising army: based on its raise targets (proposed), current composition once raised.

## Next step

Task 1 with a peer programmer (brief below). Reserve slider UI goes in task 2; task 1 adds the command field + backend.ts wire mirror.

## Peer brief

Goal: task 1, sim side only (no UI). Consumption becomes the only sink; army upkeep stops being a treasury -> void charge.

Imprints: chain for `src/game_common/`, `src/game_common/world_sim/`, `src/game_common/world_sim/armies/` if present. Related: `src/game_common/match_commands.cpp`, `front/src` backend wire code (`backend.ts`). Notes: `src/game_common/world_sim/.ai_imprint/army_upkeep_and_treasury.md`, `settlement_economy_model.md`. Docs: this file, `economy_revamp_2026-10-22.md`, `design_doc.md` ("Upkeep & Economy").

Done criteria:
1. Each army has a supplies stock. Daily tick: consumes sum(soldiers x per-type rate) (reuse `upkeep_*` rates); if inside territory owned by its owner, refills min(capacity - stock, treasury) from the treasury; unfunded fraction of consumption = desertion (each troop type loses that fraction, deserters vanish). Capacity = N days of consumption (new game parameter, default 10); raising army uses its raise targets; stock above capacity not clamped. Active from the moment the army exists, including while raising.
2. Remove the old `world_system_armies_upkeep` treasury charge.
3. Pool soldiers: daily wages from the treasury into the settlement's pending wages (like worker wages), same per-type rates, paid min(cost, treasury). They still consume locally; soldiers in armies do not consume at home. No pool desertion in v1.
4. `RAISE_ARMY` carries an initial reserve (gold). Valid if <= treasury; taken from the treasury immediately on apply into the new army's stock; 0 is valid. Use the `raise_army_can_afford` / `raise_army_charge` stubs if they fit.
5. Treasury and army stock never go negative.
6. Expose army supplies and consumption/day in the GDO (follow conventions).
7. Mirror the changed packed command struct in `backend.ts` in the same turn; client sends reserve 0 for now.
8. Update `army_upkeep_and_treasury.md` in `.ai_imprint`.

Decisions already made: see Decisions above. Not in scope: UI, homecoming, looting, embark cost, AI.
Open questions: none. Peer must not make design decisions; ask the user. Builds: not run unless the user says so.
