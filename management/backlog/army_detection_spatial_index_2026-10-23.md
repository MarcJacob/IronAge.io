# BACKLOG: Army target detection + spatial index

Input date: 2026-10-23

## Item

Armies detect nearby entities and act on it: auto-attack enemies, flee from stronger ones, join friendly battles. Needs a spatial index of entities so queries are local.

## Description

- Applies when the army has no overriding standing order (see `army_standing_orders_2026-10-23.md`). Above a range the order AI chooses; below a certain range joining a friendly battle is forced by a dedicated system.
- Spatial index per region instead of a scan over all entities. Fixed per-region GUID buffer with a max count is considered flimsy / wasteful. Octree maybe later.
- Idea to evaluate: derived (non-snapshot) uniform grid rebuilt each tick, counting-sort layout (cell start offsets + one packed entity array), no per-cell cap. Alternative: per-cell linked list through a next-index per entity.
- Other consumers: match AI, caravan plunder, battle contact.
- Related: `work_units/alpha_war_extensions_2026-10-18.md`.
