# BACKLOG: Combining armies

Input date: 2026-10-24

## Item

Let a player combine armies so many can be controlled as one.

## Description

- Not covered by an existing unit: `work_units/alpha_war_extensions_2026-10-18.md` has accompany orders (follow / help in battle), but design doc says armies are bound to their origin settlement and cannot merge.
- Option A: redesign army entities as a container of troop entities, each a group of soldiers that remembers its origin.
- Option B: a "Campaign" entity created when an army starts a campaign with a friendly army; acts as a high command that controls many armies at once. Armies stay separate entities.
- Campaign v1: common order only. Later: optional common supply pool armies / players can contribute to.
- Related: `army_standing_orders_2026-10-23.md` (a campaign could hold the standing order), `army_supplies` (supplies and origin settlement per army).
