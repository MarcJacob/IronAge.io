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
  - R0 [DONE, tested, committed] Small fixes: attack-target wrapper vs engagements, siege exit by order, lift siege fix, reach 2 tiles, stale comments / imprint lines. Field withdrawal removed (see decisions).
  - R1 [DONE, tested, committed 20db280; open: GO_HOME disband has no owner check, stale comment match_ai_tactics_military.cpp:87] Pop model (survey done, see decisions "Pop"): all soldiers (garrison + armies, all types) count in origin settlement's pop and use pool slots; raised ones don't consume local goods.
  - R2a [DONE, committed 8fd4763 / d1fa0ba] Stats table already matched doc; damage scale param; upkeep + carry set to doc values in `game_parameters.h`. Counter bonuses dropped for now. Stale: `match_ai/game_mechanics.imprint.md:63` (old upkeep numbers, user to edit), `military_state.md:58`, design doc line 68.
  - R2b [DONE, committed 8fd4763 / d1fa0ba] Wagons: fifth troop type `WAGONS = 4`, max = 2 per tradepost (`tradepost_wagon_max_increase`), raising moves 1 pop into `raised.wagons`, return as unemployed on disband / desertion, `army_wagon_speed_divisor` = 2 (army halves while it has any wagon; speed set every tick in `world_system_movement.cpp`), upkeep 1 / carry 100 / stats {0,0,30}, raise UI gauges + `target_wagons` in raise commands, ceiling command rejects wagons. AI raises none.
    - Follow-up [DONE in code, unbuilt]: garrison wagon pool like levies (max counts garrison + raised, `wagon_training_per_day` = 1, unpaid, garrison deaths kill pop). Zero-damage rules in engagement code: `troops_base_damage` helper; 0-damage army cannot start / join (`world_army_pick_engagement_side`, `world_army_attack_entity`); a side dealing 0 damage in a round loses (not in idle BESIEGING); both harmless = ends, armies freed. Raise validity: `target_wagons <= max - raised`.
    - Notes: besieged settlement with only wagons falls on the first assault round; new tradepost's pool fills +1/day. `.ai_imprint` files got appended R2b sections (old 4-type lines still present, overridden).
  - R2c Active-fighting flag [DONE, tested]: engagement state structs removed (arrays like armies, per-side `side_troops/damage/commander(side, i)`), `fighting_active` added; .cpp files moved to the already-committed enum names `SIEGE_IDLE/ASSAULT/SALLY/OUTSIDE_BATTLE`. Open: TS `ENGAGEMENT_PHASE` names and `commands.h` comments still old. Original intent: boolean on engagement state gating damage rounds; set true in any phase where troops deal / take damage. Other mechanics read it (standing supply mechanics already skipped in field battle by d1fa0ba; extend to siege assaults via the flag, idle BESIEGING keeps supply mechanics). Troop stats tweaked + engagements slowed in d1fa0ba (battle length playtested, good enough for now). AI `home_troops` / garrison levies question deferred.
  - R2d [DONE, tested] Disband / desertion levies fill the garrison levy pool up to max (`settlement_release_raised`), excess stays unemployed. Garrison levies are not soldiers (`soldiers_total` counts only raised levies), so no extra count change. Supersedes the "levies return as unemployed" Pop rule. Open: AI `home_troops` sum still includes `garrison.levies`.
  - R3 Army gold, pay, supply purchases (caravans / stationed settlement), armies + campaigns as trade targets. Replaces `army_supplies` tasks 3-4. Design: `military_design.md` "Supply system". Decisions:
    - Pay is automatic for every army in friendly territory (daily consumption in gold from treasury, if funded); a toggle to disable it may come later.
    - Fixed price per good (parameter, start 1 gold); pay = consumption x price, topped up to gold capacity. Whole payment goes to the selling settlement owner's treasury (no split). Survey: `world_sim/.ai_imprint/army_supply_survey.md`.
    - `supplies` becomes goods (today gold-equivalent). Purchases draw from `market_goods` only; besieged settlements still sell to armies stationed inside. Disband: gold to treasury, leftover supplies to home market. Raise: costs gold = one day of upkeep, plus one day of goods from the settlement if available (a minimum stays for the settlement, ~one day of its consumption).
    - Foreign non-hostile armies as caravan targets, `allow_foreign_export` applies (decide in R3c).
    - R3a+b [DONE, tested, committed]: gold pay + stationed purchase in `world_system_armies_upkeep`, `good_price_gold` param, `army_homecoming` in `world_sim_entity_army.cpp`, raise starts with gold + goods. Removed `homecoming_settlement_goods_percent`, `army_supply_refill_days`. "Stationed" = standing in a settlement's territory (any order). Pay partial if treasury short. Open: `world_destroy_army` callers (stop-raising before any man, empty raising army, engagement casualties) lose gold / goods; stale lines in `game_mechanics.imprint.md` ~66-69 (user to edit) and `army_supply_survey.md`.
    - R3c [DONE, tested, committed] Armies as caravan trade targets (notes: `world_sim/.ai_imprint/caravan_army_target.md`):
      - Caravan destination is a typed `entity_guid` (settlement or army): `destinations` / `destination` (GDO name `destination_settlement` kept), `caravan_destination_tile`.
      - Target = any live non-hostile army (incl. in battle / GO_HOME); friendly bonus `army_friendly_trade_bonus_percent`; profit via `trade_volume_profit(..., destination_is_army)`: no distance bonus, no short-trade ramp, capacity-ratio neutral; no small-army threshold; no scarcity change (origin malus no longer exists, replaced by the export % cap). Size = min(caravan value, room, gold / price).
      - Movement: followed with throttled path refresh (`caravan_army_follow_tiles`), retarget if dead / hostile; GO_HOME followed until destroyed. Army in an engagement: caravan stops at `engagement_join_radius`, delivers from there (radius + 1) only while `fighting_active` is false, else waits (no timeout, goods held).
      - Delivery (`caravan_deliver_to_army`): army gold -> caravan origin owner's treasury + income (even if same owner); origin volume sold + profit, territory settlement under the army sold; remainder retargets; no receiver share, no room reservation.
      - Direct purchase from `market_goods` only when army location == settlement location; pay stays territory-based.
    - R3d homecoming: covered by R3a+b. R3e UI: only the caravan inspector army text done; optional army-panel line for incoming caravans open.
    - Stale docs (user): `game_mechanics.imprint.md` ~66-69, `economy_design.md:88`; `army_supply_survey.md` stale on refill / homecoming.
  - R4 Looting. Design in `military_design.md` "Raiding & Looting". Decisions:
    - Engagement Loot Pool (gold + supplies): destroyed armies drop all gold / supplies into it; conquest adds 20% of peasants + 30% of urban pop (killed, 1 gold each); no building destruction. Shared among victorious armies by manpower, capped by carry capacity; leftover becomes goods / trade volume of the settlement where the engagement is located.
    - Banditry: caravans only (no workers); valid by origin owner's status (hostile / neutral); intercepted when in range of an army even when not chased; active chase depends on order (Station leash, Raid, Defend); loot half gold / half supplies, capped by carry capacity.
    - R4a [TODO] Loot Pool + army-death drops + conquest + distribution. R4b [TODO] Banditry.
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
- Pop (supersedes the levy pop rules below): every soldier stays in its origin settlement's soldier pop, in garrison or army; raising does not kill pop; army deaths / desertion reduce origin pop; disbanding returns soldiers to the garrison pool, except levies who return as unemployed pop (which refills the levy pool anyway). Garrison soldiers consume local goods like the rest of the pop; raised soldiers don't (they have army upkeep) but remain a demographic weight. If the origin is conquered, the old owner's armies attrit by desertion at a slow rate (param, ~2%/day) and deserters return to their home settlement (now the new owner's) for now; no death. Survey: `world_sim/.ai_imprint/soldier_population.md`.
  - Levies (the real gap; non-levy types already count in pop): garrison levies stay part of the working pop and consume local goods, unpaid; raising moves one pop (unemployed, then workers, then peasants) into `raised.levies`, `pop_total` unchanged; disband / desertion return levies to unemployed; max (20% of `pop_total`) counts garrison + raised; garrison levy deaths still kill pop; raised levies pay army upkeep.
  - Fix disband owner check (old owner's raised men must not enter the new owner's garrison).
