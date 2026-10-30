# Alpha: war extensions

Started: 2026-10-18. Design: `management/design_doc.md`, Alpha > War & conquest. Builds on `basic_army_battle_system_2026-10-15.md`.

## Scope

Troop-type counters and defense rolls, sieges (starve or attack, building defense bonuses), settlement conquest, caravan plunder, accompany orders, water crossing penalties, direct movement, military buildings, bonus system for armies / battles.

## Army rework (started 2026-10-25)

Rebuild army mechanics closer to the user's vision: encourage correct behaviors, slow the military pace of the game.

- Prerequisite: proper movement, straighter lines + travel cost (`straight_line_movement_2026-10-15.md`, `path_commitment_rewrite_2026-10-22.md`).
- Design: user is writing it into `management/design_doc.md`; then break into tasks here, then break into tasks here.
- May absorb / reorder: `army_supplies` tasks 3-4, `trade_market_goods` phases 2-3, backlog army items (stance / retreat, standing orders, combining, detection index).
- Multi-file architecture change.

## Tasks

Builder survey done; code map in `src/game_common/world_sim/.ai_imprint/military_state.md`. Each phase is reviewed / hand-tested by the user before the next.

- P0 [DONE] Fixes: levy draw bug, remove dead `siege_defenders`.
- P1 [DONE] Garrison: levy pool (see decisions), garrison joins assault battle as a side.
- P2a Standing orders: data + Station / Attack replacing current move / attack 1:1.
- P2b Remaining orders: Defend territory, Raid, Go home; invalid-order fallbacks; AI switched to orders.
- P3a Siege as engagement (supersedes P1 siege-as-battle-side), plan done:
  - S0 [DONE] Data model (kind, phase, main_target, commanders, settlement.siege, attacker / defender side_state), no behaviour change. Siege starts directly in ASSAULT.
  - S1 [DONE] Starvation + caravan rules. Returning caravans stop 10 tiles from a besieged origin via world_movement_stop in movement_decision.
  - S2 [DONE] Idle BESIEGING phase, BEGIN_ASSAULT, AI assault rule, engagement client mode / panel / render (selectable, settlement-sized).
  - S3 LIFT_SIEGE, WITHDRAW_ARMY, lifted-when-empty.
  - S4 SALLY_OUT, outside-battle phase, commanders.
  - S5 Rename battle -> engagement (after P2).
- P3b Battle damage / defense model.
- P3c Raiding / looting, live off the land, supplies homecoming (absorbs `army_supplies` 3-4).
- P4 Campaign entity (+ UI).
- P5 Campaign AI, replacing `match_ai` pseudo-campaign.

## Decisions

- Conquest: the settlement is simply transferred to the victor, permanently. Building damage / wealth looting maybe later.
- Gifting settlements between players peacefully: maybe later (see diplomacy unit).
- Levies: garrison pool replenishing like other types; max and training speed depend only on population; no ceiling, no upkeep. Pool levies do not reduce peasants / unemployed / workers. Losing or raising a levy kills one pop (unemployed, then worker, then peasant).
- Raising phase stays; armies raised anywhere in territory, incl. inside the settlement. Composition never changes, no reinforcement (except campaign armies).
- Standing orders (invalid -> Station at current location; attack-settlement order -> Defend it if no longer hostile):
  - Station: go to location, stand, attack hostiles within leash 5 / 10 tiles.
  - Defend territory: wander randomly in territory, attack intruders; retreat into settlement if a stronger force arrives.
  - Attack / Besiege: attack target entity, start / join battle or siege.
  - Raid: target territory; go there, raid, attack nearby enemy / neutral caravans, flee threats.
  - Go home: return to home settlement and disband.
  - Current move = Station, current attack = Attack. Move / attack commands stay as wrappers.
- Target scans: full scan of all armies (no spatial index yet).
- Battle: per type damage and defense. Round: each side deals up to its damage count (random min-max %), spread over armies as now, then over troop types; each type reduces damage by random 0..its total defense. No retreat / morale yet.
- Siege (revised): separate entity. Settlement is just put under siege; no battle by default. Siege owner (commander, owner of the army that started it) can Begin Assault; settlement owner can Sally Out. Battle damage only during an assault / sally; once started by either side it lasts until resolution. Settlement joins that battle directly as a side (no temp army). Siege / starvation apply whether or not an assault is ongoing: production lost, no trading, consumption halved, no pop growth.
- Siege as engagement (supersedes "separate entity"): battle entity becomes a "Military Engagement" state machine (`kind` field / siege, `phase`), main target = army or settlement; no new entity type. Rename battle -> "engagement" (in scope, separate mechanical step).
  - Lifted when no armies remain on the attacker side (any reason). An army can withdraw via a special action; siege commander can lift the whole siege from the engagement.
  - Any player's armies can join either side. Defending armies cannot enter the settlement once the siege started; if they attack besiegers they join on the defender side, forcing an "outside battle" that the garrison / armies inside do NOT join unless they sally out (engagement turns back into a sally).
  - Each side has a commander player, determined contextually: siege commander changes if another player's troops outnumber the commander's; if all armies are in a campaign, the campaign leader army's owner commands.
  - All engagements pin involved armies until concluded or explicitly left when possible (idle siege included).
