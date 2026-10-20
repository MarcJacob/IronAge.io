// Gauge states for the troops of a settlement (local pool vs ceiling vs max) and of an army (current vs raise target vs max),
// shared by the modes that fill the entity properties panel. Reads entity snapshots only: what a slider does is up to the callback.

import type { EntityData } from "../gdo.js"
import type { GaugeState } from "./ui_element_properties_panel.js"

// Troop types in the order of the commands' target_* properties. name is the suffix of the entity GDO properties.
// Levies come from the peasants, the others from the settlement's local pool.
const TROOPS = [
    { name: "levies", label: "Levies", color: "#c7cbd1" },
    { name: "men_at_arms", label: "Men-at-arms", color: "#4aa3ff" },
    { name: "horsemen", label: "Horsemen", color: "#f0b040" },
    { name: "knights", label: "Knights", color: "#c060e0" },
];

const RAISE_MIN_POPULATION_LEFT = 10; // Mirrors RAISE_MIN_POPULATION_LEFT in the backend (levy max = peasants - this).
const CEILING_SATURATED = 0xFFFF; // Raw ceiling meaning "follows max".
const MAX_COUNT = 0xFFFF; // Counts are ui16.

// Local pool of a settlement, one gauge per pooled type (no levies): current, ceiling slider, max.
// on_ceiling: makes the sliders interactive. Called on release with the troop_type of SET_SETTLEMENT_TROOP_CEILING and the raw ceiling to send.
export function settlement_gauges(settlement: EntityData, on_ceiling?: (troop_type: number, ceiling: number) => void): GaugeState[]
{
    return TROOPS.slice(1).map((troop, i) => {
        const max = settlement.get_number(`max_${troop.name}`);
        return {
            label: troop.label,
            color: troop.color,
            current: settlement.get_number(`troops_${troop.name}`),
            max,
            slider: settlement.get_number(`ceiling_${troop.name}`), // Effective ceiling.
            warn_below_current: true,
            on_change: on_ceiling === undefined ? undefined : (value: number) => on_ceiling(i, value >= max ? CEILING_SATURATED : value),
        };
    });
}

// One gauge per troop type of an army: current counts (zero without army), target slider, max (what the home settlement can give).
// army: null for an army not raised yet. home: null if unknown (max falls back to current).
// targets: the target per type, null for no sliders. on_target: makes the sliders interactive, called on release.
export function army_gauges(army: EntityData | null, home: EntityData | null, targets: number[] | null, on_target?: (troop_index: number, value: number) => void): GaugeState[]
{
    return TROOPS.map((troop, i) => {
        const current = army === null ? 0 : army.get_number(`composition_${troop.name}`);

        let max = current;
        if (home !== null)
        {
            // Pooled types: the home max, or the pool if it holds more (pool above max is kept) so the existing troops can be drawn.
            max = i === 0
                ? Math.max(0, home.get_number("peasants") - RAISE_MIN_POPULATION_LEFT)
                : Math.max(home.get_number(`max_${troop.name}`), home.get_number(`troops_${troop.name}`));
        }
        return {
            label: troop.label,
            color: troop.color,
            current,
            max: Math.min(max, MAX_COUNT),
            slider: targets === null ? undefined : targets[i],
            on_change: targets === null || on_target === undefined ? undefined : (value: number) => on_target(i, value),
        };
    });
}

// One gauge per troop type of an army that is not raising: current counts against max, no target marker or sliders.
// Max is the army's raise target per type (its original counts), or the current count if that is higher so bars never overflow.
export function army_count_gauges(army: EntityData): GaugeState[]
{
    return TROOPS.map(troop => {
        const current = army.get_number(`composition_${troop.name}`);
        const original = army.get_number(`raised_${troop.name}`);
        return { label: troop.label, color: troop.color, current, max: Math.max(original, current) };
    });
}

// The raise targets of an army, in troop order.
export function army_raise_targets(army: EntityData): number[]
{
    return TROOPS.map(troop => army.get_number(`raised_${troop.name}`));
}

// Targets in troop order as the target_* properties of RAISE_ARMY and SET_ARMY_RAISE_TARGETS.
export function raise_target_props(targets: number[]): { target_levies: number, target_men_at_arms: number, target_horsemen: number, target_knights: number }
{
    return { target_levies: targets[0], target_men_at_arms: targets[1], target_horsemen: targets[2], target_knights: targets[3] };
}