- Engagement exit: field withdrawal system was tried and REMOVED (user to redesign later; ideas: WITHDRAWING state taking damage / no damage dealt, 5-round gate, exit by destination, auto-rejoin problem on far-side destinations; morale -> WITHDRAWING_SHATTERED). Now: fighting engagements pin armies (new orders are accepted and stored, executed after the engagement if the army survives); an army in an idle BESIEGING siege given an order that doesn't want it there leaves at once (WITHDRAW_ARMY = that exit, STATION at nearest friendly settlement). "Wants it there" = ATTACK on own engagement / army in it / besieged settlement, DEFEND of the besieged settlement from the defender side. LIFT_SIEGE destroys the engagement, every freed army gets STATION. Attack / pursuit reach = 2 tiles (`order_attack_reach_tiles`).
- R2 decisions: keep the current code formula (doc line 68 to be updated by user to match); add one game parameter scaling all battle damage for tuning battle length; wagons = fifth `TROOP_TYPE`, stats from `military_design.md`, count as soldiers only once raised (like levies, no garrison pool), each caravanserai (exists; code names it `tradepost`) allows raising 2, raising a wagon costs 1 pop like a levy, speed half of other types for now (army speed rework later: slowest type as base + size + carried amount), effective defense clamped to 1 (user may add +1 in the formula later, leave as is), raise UI included (just another type). Plan: R2a stats / formula / damage param / upkeep + carry set to doc values (counters dropped for now, no counter bonuses in code), then R2b wagons. R2a damage param done: `engagement_damage_scale_percent` (100) in `game_parameters.h`.
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

