# Settlement trade and construction

Started: 2026-10-03 (as `settlement_econ_tick`, renamed 2026-10-12)

## Goal

Next settlement economy mechanics on top of the existing population growth / land output:
trade and construction (e.g. infrastructure such as roads, using tile flags).

## Done (from the former econ tick scope)

- Population growth toward a target = land output + local wealth; land output from owned
  tiles weighted by influence and contestation (see `village_influence_2026-10-03.md`).
  Good enough for now; capacity = `pop + 10 + pop / 2`.

## Open design

- Trade: inputs, caravans, local wealth, trade attractivity; see design_doc.md.
- Construction: what can be built, costs, effect on tile cost / output.
- Tick cadence relative to the sim tick.

## Decisions

- Road experiment (commits 1830051, reverted in 5e776f3) is a throwaway; starting fresh.
- Road on a tile = low travel cost regardless of terrain (kept idea).
- Not instant, and not "from nearest road to target" (dropped).
- Pathfinding is its own general system, not road-specific (experiment's was too complex and road-focused).

- Pathfinding, two layers in game common (client and server both use it):
  - Low: synchronous path request, caller supplies the memory, returns a path. Client calls it directly for previews (movement, road construction).
  - High: world-wide path cache for re-using results.
  - Full tile-grid resolution.
- Construction: instant for now, but logic lives in a settlement construction system, never in the input command (so it can become timed later).
- Road target: player picks target, path auto-computed from the settlement; maybe separate start/end later.
- Generalized construction start: on-map elements (roads first) and in-settlement elements; may diverge later.

## Tasks

1. [ ] Read-only survey: current movement, tile travel cost, tile flags, any existing pathing.
2. [ ] Pathfinding low layer (request API, caller memory).
3. [ ] Wire movement orders to it.
4. [ ] Path cache layer.
5. [ ] Construction system (settlement-side) + road as first on-map type; input command only dispatches.
6. [ ] Client preview of path / road.

## Peer brief

- Goal: design discussion + read-only survey for task 1, then help the user define the pathfinding low-layer API (task 2). No source edits unless the user says so.
- Scope: game common (shared sim, used by server and wasm client). Find the relevant folders via **/*.imprint.md; start from the imprints for game common and its sub-folders.
- Survey, report to the user: how movement orders move entities today; where tile travel cost and tile flags live; entity and tile data layout; existing pathing, if any; the caller-supplied scratch memory pattern (arenas) in game common.
- Decisions made: see Decisions above (two-layer pathfinding, tile-grid resolution, instant construction in a settlement construction system and not in the input command, roads first).
- Constraints: no std library in game common / wasm; locality, simplicity, brevity; keep it simple, not road-specific (a previous attempt was too complex); small reviewable steps; flag multi-file architecture changes.
- Done: user has a clear picture of the current state and an agreed pathfinding API sketch. Record it in this file only if the user asks.
- Open questions: map size / grid dimensions; whether path cache keys on (start, end) or on regions.

## Depends on

`village_influence_2026-10-03.md`.

## Progress

Design phase. Next: survey (task 1).
