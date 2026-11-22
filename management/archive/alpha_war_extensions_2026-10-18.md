# Alpha: war extensions [DONE]

Started: 2026-10-18. Design: `management/design/military_design.md`. Builds on `basic_army_battle_system_2026-10-15.md`. Live follow-ups: `campaign_objectives`, `army_supply_redesign`, `match_ai`.

## Built (tested, committed)

- Standing orders: Station, Defend, Attack, Raid, Go home, Accompany, Join campaign; targeted-order UI mode.
- Engagements: battles and sieges as one state machine (kind / phase); begin assault, sally out, lift siege; auto-join by join radius; commander per side; client grouping and rendering.
- Army rework: troop-type stats table, wagons (fifth type), garrison levy pool, soldier pop model, damage scale parameter, supplies as goods, gold pay and stationed purchase, armies as caravan trade targets.
- Looting: raiding, engagement loot pool, conquest looting, caravan banditry.
- Campaigns: entity with states (WAITING / GATHERING / MARCHING / ON_OBJECTIVE), objective CONQUER_SETTLEMENT, settlements raise campaign armies, retinues (player-made armies) vs campaign armies, campaign-driven assault decision.
- AI on campaigns (no retinues, no army orders).

## Decisions worth keeping

- Conquest transfers the settlement permanently.
- Battle formula (per troop type, defense and armor min 1): `casualties = (dmg / rand(1, defense)) / rand(1, armor)`.
- Siege idle BESIEGING keeps supply mechanics; assault / sally / outside clashes set `fighting_active` and pin armies.
- Armies hold no more than carry capacity; excess drops to the engagement loot pool or vanishes.
- `settlements.campaign_army` survives campaign end; a new campaign grabs it instead of raising.
- Campaign armies take only campaign logic; retinues take player orders.
- Campaign AI runs in the deterministic sim.
- Builders run no build / test; user tests by hand.
