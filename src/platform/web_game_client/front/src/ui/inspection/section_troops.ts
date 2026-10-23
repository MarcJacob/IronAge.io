// Inspection section: troops. Builders of panel rows for the troops of a settlement (local pool vs ceiling vs max) and of an army
// (current vs raise target vs max), shared by the inspectors. Reads entity snapshots only: what a slider does is up to the callback.
// A callback left undefined makes its sliders read-only.

import { get_game_parameters } from "../../gdo.js"
import type { EntityData } from "../../gdo.js"
import type { PanelRow } from "./ui_element_properties_panel.js"

// Troop types in the order of the commands' target_* properties. name is the suffix of the entity GDO properties.
// Levies come from the peasants, the others from the settlement's local pool.
const TROOPS = [
    { name: "levies", label: "Levies", color: "#c7cbd1" },
    { name: "men_at_arms", label: "Men-at-arms", color: "#4aa3ff" },
    { name: "horsemen", label: "Horsemen", color: "#f0b040" },
    { name: "knights", label: "Knights", color: "#c060e0" },
];

const CEILING_SATURATED = 0xFFFF; // Raw ceiling meaning "follows max".
const MAX_COUNT = 0xFFFF; // Counts are ui16.

// Local pool of a settlement, one gauge per pooled type (no levies): current (at home), raised (in armies), ceiling slider, max.
// A type with no max, nobody at home and nobody raised is left out (nothing to show or set yet).
// on_ceiling: called on release with the troop_type of SET_SETTLEMENT_TROOP_CEILING and the raw ceiling to send.
export function settlement_pool_section(settlement: EntityData, on_ceiling?: (troop_type: number, ceiling: number) => void): PanelRow[]
{
    const rows: PanelRow[] = [];
    TROOPS.slice(1).forEach((troop, i) => {
        const max = settlement.get_number(`max_${troop.name}`);
        if (max === 0 && settlement.get_number(`troops_${troop.name}`) === 0 && settlement.get_number(`raised_${troop.name}`) === 0) return;

        rows.push({ kind: "gauge", id: `troops_${troop.name}`, state: {
            label: troop.label,
            color: troop.color,
            current: settlement.get_number(`troops_${troop.name}`), // At home.
            raised: settlement.get_number(`raised_${troop.name}`), // In armies.
            max,
            slider: settlement.get_number(`ceiling_${troop.name}`), // Effective ceiling, on the total (home + raised).
            slider_name: "ceiling",
            warn_below_current: true,
            on_change: on_ceiling === undefined ? undefined : (value: number) => on_ceiling(i, value >= max ? CEILING_SATURATED : value),
        } });
    });
    return rows;
}

// The raise targets of an army, in troop order.
export function army_raise_targets(army: EntityData): number[]
{
    return TROOPS.map(troop => army.get_number(`raised_${troop.name}`));
}

// One gauge per troop type of an army being raised: current counts (zero without army), target slider, max (what the home settlement can give now).
// A type with nothing available, no troops in the army and no target is left out (nothing to set).
// army: null for an army not raised yet. home: null if unknown (max falls back to current). targets: the target per type, in troop order.
// on_targets: called on release with all the targets, the released one updated.
export function army_raise_section(army: EntityData | null, home: EntityData | null, targets: number[], on_targets?: (targets: number[]) => void): PanelRow[]
{
    const rows: PanelRow[] = [];
    TROOPS.forEach((troop, i) => {
        const current = army === null ? 0 : army.get_number(`composition_${troop.name}`);

        let max = current;
        if (home !== null)
        {
            // What the settlement can give: the free peasants above the population kept back for levies, the troops in the local pool for the others.
            max = i === 0
                ? Math.max(0, home.get_number("peasants") - get_game_parameters().get_number("raise_min_population_left"))
                : home.get_number(`troops_${troop.name}`);
        }
        if (max === 0 && current === 0 && targets[i] === 0) return;

        rows.push({ kind: "gauge", id: `army_${troop.name}`, state: {
            label: troop.label,
            color: troop.color,
            current,
            max: Math.min(max, MAX_COUNT),
            slider: targets[i],
            slider_name: "target",
            compact: true,
            on_change: on_targets === undefined ? undefined : (value: number) => {
                const updated = targets.slice();
                updated[i] = value;
                on_targets(updated);
            },
        } });
    });
    return rows;
}

// One gauge per troop type of an army that is not raising: current counts against max, no target marker or sliders.
// Max is the army's raise target per type (its original counts), or the current count if that is higher so bars never overflow.
export function army_count_section(army: EntityData): PanelRow[]
{
    return TROOPS.map((troop): PanelRow => {
        const current = army.get_number(`composition_${troop.name}`);
        const original = army.get_number(`raised_${troop.name}`);
        return { kind: "gauge", id: `army_${troop.name}`, state: { label: troop.label, color: troop.color, current, max: Math.max(original, current), losses: Math.max(original - current, 0) } };
    });
}
