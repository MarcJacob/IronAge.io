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
- Design now in `management/design/military_design.md` (troop stats, wagons, battle formula, supply system, conquest looting). Tasks (each reviewed by user before the next):
  - R0 [DONE, tested, uncommitted] Small fixes: attack-target wrapper vs engagements, siege exit by order, lift siege fix, reach 2 tiles, stale comments / imprint lines. Field withdrawal removed (see decisions).
  - R1 Pop model: all soldiers (garrison + armies, all types) count in origin settlement's soldier pop and use pool slots; no local goods consumption by them.
  - R2 Troop stats, Wagons, new battle formula, counter bonuses.
  - R3 Army gold, pay, supply purchases (caravans / stationed settlement), armies + campaigns as trade targets. Replaces `army_supplies` tasks 3-4.
  - R4 Conquest looting, banditry, army-to-army looting.
  - R5 P4 campaign / P5 campaign AI (shared supply pool).

## Tasks

Builder survey done; code map in `src/game_common/world_sim/.ai_imprint/military_state.md`. Each phase is reviewed / hand-tested by the user before the next.

- P0 [DONE] Fixes: levy draw bug, remove dead `siege_defenders`.
- P1 [DONE] Garrison: levy pool (see decisions), garrison joins assault battle as a side.
- P2a [DONE] Standing orders: data + Station / Attack replacing current move / attack 1:1.
- P2b [DONE] Remaining orders: Defend territory, Raid (stub), Go home; invalid-order fallbacks; AI switched to orders.
- P2c [DONE, tested] Orders cleanup: one `_logic` function per order type (`order_travel` helper), `ARMY_ORDER_STATE` enum (TRAVELLING = 0, shared), `order_*` game parameters, diplomatic status helpers (final API under P3d). Arrival resolves at the start of the next tick.
- P3c [DONE, tested] Raiding:
  - A [DONE, f4e6942] Raid states: RAID_MOVING, territory-entry trigger, slow-move param.
  - B [DONE, 2419fb8] Loot, carry-capacity params (replace supplies days cap), daily cap on settlement, raided flag, UI stock. Loot runs before economy daily tick on last day's production.
  - C [DONE] Gold pool full (checked after the daily loot) -> auto GO_HOME + disband; deposit in `world_disband_army` (all callers): gold to treasury, supplies split by `homecoming_settlement_goods_percent` (50, placeholder) between home market goods and treasury. Replaces `army_supplies` task 3 (army-to-army looting still open). Manual disband now also deposits supplies.
  - D [DONE, tested] Targeted orders: Raid takes a target settlement from the command (army anywhere). One client territory-targeting mode with presets Defend / Attack / Raid: territories green (valid) / grey / yellow (hovered), mode left only by hotkey again, right click or Escape; numeric hotkeys. Defend valid = friendly territories; Attack / Raid valid = hostile or neutral. Station and non-settlement attacks stay contextual (right click); Go home keeps its button. Friendly / neutral are stubs behind the diplomatic status helpers.
  - Still open: caravan attacks (banditry), conquest looting, trade attractiveness malus, army-to-army looting, live off the land.
  - Tuning: supplies capacity per man is 20 / 3 / 2 / 1/3 days of own consumption (user expected 10 / 2 / 5 / 1): upkeep rates differ from those assumed; knights-only armies outside territory starve within a day. Tune carry values or upkeep if needed.
- P2d [DONE, tested] AI moved to standing orders (later: campaign system).
- P3a Siege as engagement (supersedes P1 siege-as-battle-side), plan done:
  - S0 [DONE] Data model (kind, phase, main_target, commanders, settlement.siege, attacker / defender side_state), no behaviour change. Siege starts directly in ASSAULT.
  - S1 [DONE] Starvation + caravan rules. Returning caravans stop 10 tiles from a besieged origin via world_movement_stop in movement_decision.
  - S2 [DONE] Idle BESIEGING phase, BEGIN_ASSAULT, AI assault rule, engagement client mode / panel / render (selectable, settlement-sized).
  - S3 [DONE] LIFT_SIEGE, WITHDRAW_ARMY, lifted-when-empty.
  - S4 [DONE] SALLY_OUT, outside-battle phase, commanders.
  - S5 [DONE] Rename battle -> engagement.
