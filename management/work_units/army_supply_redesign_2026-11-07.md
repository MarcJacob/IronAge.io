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

## Decisions (2026-11-08, supersede Direction above where they differ)

- Non-retinue armies are valid trade targets for settlement caravan scans (mostly already so).
- Remove the army gold check. Armies never spend carried gold; it is only loot to bring home.
- Caravans to armies are paid immediately from the owner's treasury at dispatch.
- Armies track total incoming supply (in-flight caravans); settlements send exactly enough caravans to cover the need.
- Attrition / desertion redesign comes after the above works: gradual as supplies run low, capped at a max percentage (replaces "everyone leaves at zero").

- Payment counts as trade volume for the dispatching settlement. No owner cut / trade tax for now.
- Daily pay in owned territory is removed. Army upkeep feel (e.g. gold sink for retinues) is a later design.

- Incoming supply is a stored field on the army (no caravan-list scans).
- Caravans paid at dispatch never retarget to armies; a caravan that loses its army target (or finds no room) goes home. Any successful delivery destroys the caravan (no remainder).
- Dispatch payment and retinue purchase are a pure gold sink: owner treasury pays (expense only), no income to any settlement owner, settlement only gains trade volume.
- Supersedes "non-retinue armies are trade targets": an army is a caravan target only if it is part of a campaign (retinue or not). Caravan resupply is a campaign feature; the campaign coordinates the supply line.
- ALL armies (not just retinues) buy directly from the settlement they are stationed in (treasury-paid sink, as above), avoiding the caravan intermediate.

## Tasks

- A. Design the new supply rules: pricing, who profits, daily pay, carry-capacity clamp. Partly decided (see Decisions).
- B1. Sim: gold check removed, treasury-paid caravans, incoming-supply tracking on armies, exact-need dispatch (+ client UI where supply is shown).
- B1 [built, awaiting hand-test]: gold check + daily pay removed, treasury-paid dispatch, stored `incoming_supplies`, retinue direct purchase (same-owner settlement only), stale docs fixed. Fixed after first hand-test: caravans can spawn for armies on the origin tile (empty path was treated as unreachable), payment is now a sink. Last full build not completed (server exe locked): rebuild needed. `military_design.md` not yet updated.
- B1c. Settlements track stored incoming goods from in-flight caravans (like army `incoming_supplies`); counted at 50% in scarcity, in both the target weighting and the profit calculation, to stop many settlements over-sending at once. Built (full build OK), awaiting hand-test. Param `trade.incoming_goods_scarcity_percent` = 50 (0 = old behavior). Delivery profit also uses the pool with incoming (excluding the delivering caravan). Spawn size cap (`caravan_candidate_size`) still uses raw market total: open. Retinue purchase bug resolved (hand-tested working).
- B1d. Rule change: caravan target = campaign member; direct purchase for all stationed armies. Built (DLL compiled; exe link failed, server was running: rebuild needed), awaiting hand-test. "Campaign member" = `armies.campaign` holds a valid guid. Purchase keeps same-owner settlement + exact tile condition. Non-campaign armies away from settlements get no supply (intended; see `army_autonomy_2026-11-08.md`).
- B1b. Fix redirected caravans (e.g. army moved / gone): they must not sell goods to a foreign settlement when their origin's trade rules disallow it.
- B1e. TEMPORARY: armies joining a campaign are also caravan targets. Desertion when out of supplies = 10% of the army per day (named parameter `army_low_supply_desertion_percent`; was: everyone leaves). Built (DLL compiled; exe link failed, server running: rebuild needed), awaiting hand-test. "Joining" = order JOIN_CAMPAIGN with a live target. Desertion decision: 10% x (unfunded / consumption) of the army's MAX count (raised-with count, per troop type), full 10% only at zero supplies; built (DLL compiled; exe link failed, rebuild needed), awaiting hand-test. Max count = max(`raised_troops`, current) per type; deserters = ceil(max x 10% x unfunded / consumption), capped at current. A stopped-early army keeps its original `raised_troops`.
- B2. Gradual attrition / desertion with a cap. After B1; rules to design first.
- C. Campaign AI supply logic: stock up before departure, keep supply lines open, preparation before water crossings (options: wait for full supplies, extra wagons, minimum days of supply by crossing length, refuse to cross when the line cannot follow). User to pick.
- D. Shared campaign supply pool (P5 decisions in `alpha_war_extensions_2026-10-18.md`).

## Open

- Pricing of supplies.
- Water crossing behavior (C).
- Sim `campaign_replace_lost_armies` re-raise gold sink.

## Next step

User designs A, then a builder implements B. C and D follow.
