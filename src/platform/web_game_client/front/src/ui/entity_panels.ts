// Properties panel states of the inspected entities, one function per entity type: general stats as rows, troops as gauges (troop_gauges.ts).
// Reads entity snapshots only: what a slider does is up to the callback. A new entity type gets its own function here, called by the mode that shows it.

import type { EntityData } from "../gdo.js"
import type { PanelState, StatRow } from "./ui_element_properties_panel.js"
import { settlement_gauges, army_gauges, army_count_gauges, army_raise_targets } from "./troop_gauges.js"

// Raised armies of a settlement (count against max) and the share of its population they hold. Raised men still count in the population.
export function settlement_raised_rows(settlement: EntityData): StatRow[]
{
    return [
        { label: "Raised armies", value: `${settlement.get_number("raised_armies")} / ${settlement.get_number("max_raised_armies")}` },
        { label: "Raised", value: `${settlement.get_number("raised_percent")}% (${settlement.get_number("raised_men")} men)` },
    ];
}

// Settlement: macro indicators, then the local troop pool (current, ceiling slider, max).
// on_ceiling: makes the ceiling sliders interactive (see settlement_gauges).
export function settlement_panel(settlement: EntityData, on_ceiling?: (troop_type: number, ceiling: number) => void): PanelState
{
    return {
        title: "Settlement",
        stats: [
            { label: "Population", value: settlement.get_number("population") },
            { label: "Wealth", value: settlement.get_number("local_wealth") },
            { label: "Land output", value: settlement.get_number("land_output") },
            { label: "Tier", value: settlement.get_number("tier") },
            ...settlement_raised_rows(settlement),
        ],
        gauges_title: "Local troops",
        slider_name: "ceiling",
        gauges: settlement_gauges(settlement, on_ceiling),
    };
}

// Army: total men and state, then its troops. While raising: current / target slider / max (from the home settlement). Otherwise current counts only.
// home: the home settlement, null if unknown. on_targets: makes the target sliders interactive while raising, called on release with every target.
export function army_panel(army: EntityData, home: EntityData | null, on_targets?: (targets: number[]) => void): PanelState
{
    const raising = army.get_number("raising") === 1;
    const targets = army_raise_targets(army);
    const gauges = raising
        ? army_gauges(army, home, targets, on_targets === undefined ? undefined : (troop, value) => { targets[troop] = value; on_targets?.(targets); })
        : army_count_gauges(army);

    return {
        title: "Army",
        stats: [
            { label: "Men", value: gauges.reduce((sum, gauge) => sum + gauge.current, 0) },
            { label: "State", value: army.get_number("battle") !== 0 ? "In battle" : raising ? "Raising" : "Ready" },
        ],
        gauges_title: "Troops",
        slider_name: raising ? "target" : undefined,
        gauges,
    };
}