- Caravan attack: an army that attacks a caravan takes half its value into supplies and half into gold. Stationed / raiding armies attack hostile / neutral caravans (P3c).
- Raid input: army is switched to raiding (button) and raids the territory it stands on; only allowed in territory not owned by the army's owner. Armies carry a gold stock (shown in UI); loot fills it in P3c; added to treasury on homecoming.
- Siege details:
  - Icons (placeholder SVG): swords for field battles and for any fighting siege phase (assault, sally, outside); catapult only while idle BESIEGING.
  - Siege on a settlement with no garrison and no defender armies resolves immediately (conquest).
  - Outside battle: an army attacking the besiegers joins the DEFENDER side; garrison / inside armies don't join. Defenders win -> siege lifted; attackers win -> siege continues. Sally / assault / outside clashes last until one side is dead / gone.
  - Defender commander is always the settlement owner. Campaign leader commander: stub until P4.
  - Engagement storage full: assert (never expected).
  - Engagement fields use explicit attacker_* / defender_* names, not 2-element arrays.
  - Caravans cannot enter a besieged settlement: origin besieged -> wait some distance away; destination besieged -> only within 30 tiles re-check, pick a new target if still besieged.
  - Training, construction, trade decay continue during a siege. Growth: only positive growth suppressed.
  - All engagement actions (begin assault, sally out, lift siege) live on the engagement entity; only army-specific actions (withdraw) on armies.
  - Siege engagement located between the besieging army and settlement; selectable; significance / size equals the settlement's (TS uses the same formula).
  - A foreign army joins the defender side only by attacking besiegers (stationing in settlements later).
  - Rename after P2, one commit (C++ + TS + HTML); client GDO names kept until then.
- Levy pop rules: raising a levy kills pop (becomes soldier); army levy deaths / desertion do not kill pop; garrison levy deaths do. Levy deserters and disbanded-army levies return as unemployed pop. Levy max 20% of pop, training 2%/day (min 1). Army levies pay upkeep, garrison levies don't. Garrison levies not in total_troops. Excess levies vanish on pop drop.
- Raid: army on raid location takes up to 10% of settlement production per day, capped by 10% of its carrying capacity; excess destroyed. Loot split supplies / gold, 50% start.
- Live off the land stance: 15% slower; supplies from tile and troop count, logistic curve exceeding consumption (1 per soldier) under 150 men, plateau above.
- Campaign: real entity (own logic); region = territory + settlement; Defense objective only for own territory for now. Leader army = army it was started from. Valid trade target with settlement-like profit formula, but must hold gold to pay (1:1 + scarcity), no distance bonus. 75% of garrison becomes a new campaign army; settlement in one campaign at a time; campaign army refills instantly from home while in friendly territory. Vassal / overlord forced join later. Entity limits unchanged.
- Campaign UI: tall banner on a shaft at leader army tile, large minimum screen size. Selected: name "Campaign", objectives, member army list (inspection extra data), faint white lines to members and objectives, state, each member's order.
- Campaign AI runs in the deterministic world sim; replaces `match_ai` pseudo-campaign.
- Movement session works on movement / pathing backend only; P2 may move / remove the army block in `movement_decision`.
- Builders run no build / test; user tests by hand.

## Depends on

`alpha_troops_recruitment_2026-10-18.md`, `basic_world_economy_2026-10-18.md` (buildings), `straight_line_movement_2026-10-15.md`.

## Next step

P0, P1, S0 done, tested, committed. S0-S2 done, tested, committed. S3, S4, P2a, P2b written (unbuilt, untested); P2b amendments applied (Raid = switch on current non-owned territory, army gold stock + UI). S5 rename written (unbuilt; stale refs left in human imprints / docs: include_match.imprint.md, game_mechanics.imprint.md, military_design.md etc.), P3b in progress, then P3c (loot, live off the land, caravan attacks, supplies homecoming; army carrying capacity value to propose). User authorized chaining all phases until the campaign (P4, exclusive): S3, S4, P2a, P2b, S5 rename, P3b, P3c, one at a time, reported after each. `management/design/military_design.md` still lacks Troop Types, Battles, Sieges, Raiding & Looting, Standing Orders (decisions above stand in for now; user may write them in).
