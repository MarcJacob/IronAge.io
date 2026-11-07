# Army supply redesign + campaign AI supply intelligence

Started: 2026-11-07 (planned, not begun). Design: `management/design/military_design.md`, "Supply system". Current code: R3 in `management/work_units/alpha_war_extensions_2026-10-18.md`. Supersedes `army_supplies_2026-10-23.md` open tasks.

## Scope

1. Redesign how armies are supplied (gold reserve problem).
2. Make the campaign AI smart about supply and supply lines, especially before crossing water.
3. Shared campaign supply pool (P5), on top of the above.

## Problem

- Too easy for an army to walk off owned territory, run out of gold and stop getting resupplied (pay only works in friendly territory; caravans to armies need the army to hold gold).
- Campaign AI does not plan around supply or supply lines, notably before crossing water.

## Direction (user)

- Caravans sent to an army are paid for by the owner's treasury as they leave the settlement, not by the army's gold.
- The army's own gold is for: attracting caravans from non-hostile non-owned settlements, buying from the settlement it stands in, and dropping off at home.

## Tasks

- A. Design the new supply rules (pricing, who profits, daily pay, interaction with carry-capacity clamp). Not started.
- B. Implement A in the sim (+ client UI where supply is shown).
- C. Campaign AI supply logic: stock up before departure, keep supply lines open, preparation before water crossings (options: wait for full supplies, extra wagons, minimum days of supply by crossing length, refuse to cross when the line cannot follow). User to pick.
- D. Shared campaign supply pool (P5 decisions in `alpha_war_extensions_2026-10-18.md`).

## Open

- Pricing, profit split for treasury-paid caravans.
- Does daily pay in owned territory stay?
- Water crossing behavior (C).
- Sim `campaign_replace_lost_armies` re-raise gold sink.

## Next step

User designs A, then a builder implements B. C and D follow.
