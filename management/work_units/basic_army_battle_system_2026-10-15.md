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

None. Battle UI hand-tested OK (2026-10-16).

## Phase 2 (2026-10-19, unattended run; commit only, never push)

Phases (sequential commits by manager):
- A [client UI]: generic entity inspection panel (troops + settlement macro stats), new "advanced action panel" above action bar, army raising from settlement uses it; fix raising mode text.
- B [sim]: raising rework: N raised armies per settlement (number, not bool); army still counts in settlement pop (track raised %); spawns inside settlement, moves to target when done raising; right click retargets; origin kept. Client: path preview green (move) / red (attack target) for selected army.
- C [sim+UI]: Warband (current army) / Army (up to 64 warbands by entity index, same owner, join within 5 tiles, merge, detach, accompany friendly). Army panel: combined bar + warband list.
- D [sim+UI]: Siege entity (-5% pop/troops per growth tick), join/attack besiegers, Assault (defense x2) / Lift / detach; defender Sally (no bonus); defending-army spawn in own function. Disband action (inside home territory).
- E: svg icons crossed swords (battle), catapult (siege).

### Architecture gaps noted (for user)

(builders append here, terse)

- A: *front_src.imprint.md* lists the UI elements (hint, action bar) and not the properties panel nor the new advanced action panel / *entity_panels.ts*: imprint needs a user update.
- A: the raise mode still previews a path from the settlement to the hovered tile, though the army spawns at the tile (no walk today). Kept as is; it becomes accurate with phase B (spawn inside, walk out).
- A: *ui_element_advanced_action_panel.ts* imports the renderer (`PanelView`) from *ui_element_properties_panel.ts* (element -> element import). Could move to a shared *ui/panel_view.ts* if more panels come.
- B: raised men are claims on the home settlement, not removed. Population shrinking (growth) or a lowered troop ceiling can leave the settlement holding fewer men than its army claims; removals saturate (population never below 1, pool at 0). No reconciliation.
- B: pool draws only compare against the raising army's own counts (`pool - army count`), correct only while `SETTLEMENT_MAX_RAISED_ARMIES` is 1. More armies per settlement must subtract every raised army's claims.
- B: siege levies (20% of population) are taken from a population that still counts the settlement's raised men (they fight twice). To revisit in phase D.
- B: an army whose home settlement is lost (destroyed / captured) keeps its men, now counted nowhere; its later losses / settling take nothing from anyone.
- B: a raising army pulled into a battle loses its stored destination (battle_join stops it); after the battle it raises in place, wherever it stands.
- B: raised armies are derived (scan of all armies by home settlement), not stored: O(armies) per raise validity check and per settlement query.
- C1: the army GDO lists warbands as 64 flat properties (`warband_0`..`warband_63`, no array property type); the web entity GDO buffer went 384 -> 2048 bytes. An array GDO property type would be cleaner.
- C1: SETTLEMENT_MAX_RAISED_ARMIES (and settlement GDO `raised_armies`) now counts warbands; names kept.
- C1: an army following an army that merges away (destroyed) just stops; it is not redirected to the merged army.
- C1: detach selection = bit mask of slots in the army's warband list; slots shift down when warbands leave (merge/detach/death), so a client must build the mask from a fresh army GDO.
- C1: FOUND_SETTLEMENT on an army in a battle (pre-existing): its men stay counted in the battle side, which then never empties.
- C1: battle damage is rounded up per warband (was per army): an army of many warbands loses slightly more per round.
- D1: *world_sim.imprint.md* lists the themes per file; the new *world_system_siege.cpp* (siege theme) is not in it: imprint needs a user update.
- D1: sieges are not in the client render state (no render entity, not pickable): a siege drawn on its settlement would contest clicks on the settlement (attack target). Reached through the settlement / army GDO `siege` field instead. Rendering is D2's choice.
- D1: any non-owner army attacking a besieged settlement joins its siege, whoever started it (as the old siege battle did); the commander stays the first attacker. DETACH_FROM_SIEGE is accepted from the army's owner or the commander.
- D1: siege defenders still levy 20% of a population that counts the settlement's raised men (B gap kept: "exactly like now").
- D1: sieges (like battles) are not counted in `match_get_required_mem` (about 90 B each, one slot per micro entity), covered by the 32 MiB base.
- D1: armies in a siege keep any order given meanwhile (move, attack, merge) and act on it once out, like raising armies; DETACH_WARBANDS out of a besieging army gives a free (non-besieging) army.
- D1: the defense bonus has no per-unit-type defense to double (battles have none): it halves the attackers' damage (rounded up) for that battle.
- C2: *front_src.imprint.md* mode tree says "Army selected: committed action (Settle), no hotkey": it now also has Stop raising / Detach and a hint line; imprint needs a user update.
- C2: the selected army's path preview is red for any action target, including accompany / merge (own army). Could use the army `state` instead.
- C2: per-frame panel cost for a selected army: 1 + 2 x warbands entity queries (warband + its home), up to 129 per frame.
- D2: *front_src.imprint.md* mode tree lacks "Siege selected" (child of Main play, any viewer); imprint needs a user update.
- D2: battle / siege render entities have no owner, so they are always tinted as not owned (red), and enemy-pick (right click) offers them as targets: a battle target is still sent and refused (pre-existing), a siege is mapped to its settlement.
- D2: Assault / Lift / Sally / Disband validity is asked to the backend every frame (one command build each) to grey the buttons out; no reason is shown (only the Disband hint says "inside home territory").
- D2: path preview of a raising army with a stored attack target is now green (its state is RAISING); red only while state is ATTACKING.

