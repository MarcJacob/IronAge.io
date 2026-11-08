# Army supply redesign + campaign AI supply intelligence

Started: 2026-11-07. Design: `management/design/military_design.md`, "Supply system" (not yet updated to the rules below). Original code description: R3 in `management/work_units/alpha_war_extensions_2026-10-18.md`. Supersedes `army_supplies_2026-10-23.md` open tasks. Related: `army_autonomy_2026-11-08.md`.

## Scope

1. Redesign how armies are supplied (gold reserve problem). Built, see B1*.
2. Campaign AI smart about supply and supply lines, especially before water crossings (C).
3. Shared campaign supply pool (D).

## Decisions

- Armies never spend carried gold (loot only). Army gold check and daily pay in owned territory removed. Army upkeep feel (e.g. gold sink for retinues) is a later design.
- Caravans to armies are paid at dispatch from the owner's treasury: pure gold sink (expense only, no income to any settlement owner, no trade tax), settlement only gains trade volume (size + profit). Current supply pricing unchanged.
- Armies carry a stored `incoming_supplies` (in-flight caravans); settlements send exactly enough caravans. Settlements carry a stored `incoming_goods`, counted at 50% (`trade.incoming_goods_scarcity_percent`, 0 = old behavior) in scarcity (target weighting and profit).
- An army is a caravan target only if it is a campaign member (`armies.campaign` valid) or is joining one (order JOIN_CAMPAIGN with a live target: TEMPORARY exception). Caravan resupply is a campaign feature; the campaign coordinates the supply line.
- ALL armies buy directly from the settlement they stand on (own-owner settlement, exact tile), treasury-paid sink, trade volume to the settlement.
- Caravans never retarget to armies. A caravan losing its army target (or finding no room) goes home. Any successful delivery destroys the caravan (no remainder).
- Redirected caravans obey the origin's `allow_foreign_export`.
- Desertion when out of supplies: per troop type, daily `ceil(max x 10% x unfunded / consumption)`, capped at current; max = max(`raised_troops`, current); full 10% only at zero supplies (`army_low_supply_desertion_percent`).
- Non-campaign armies away from settlements get no supply (intended).

## Tasks

- A. [DONE] Supply rules (see Decisions). Open: pricing.
- B1, B1b, B1c, B1d, B1e: built, DLL compiled. Awaiting full hand-test (rebuild with server stopped; exe link failed last time). Retinue direct purchase and the raising-army / origin-tile caravan bug were hand-tested OK earlier.
- B2. Gradual attrition curve beyond the simple rule above. To design.
- C. Campaign AI supply logic: stock up before departure, keep supply lines open, preparation before water crossings (options: wait for full supplies, extra wagons, minimum days of supply by crossing length, refuse to cross when the line cannot follow). User to pick.
- D. Shared campaign supply pool (P5 decisions in `alpha_war_extensions_2026-10-18.md`).

## Open

- Pricing of supplies.
- `caravan_candidate_size` (spawn size-to-room cap) uses raw market total; maybe count incoming goods (100%) if overkill persists after B1c.
- Update `military_design.md` supply section to the rules above.
- Water crossing behavior (C).
- Sim `campaign_replace_lost_armies` re-raise gold sink.

## Next step

Rebuild (server stopped) and hand-test B1-B1e (checklists: caravans reach campaign / joining armies, treasury drops at dispatch, no oversend, caravans released on target loss, desertion scaling). Then B2 design, C, D.