- P3b Battle damage / defense model: per-type damage / defense in battle exists; confirm remaining scope with user.
- P3d [DONE, tested] Stacked armies (AI campaigns piled armies on one tile: hidden, only one engaged):
  - Render grouping: entities (armies first, maybe all) too close to each other join a render "group", drawn as a whole with members shown separately (row) and their own numbers.
  - Engagement joining rework:
    - Joining is automatic: an army standing too close to an engagement joins it (joining = going there). No more willing join by attacking a member army.
    - Armies cannot target armies that are part of an engagement; the engagement itself is the target.
    - Engagement relation to a player: friendly (attackers are on my side), hostile (attackers hostile to me), neutral (attackers and defenders both neutral / hostile to me). Armies auto-join the correct side when friendly / hostile; never join a neutral one (for now).
    - Also fixes groups of small armies being hard to stop.
    - Join radius: 2 tiles minimum, +1 tile per power of 2 of total troops involved above 256 (512 -> +1, 1024 -> +2...). Sieges: count only troops not inside the settlement (for now: everything but the garrison).
    - Joining keeps the army's standing order and resumes it afterwards.
    - Render grouping starts with armies only; design so other entity types can use it.
    - Plan: builder A = sim (engagement joining rework), then builder B = client render grouping.
    - A [DONE]: auto-join system, params `engagement_join_base_tiles` / `engagement_join_troops_threshold`, hostility helpers accept engagements, ATTACK targets engagements (neutral engagement = wait at radius edge), right click on engagement.
      - Relation: `PLAYER_DIPLOMATIC_STATUS` (NEUTRAL = 0, FRIENDLY, HOSTILE). `world_player_get_diplomatic_status` (stub diplomacy), `world_entity_get_diplomatic_status`, `engagement_diplomatic_status`; bool entity helpers are inline wrappers. Engagement: better side (friendly > neutral > hostile) joined; equal sides = NEUTRAL, not joined; FRIENDLY = join attackers, HOSTILE = join defenders (side inferred from status).
      - WITHDRAW_ARMY disabled for engaged armies (WRONG_PHASE; button gone); re-add properly later. `left_engagement` removed. LIFT_SIEGE stays.
      - Open: `SET_ENTITY_ATTACK_TARGET` wrapper still refuses engagements.
    - B [DONE]: client army render grouping in `render.ts` (`group_by_proximity` helper, `RENDER_CONFIG.army_group_radius` 2 tiles, row layout, per-army count, hit-test at displayed position). Engaged and hidden armies not grouped; path preview / highlights use true positions.
    - Disband: levies return as unemployed pop, not peasants (already the case).
