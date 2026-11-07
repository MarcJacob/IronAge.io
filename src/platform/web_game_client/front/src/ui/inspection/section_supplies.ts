// Inspection helpers: army supplies. The gauge color by how full the supplies are (army panel), and the daily consumption of troops
// (cost of raising an army, "Raise army" panel).

import { get_game_parameters } from "../../gdo.js"

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

// Daily consumption of the given troops (levies, men-at-arms, horsemen, knights, wagons), from the game parameters. Same rule as the simulation.
// Raising an army costs a day of it in gold (at most the troops' carry capacity, see raising_cost), taken from the treasury and put in the army's gold.
export function troops_consumption(targets: number[]): number
{
    const parameters = get_game_parameters();
    return Math.floor(targets[0] * parameters.get_number("upkeep_levy_per_mille") / 1000)
        + targets[1] * parameters.get_number("upkeep_men_at_arms")
        + targets[2] * parameters.get_number("upkeep_horseman")
        + targets[3] * parameters.get_number("upkeep_knight")
        + targets[4] * parameters.get_number("upkeep_wagon");
}

// What the given troops carry at most in each pool (supplies, gold), from the game parameters. Same rule as the simulation (army_troops_carry_capacity).
export function troops_carry_capacity(targets: number[]): number
{
    const parameters = get_game_parameters();
    return targets[0] * parameters.get_number("carry_levy")
        + targets[1] * parameters.get_number("carry_men_at_arms")
        + targets[2] * parameters.get_number("carry_horseman")
        + targets[3] * parameters.get_number("carry_knight")
        + targets[4] * parameters.get_number("carry_wagon");
}

// Gold raising an army with these targets costs: a day of their consumption, at most their carry capacity. Same rule as the simulation (army_raising_cost).
export function raising_cost(targets: number[]): number
{
    return Math.min(troops_consumption(targets), troops_carry_capacity(targets));
}
