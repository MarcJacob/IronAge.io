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

Not broken down yet.

## Decisions

- Conquest: the settlement is simply transferred to the victor, permanently. Building damage / wealth looting maybe later.
- Gifting settlements between players peacefully: maybe later (see diplomacy unit).

## Depends on

`alpha_troops_recruitment_2026-10-18.md`, `basic_world_economy_2026-10-18.md` (buildings), `straight_line_movement_2026-10-15.md`.

## Next step

Not started.
