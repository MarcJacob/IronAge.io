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

## Depends on

`village_influence_2026-10-03.md`.

## Progress

Not started.
