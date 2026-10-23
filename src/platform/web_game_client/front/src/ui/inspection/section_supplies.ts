// Inspection section: army supplies. Panel rows for the supplies an army carries (gauge colored by how full it is, consumption and days left,
// desertion warning), and for the cost of raising an army. Reads entity snapshots only, so it works for any owner.

import { get_game_parameters, get_local_player_state, type EntityData } from "../../gdo.js"
import type { PanelRow } from "./ui_element_properties_panel.js"

// Gauge color stops by how full the supplies are (fraction of capacity): empty side red, middle yellow, full green. Linear in between.
const SUPPLIES_COLOR_STOPS: { at: number, rgb: [number, number, number] }[] = [
    { at: 0.25, rgb: [224, 80, 80] },
    { at: 0.5, rgb: [240, 192, 64] },
    { at: 0.75, rgb: [80, 192, 96] },
];

export function supplies_color(fraction: number): string
{
    const stops = SUPPLIES_COLOR_STOPS;
    let rgb = stops[0].rgb;
    if (fraction >= stops[stops.length - 1].at) rgb = stops[stops.length - 1].rgb;
    else if (fraction > stops[0].at)
    {
        let i = 1;
        while (fraction > stops[i].at) i++;
        const t = (fraction - stops[i - 1].at) / (stops[i].at - stops[i - 1].at);
        rgb = [0, 1, 2].map(c => Math.round(stops[i - 1].rgb[c] + (stops[i].rgb[c] - stops[i - 1].rgb[c]) * t)) as [number, number, number];
    }
    return `rgb(${rgb[0]}, ${rgb[1]}, ${rgb[2]})`;
}

// Supplies of an army: gauge against its capacity, consumption per day with the days the supplies last, and a warning once soldiers deserted or supplies run out.
export function army_supplies_section(army: EntityData): PanelRow[]
{
    const supplies = army.get_number("supplies");
    const capacity = army.get_number("supplies_capacity");
    const consumption = army.get_number("supplies_consumption");
    const desertions = army.get_number("desertions");
    const days = consumption > 0 ? Math.floor(supplies / consumption) : null;

    const rows: PanelRow[] = [
        { kind: "gauge", id: "supplies", state: {
            label: "Supplies",
            color: supplies_color(capacity > 0 ? supplies / capacity : 1),
            current: supplies,
            max: capacity > 0 ? capacity : undefined,
            scale: Math.max(supplies, 1),
        } },
        { kind: "value", id: "supplies_consumption", state: {
            label: "Consumption",
            value: `${consumption} / day`,
            note: days === null ? undefined : `(${days} ${days === 1 ? "day" : "days"} left)`,
            tooltip: [
                "Soldiers consume supplies every day.",
                "Armies refill from the treasury inside their owner's territory.",
                "Missing supplies make soldiers desert.",
            ],
        } },
    ];

    if (desertions > 0)
        rows.push({ kind: "text", id: "supplies_warning", state: { label: "Desertion", text: `${desertions} soldiers deserted last day`, warn: true } });
    else if (consumption > 0 && supplies < consumption)
        rows.push({ kind: "text", id: "supplies_warning", state: { label: "", text: "LOW ON SUPPLIES !", warn: true } });

    return rows;
}

// Daily consumption of the given troops (levies, men-at-arms, horsemen, knights), from the game parameters. Same rule as the simulation.
function troops_consumption(targets: number[]): number
{
    const parameters = get_game_parameters();
    return Math.floor(targets[0] * parameters.get_number("upkeep_levy_per_mille") / 1000)
        + targets[1] * parameters.get_number("upkeep_men_at_arms")
        + targets[2] * parameters.get_number("upkeep_horseman")
        + targets[3] * parameters.get_number("upkeep_knight");
}

// The cost of raising an army with these targets: one day of its troops' consumption, taken from the treasury and put in the army's supplies. Red if the treasury cannot pay it.
export function army_raise_cost_section(targets: number[]): PanelRow[]
{
    const cost = troops_consumption(targets);
    const treasury = get_local_player_state().get_number("treasury");
    return [{ kind: "text", id: "raise_cost", state: { label: "Cost:", text: `${cost} (1 day of supplies)`, warn: cost > treasury } }];
}
