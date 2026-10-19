// Properties panel states of the inspected entities, one function per entity type: general stats as rows, troops as gauges (troop_gauges.ts).
// Reads entity snapshots only: what a slider does is up to the callback. A new entity type gets its own function here, called by the mode that shows it.

import type { EntityData } from "../gdo.js"
import { ARMY_STATE } from "../backend.js"
import type { PanelState, StatRow } from "./ui_element_properties_panel.js"
import { settlement_gauges, army_gauges, army_count_gauges, army_raise_targets, troop_segments } from "./troop_gauges.js"

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

// A warband as the army panel shows it: its snapshot and its home settlement's (null if unknown or lost).
export interface WarbandView
{
    warband: EntityData;
    home: EntityData | null;
}

const ARMY_STATE_TEXT: { [state: number]: string } = {
    [ARMY_STATE.IDLE]: "Idle",
    [ARMY_STATE.MOVING]: "Moving",
    [ARMY_STATE.ATTACKING]: "Attacking",
    [ARMY_STATE.FOLLOWING]: "Accompanying",
    [ARMY_STATE.MERGING]: "Merging",
    [ARMY_STATE.RAISING]: "Raising",
    [ARMY_STATE.IN_BATTLE]: "In battle",
};

// Army: total men, state and warband count. With several warbands: one bar split between the troop types, then the warbands as a selectable list.
// Then the troops of focus as gauges, if any. While focus raises: current / target slider / max (from its home settlement). Otherwise current counts only.
// warbands: the army's warbands in slot order. focus: one of them (the only one, or the one selected), null for no gauges.
// selected / on_toggle: the list selection, by warband guid. on_targets: makes focus's target sliders interactive while it raises, called on release with every target.
export function army_panel(army: EntityData, warbands: WarbandView[], focus: WarbandView | null, selected: Set<number>,
    on_toggle: (warband_guid: number) => void, on_targets?: (targets: number[]) => void): PanelState
{
    const several = warbands.length > 1;
    const panel: PanelState = {
        title: "Army",
        stats: [
            { label: "Men", value: troop_segments(army).reduce((sum, segment) => sum + segment.value, 0) },
            { label: "State", value: ARMY_STATE_TEXT[army.get_number("state")] ?? "Unknown" },
            { label: "Warbands", value: warbands.length },
        ],
    };

    if (several)
    {
        panel.split_bar = troop_segments(army);
        panel.list_title = "Warbands (click to select)";
        panel.list = warbands.map((view, slot) => ({
            text: warband_row_text(view, slot),
            segments: troop_segments(view.warband),
            selected: selected.has(view.warband.guid),
            on_click: () => on_toggle(view.warband.guid),
        }));
    }

    if (focus !== null)
    {
        const raising = focus.warband.get_number("raising") === 1;
        const targets = army_raise_targets(focus.warband);
        panel.gauges_title = several ? `Warband ${warbands.indexOf(focus) + 1} troops` : "Troops";
        panel.slider_name = raising ? "target" : undefined;
        panel.gauges = raising
            ? army_gauges(focus.warband, focus.home, targets, on_targets === undefined ? undefined : (troop, value) => { targets[troop] = value; on_targets?.(targets); })
            : army_count_gauges(focus.warband);
    }
    return panel;
}

// "2. 120 men, from (34, 51), raising": slot number, men, home settlement location.
function warband_row_text(view: WarbandView, slot: number): string
{
    const men = troop_segments(view.warband).reduce((sum, segment) => sum + segment.value, 0);
    const home = view.home !== null ? view.home.get_location("location") : null;
    return `${slot + 1}. ${men} men, ` + (home !== null ? `from (${home.x}, ${home.y})` : "no home")
        + (view.warband.get_number("raising") === 1 ? ", raising" : "");
}