## Peer brief (R4a)

- Goal: engagement Loot Pool. Design: `management/design/military_design.md` "Raiding & Looting" > Looting; decisions: R4 above.
- Scope: Loot Pool (gold + supplies) per engagement; destroyed armies drop all gold / supplies into it; conquest adds gold = 20% peasants + 30% urban pop killed, 1 gold each (game parameters); no goods taken, no building destruction; at engagement end, pool shared among victorious armies (alive on the winning side; siege attackers on conquest) by manpower, capped by carry capacity (reuse Raid carry logic); leftover becomes goods / trade volume of the settlement whose territory the engagement is in, else vanishes.
- Files: `src/game_common/world_sim/` engagement files (`world_entity_engagements.*`, engagement systems), army destroy / disband callers, conquest code, raid loot code (carry helper).
- Imprints: world_sim chain, `.ai_imprint/military_state.md`, `army_supply_survey.md`.
- Constraints: armies destroyed outside an engagement keep current behavior; no new client UI (ask before adding a stock readout); no build / test; warn on multi-file architecture changes.
- Done: code complete, user hand-tests. Report changes, params, not done, test checklist.
- Open: none (goods taken at conquest = no; field battle with no territory settlement = leftover vanishes; assumed from recommendations).

## Depends on

`alpha_troops_recruitment_2026-10-18.md`, `basic_world_economy_2026-10-18.md` (buildings), `straight_line_movement_2026-10-15.md`.

## Army / engagement cleanup (peer session, ~30 files, DONE, tested, committed)

Grew out of R1 leftovers.
- R1 leftovers [DONE in code]: GO_HOME with changed-hands home -> men desert, army destroyed, gold / supplies vanish; `world_disband_army` asserts owner; stale comment fixed.
- Troops: `TROOP_TYPE`, `army_troop_count`, one `troop_type_stats {damage, defense, armor}` table (health gone), `army_total_manpower` (ui64).
- Engagement code isolated in own files (`world_entity_engagements.h` + .cpp, enums for side / kind / phase, toolbox functions, both engagement systems); leftover constexprs became game params / AI constants. Commands no longer touch an army's engagement; each order handles it.
- Leave rule: `world_engagement_can_be_left` = army beyond join radius x param (2), or attacker side of an idle siege. Auto-join is aggro only, skips leavable engagements; STATION joins a siege whose join radius contains its station point; ATTACK joins on arrival.
- DEFEND / RAID no longer wander (RAID_MOVING and its params removed).
- Fix pending build: engagement GDO count properties switched UI32 -> UI16.
- Watch in playtest: armies leaving fights beyond 2x radius; STATION joining sieges; ATTACK reaching idle sieges via order only; caravan siege retarget now on movement tick.
- Open: battles end in 1-2 rounds (formula / numbers, user to decide; design doc line 68 same); splitting `systems_war.h` per entity conflicts with one-header-per-category rule in `world_sim.imprint.md` (decision needed); `world_army_attack_entity` not yet moved to army files; `order_hostile_men` / `order_find_hostile_army` stay in orders file.
- Stale docs: `management/design/military_design.md` (formula), older sections of `military_state.md`.

## Next step

Everything through P3e is tested and committed (0031653).
Status: R0, R1, cleanup, R2a, R2b done and committed. R2c (active-fighting flag) and R2d (levies back to pool on disband) done and tested; user commits. R3 design done and decided; R3 done, tested, committed. Next: R4-R5.
Remaining: P3b scope check; P3c leftovers (caravan attacks, conquest looting, trade malus, army-to-army looting, live off the land); re-add withdrawing from engagements properly; `SET_ENTITY_ATTACK_TARGET` wrapper refuses engagements; engagement rendering vision (bars, fighting animation; bars on map vs panel undecided); stale `commands.h` comments (RAID target, WITHDRAW_ARMY) and `world_sim/.ai_imprint/military_state.md` lines 3-10; then P4 (campaign), P5. `management/design/military_design.md` still lacks Troop Types, Battles, Sieges, Standing Orders sections (decisions above stand in; user may write them in). User decides what is next.