- P3e Engagement rendering (client):
  - [DONE, tested] Join radius circle; icon tinted by status to the local player (red hostile, gray neutral, blue friendly; `viewer_status` from the sim query), icon scaled to the circle.
  - [DONE, tested] Client entity rendering modularized: one leaf module per entity type in `front/src/entities/` (engagement, settlement, army, caravan + ship, worker), shared `EntityRenderEnv` (with `resources` store). `render.ts` stays the orchestrator (loops, visibility, army grouping, hit-testing). Layout note in `front/src/.ai_imprint/entities_folder.md`; `front_src.imprint.md` updated.
  - Vision (not planned yet): armies inside shown moving around playing a fighting animation, decorrelated from their map location (which is set to the engagement's location). Real information lives on the engagement only:
    - Symbol + join circle (current).
    - Top bar: strength breakdown of the two sides, colored by status to the player (no longer fixed red attackers / blue defenders).
    - Left side bar: attacker armies and their individual statuses; right side bar: same for defenders; bottom: armies physically inside but not involved / withdrawing.
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
- STATION / DEFEND chasing an army in a siege (joining defender side) is intended, within leash for STATION; DEFEND opposes hostile sieges of its settlement.
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
- Pop (supersedes the levy pop rules below): every soldier stays in its origin settlement's soldier pop, in garrison or army; raising does not kill pop; army deaths / desertion reduce origin pop; disbanding returns soldiers to the garrison pool, except levies who return as unemployed pop (which refills the levy pool anyway). Garrison soldiers consume local goods like the rest of the pop; raised soldiers don't (they have army upkeep) but remain a demographic weight. If the origin is conquered, soldiers keep counting but attrit without replacement until retaken.
- Engagement exit: field withdrawal system was tried and REMOVED (user to redesign later; ideas: WITHDRAWING state taking damage / no damage dealt, 5-round gate, exit by destination, auto-rejoin problem on far-side destinations; morale -> WITHDRAWING_SHATTERED). Now: fighting engagements pin armies (new orders are accepted and stored, executed after the engagement if the army survives); an army in an idle BESIEGING siege given an order that doesn't want it there leaves at once (WITHDRAW_ARMY = that exit, STATION at nearest friendly settlement). "Wants it there" = ATTACK on own engagement / army in it / besieged settlement, DEFEND of the besieged settlement from the defender side. LIFT_SIEGE destroys the engagement, every freed army gets STATION. Attack / pursuit reach = 2 tiles (`order_attack_reach_tiles`).
- Battle formula (per troop type, defense and armor min 1): `Casualties = (dmg / rand(1, defense_skill)) / rand(1, armor)`; both random (design doc line 68 still shows the old form, user to update). Counter bonuses (men-at-arms vs horsemen, horsemen vs levies) = +50% damage dealt to the whole opposing army for now; later maybe per-source-type damage so countered types die faster.
- Levy pop rules (OLD, superseded by Pop above): raising a levy kills pop (becomes soldier); army levy deaths / desertion do not kill pop; garrison levy deaths do. Levy deserters and disbanded-army levies return as unemployed pop. Levy max 20% of pop, training 2%/day (min 1). Army levies pay upkeep, garrison levies don't. Garrison levies not in total_troops. Excess levies vanish on pop drop.
- Raid (revised, simple; likely replaced later by per-tile raiding):
  - States: TRAVELLING = reaching raided area (initially / after fleeing); RAIDING (stationary) and RAIDING_MOVING (random moves inside raided area, same selection as Defend, slower) begin the moment the area is entered; FLEEING.
  - Loot runs each day tick in any raiding state except FLEEING. Max potential: 1/10 production (half gold, half supplies), 1/10 peasants killed (-> gold), 1/5 stored local / export goods (-> supplies).
  - Effective loot gated by army carrying capacity AND a per-settlement daily raided total (reset on daily tick; patchy, temporary until tile-based raiding).
  - Carrying: separate gold and supplies pools, same size, from per-troop-type carry capacity parameters (per man: levies 10, men-at-arms 6, horsemen 20, knights 10). Replaces `army_supply_capacity_days` as the supplies capacity.
  - RAIDING_MOVING speed multiplier is a parameter (50%). Random moves: same as Defend, in the raided settlement's territory.
  - Several armies on one settlement: processed in army index order (small randomness factor for fairness); no queue. Not an intended case.
  - Peasants killed -> 1 gold each. Goods -> supplies, no economic value conversion.
  - Raided flag set on any day a non-fleeing army loots; cleared after a day without looting.
- Live off the land stance: 15% slower; supplies from tile and troop count, logistic curve exceeding consumption (1 per soldier) under 150 men, plateau above.
- Campaign: real entity (own logic); region = territory + settlement; Defense objective only for own territory for now. Leader army = army it was started from. Valid trade target with settlement-like profit formula, but must hold gold to pay (1:1 + scarcity), no distance bonus. 75% of garrison becomes a new campaign army; settlement in one campaign at a time; campaign army refills instantly from home while in friendly territory. Vassal / overlord forced join later. Entity limits unchanged.
- Campaign UI: tall banner on a shaft at leader army tile, large minimum screen size. Selected: name "Campaign", objectives, member army list (inspection extra data), faint white lines to members and objectives, state, each member's order.
- Campaign AI runs in the deterministic world sim; replaces `match_ai` pseudo-campaign.
- Army supply idea (user, to be written into design doc): armies are always trade targets for caravans (unattractive if small enough to live off the land / raid); they must pay gold for supplies; payment split between caravan owner and origin settlement, so armies weigh on the local economy and can starve the populace if numerous / long-staying. Likely replaces `army_supplies` tasks 3-4 / ties to `trade_market_goods`.
- Movement session works on movement / pathing backend only; P2 may move / remove the army block in `movement_decision`.
- Builders run no build / test; user tests by hand.

## Depends on

`alpha_troops_recruitment_2026-10-18.md`, `basic_world_economy_2026-10-18.md` (buildings), `straight_line_movement_2026-10-15.md`.

## Next step

Everything through P3e is tested and committed (0031653).
Order agreed: (1) one builder for the small fixes (attack-target wrapper vs engagements, re-add withdraw, stale comments / imprint lines); (2) user writes missing military design sections incl. the army supply system; (3) P4 campaign (needs plan first; multi-file).
Remaining: P3b scope check; P3c leftovers (caravan attacks, conquest looting, trade malus, army-to-army looting, live off the land); re-add withdrawing from engagements properly; `SET_ENTITY_ATTACK_TARGET` wrapper refuses engagements; engagement rendering vision (bars, fighting animation; bars on map vs panel undecided); stale `commands.h` comments (RAID target, WITHDRAW_ARMY) and `world_sim/.ai_imprint/military_state.md` lines 3-10; then P4 (campaign), P5. `management/design/military_design.md` still lacks Troop Types, Battles, Sieges, Standing Orders sections (decisions above stand in; user may write them in). User decides what is next.