### Phase D2 notes (client: sieges, icons, disband)

- Sieges in the render state (*game_client_render.cpp*, like battles, owner INVALID). render.ts draws a siege icon right of its settlement's icon, edges touching (`entity_draw_center`, also used by picking): both stay clickable, no overlap at any zoom.
- New mode Siege selected (*modes/siege_selected.ts*), pushed by Main play for any selected siege. Panel `siege_panel` (*entity_panels.ts*): besiegers (men, armies), defenders, state; red / blue balance bar; one list row per side with its troop type split bar (`troop_segments` got a property prefix). Commander: Assault, Lift siege; defender: Sally out; others: panel only. Buttons disabled while `is_command_valid` says no; hint per role.
- Army selected: "Detach from siege" while its GDO `siege` is set; "Disband" always, disabled while invalid. Right click on a siege icon attacks its settlement (joins the siege). `ARMY_STATE.BESIEGING` mirrored, panel text "Besieging".
- Icons: *web/art/entity_battle.svg* (crossed swords), *entity_siege.svg* (catapult), hand-written black silhouettes loaded and tinted like the other entity SVGs. Battles now get the selection outline too.
- Path preview: red only while the army state is ATTACKING, green otherwise.

### Phase C2 notes (client UI)

- `ARMY_STATE` mirrored in *backend.ts* (next to `ENTITY_TYPE`); panel shows it as text (FOLLOWING = "Accompanying").
- Orders (army selected): right click on another own army = accompany (SET_ENTITY_ATTACK_TARGET), Shift + right click = MERGE_ARMY. Enemies still take priority under the cursor. Chosen over a merge hotkey / mode: no new mode, one modifier. Hint line added to the mode.
- Map label: total men, plus "(N)" warbands when N > 1.
- `PanelState` extended: `split_bar` (proportional `BarSegment`s) and `list_title` / `list` (`ListRow`: text, optional small split bar, selected, on_click). Order: stats, split bar, list, gauges.
- Army panel: Men / State / Warbands stats. One warband: its gauges as before (raise sliders while raising). Several: split bar of the army's types, warband list (slot, men, home location, raising; click toggles selection), and the gauges (with raise sliders) of the focus warband = the only selected one.
- Bar: Settle, "Stop raising" (focus warband raising), "Detach" (some but not all warbands selected). Detach mask is built from a fresh army snapshot at click time; selection is kept by warband guid and pruned each frame.

### Phase C1 notes (sim + commands, no new UI)

