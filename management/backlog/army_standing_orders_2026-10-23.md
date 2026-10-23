# BACKLOG: Army standing orders

Input date: 2026-10-23

## Item

Replace direct army move / target orders with abstract standing orders: an order type plus a data buffer (same mechanism as worker work type + data), executed by a per-type logic function that sets the army's destination or target entity.

## Description

- Minimalist per-army AI: armies decide their own destination / behavior within the order.
- Target entity becomes part of the order data. Helper functions start actions on a target with checks baked in.
- Example order types: Station (wander randomly around an area), Patrol (between two locations / territories), Kill target (pursue, give up after a while).
- All types handle low supplies (see `work_units/army_supplies_2026-10-23.md`), possibly against the owner's wishes.
- UX: select, then left click to give an order; right click = contextual order (Station or Attack / Besiege).
- Goal: more natural, semi-autonomous armies; more intuitive control.
- Related: `work_units/alpha_war_extensions_2026-10-18.md` (accompany orders, direct movement).
