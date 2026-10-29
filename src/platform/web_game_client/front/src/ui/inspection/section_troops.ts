// Inspection helpers: troops. The troop types and what a settlement can give to an army being raised from it, shared by the panel state builders
// (inspect_entity.ts). Reads entity snapshots only.

import type { EntityData } from "../../gdo.js"

// Troop types in the order of the commands' target_* properties. name is the suffix of the entity GDO properties.
// All types, levies included, come from the settlement's garrison (local pool).
export const TROOPS = [
    { name: "levies", label: "Levies", color: "#c7cbd1" },
    { name: "men_at_arms", label: "Men-at-arms", color: "#4aa3ff" },
    { name: "horsemen", label: "Horsemen", color: "#f0b040" },
    { name: "knights", label: "Knights", color: "#c060e0" },
];

export const MAX_COUNT = 0xFFFF; // Counts are ui16.

// What a settlement can give to an army of the troop type (index in TROOPS) now: the troops of that type in its garrison (levies included).
export function raise_available(home: EntityData, troop_index: number): number
{
    return home.get_number(`troops_${TROOPS[troop_index].name}`);
}