- Model: `ENTITY_TYPE::ARMY` (value 3, unchanged) is now the group; new `ENTITY_TYPE::WARBAND` (6, appended) is the old army content (owner, men, raise targets, home settlement, raising, parent army). Every warband is always in exactly one army; RAISE_ARMY spawns a warband in a new army of one. Armies hold up to 64 warband slot indices (fixed array, joining order kept).
- Army-level: location, movement, action target, battle / side, merging flag. Warband-level: men, raising, home claims (`world_warband_remove_men_from_home`).
- Battles: join sums the army's warbands; each round damages each warband by its share of its side; a warband at 0 men is destroyed, and its army with its last warband. Battle state struct unchanged.
- Raising per warband; an army waits in place while any of its warbands raises. Settling (FOUND_SETTLEMENT, army guid) settles all warbands, each removing its men from its own home.
- Accompany = SET_ENTITY_ATTACK_TARGET on another own army: chases it, stops within 2 tiles (`ACCOMPANY_DISTANCE_TILES`), joins the followed army's battle (same side, any distance) and keeps following after it. Never merges.
- Merge = new MERGE_ARMY command (army, into_army): same chase, merges once within 5 tiles (`ATTACK_REACH_TILES`). If the target is in a battle first, joins it and the merge order is dropped. Dropped too if the warbands no longer fit (64). Chosen over "follow always merges", which would make accompany unable to persist.
- Detach = new DETACH_WARBANDS command (army, slots_low, slots_high): new army on the first land neighbour tile (own tile if none), no orders. Refused: empty / out of range / all slots (new `COMMAND_ERROR_INVALID_SELECTION`, mirrored in input.ts), army in battle (UNSPECIFIED), no army slot.
- SET_ARMY_RAISE_TARGETS / STOP_RAISING_ARMY now take a warband (field renamed `army` -> `warband`, same layout).
- Army GDO: owner, location, movement_*, action_target, arrows_target, merging, battle, state (`ARMY_STATE`, world.h, not mirrored in TS yet), raising (any warband), followed_army, composition_* (sums, now UI32), warband_count, warband_0..63 (guids). Warband GDO: owner, composition_*, raise_target_*, home_settlement, raising, army.
- Client kept working with minimal edits: ENTITY_TYPE mirror, render scale lookup entry, error mirror, army_selected.ts raising controls act on `warband_0`.

### Phase D1 notes (siege + disband: sim, commands, GDO; no UI)

- Model: new `ENTITY_TYPE::SIEGE` (7, appended), one per settlement. State: location (settlement's), settlement, commander (first attacker's owner), defender (settlement owner), battle (assault / sally in progress), army_count, armies[16] (`SIEGE_MAX_ARMIES`, guids, joining order). Armies hold `sieges` (guid), like `battles`.
- An army reaching an enemy settlement no longer starts a battle: it starts or joins the settlement's siege (full siege / no siege slot: it just stops). Besiegers are pinned (movement and target resolution skip them) until they leave it. Attacking a besieging army is the normal army-vs-army battle.
- Siege system (`world_system_sieges`, new *world_system_siege.cpp*, end of tick right after battles): drops destroyed armies; ends the siege when its battle is over (whoever won), the settlement is gone or changed owner, or no army is left; on the settlement's growth ticks takes 5% (at least 1) of population (never below 1) and of each local troop count.
- Defenders: `world_spawn_settlement_defenders` (world_entities.cpp), numbers from `settlement_defender_men` (world_sim_population.h): unchanged rules (20% population levies, whole pool, taken out, given back by `world_disband_army` if they win).
- `world_siege_start_battle`: battle at the settlement, siege flags as before (settlement taken if side 0 wins), besiegers not already in a battle on side 0, defenders side 1. New battle field `defense_bonus`: side 0 damage halved (assault only). A besieger arriving during it joins it.
- Commands (payload = one guid each, GDO cases added): ASSAULT_SIEGE (siege; commander), LIFT_SIEGE (siege; commander, not during its battle), DETACH_FROM_SIEGE (army; army owner or commander), SALLY_OUT (siege; defender), DISBAND_ARMY (army; owner, not in a battle, army tile in the territory of every warband's home settlement still held by the warband's owner, else `TARGET_OUT_OF_TERRITORY`). Assault / sally refused (UNSPECIFIED) with a battle in progress or no besieger free to fight; ENTITY_LIMIT_REACHED without army / warband / battle slots. No new error bit.
- Disband just destroys the army: per phase B its men are still counted in their home settlements (no men added back).
- GDOs: siege (location, settlement, commander, defender, battle, army_count, besieger_<type>, defender_<type> (what the settlement would field now), army_0..army_15); settlement and army get `siege`; battle gets `defense_bonus`; `ARMY_STATE_BESIEGING` (7) appended.
- Client: only `SIEGE = 7` in backend.ts ENTITY_TYPE and its render.ts scale entry.

