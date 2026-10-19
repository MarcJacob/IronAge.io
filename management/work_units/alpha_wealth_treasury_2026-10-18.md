# Alpha: wealth and treasury

Started: 2026-10-18. Design: `management/design_doc.md`, Alpha > Settlements > Local Wealth & Construction, Upkeep & Economy.

## Scope

Local wealth per settlement (sources: land output, buildings, caravans), base cost per population, taxation into the owner's single treasury, treasury spending hooks. HUD display and tax control.

## Tasks

Not broken down yet.

## Open questions

- Taxation: player-set rate, button, or automatic?
- Treasury -> local wealth transfer vs taxing: loop?
- Upkeep split: local troops from local wealth, armies from treasury, pool troops unspecified.

## Current state (survey done)

- `local_wealths` exists (settlement SoA, GDO `local_wealth`). Only source: caravan arrivals. No sinks, no readers.
- Land output exists: terrain base output + proximity decay, summed in `land_outputs`; drives target population only.
- Cost hooks are stubs: `raise_army_can_afford` (true), `raise_army_charge` (empty), in match_commands.cpp.
- No treasury / tax / upkeep / buildings / per-player state anywhere in world state (players are only ids on entities).
- No top-level per-player HUD readout; client has no non-entity query.
- Growth: `GROWTH_TICK_PERIOD` 50, staggered by guid. Validity error flags mirrored by hand in input.ts.

## Next step

Decide open questions above, then break into tasks.
