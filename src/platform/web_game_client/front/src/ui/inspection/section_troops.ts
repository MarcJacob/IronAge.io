// Inspection helpers: troops. The troop types and what a settlement can give to an army being raised from it, shared by the panel state builders
// (inspect_entity.ts). Reads entity snapshots only.

import { get_game_parameters } from "../../gdo.js"
import type { EntityData } from "../../gdo.js"

// Troop types in the order of the commands' target_* properties. name is the suffix of the entity GDO properties.
// Levies come from the peasants, the others from the settlement's local pool.
export const TROOPS = [
    { name: "levies", label: "Levies", color: "#c7cbd1" },
    { name: "men_at_arms", label: "Men-at-arms", color: "#4aa3ff" },
    { name: "horsemen", label: "Horsemen", color: "#f0b040" },
    { name: "knights", label: "Knights", color: "#c060e0" },
];

export const MAX_COUNT = 0xFFFF; // Counts are ui16.

// What a settlement can give to an army of the troop type (index in TROOPS) now: the free peasants above the population kept back for levies,
// the troops in the local pool for the others.
export function raise_available(home: EntityData, troop_index: number): number
{
    return troop_index === 0
        ? Math.max(0, home.get_number("pop_peasants") - get_game_parameters().get_number("raise_min_population_left"))
        : home.get_number(`troops_${TROOPS[troop_index].name}`);
}