### Phase A notes

- Panel content is one `PanelState` (title, stat rows, optional gauges heading, gauges); properties panel and advanced action panel both render it with `PanelView`.
- Per-entity-type states in *ui/entity_panels.ts* (`settlement_panel`, `army_panel`). Settlement stats: population, wealth, land output, tier (what the GDO has). Army stats: men, state (in battle / raising / ready).
- The settlement panel now stays visible while raising (the raise controls moved to the advanced action panel).
- Raise mode shows no validity text in the panel (feedback still only colors the path preview), to keep scope.

### Phase B notes

- Model: a raised army's men stay counted in its home settlement's population (levies) and local pool (other types); the army holds a claim. Rule: every man the army loses (battle) or takes away (settling) is removed from the home settlement (`world_army_remove_men_from_home`), while the home is valid and has the army's owner.
- Raised armies / men are derived, not stored (`world_settlement_raised`). `SETTLEMENT_MAX_RAISED_ARMIES = 1` constant (*world_sim_population.h*). Settlement GDO: `raised_armies`, `max_raised_armies`, `raised_men`, `raised_percent` (men of every type / population, rounded down).
- Raise limit reuses `COMMAND_ERROR_ENTITY_LIMIT_REACHED` (no new error flag, no TS mirror change).
- Army spawns at the settlement with `movement.destination` = clicked tile; the movement system skips raising armies (except to walk to a battle). Move and attack orders are now accepted while raising (stored, acted on once raised); settling still refused while raising, now with `COMMAND_ERROR_ARMY_RAISING`. Raise location still has to be in the settlement's territory.
- Levies are claimed while population > army levies + `RAISE_MIN_POPULATION_LEFT`; pooled types while pool > army count.
- Client: selected army previews the path to its current destination, green for a move, red while it has an attack target (optional `color` on `RENDER_CONFIG.path_preview`). Raised rows (`settlement_raised_rows`) in the settlement panel and the raise panel.

### Review pass (a4f1766..55e64ba, by reading only: nothing compiled)

Fixes made: none. No clear-cut compile error or inconsistency found.

Checked and consistent: designated initializer order (settlement / army / warband queries, spawn player), command enum vs table (16 rows) vs web GDO cases, COMMAND_ERROR mirror, ENTITY_TYPE mirror (WARBAND 6, SIEGE 7) and render scale lookup, ARMY_STATE mirror, GDO property names read by TS (army / warband / siege / settlement), GDO sizes (army GDO ~1.1 KiB wasm32 / ~1.4 KiB x64 < 2048 entity buffer; largest command GDO < 128), `warband_##Slot` / `army_##Slot` macros, unity include order, mixed `&&` / `||` all parenthesized (-W -Werror builds), HTML ids used by the panels, TS strict narrowing (`is_own_army` is a type guard).

Unresolved risks (not changed, design-level or pre-existing):
- `vec2_dist_squared` returns float and is used in sim distance checks (now also the accompany distance). Deterministic with the pinned flags, but a float in sim.
- SET_ENTITY_ATTACK_TARGET validity reads `owners[targetIndex]` of an enemy settlement / army without checking the target guid is valid (out of range read on a crafted guid). Pre-existing, the army case kept it.
- `match_get_required_mem` does not count sieges, battles, builders or the 64-slot warband lists: relies on the 32 MiB slack.
- `entity_guid_new` asserts look swapped (micro asserts index <= 0xFFF, macro <= 0xFFFF). Pre-existing; micro types assert past 4096 slots.
- A lost sally out hands the settlement to the siege commander (same outcome rule as an assault). Intended?
- Detaching from an army in a siege makes a free army (not in the siege) next to it.
- front_src imprint mode tree does not list Siege selected (imprint, not edited).

### Status

- A, B, C1, C2, D1, D2 (incl. E icons), compile-risk review: committed, NOT built or hand-tested (imprints forbid agent builds). Next: user builds (wasm, win32, tsc), hand-checks, fixes. Imprint updates pending (front_src, world_sim).
- Note: C2 note "ARMY_STATE not mirrored in TS" is stale (mirrored in C2/D2).
