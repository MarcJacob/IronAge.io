// Inspection section: economy. Panel rows for the wealth and the population of a settlement, with last day's change beside the value
// and the breakdown of that change in the hover tooltip. Reads entity snapshots only (the finance_* properties are last day's figures), so it works for any owner.

import { get_game_parameters, type EntityData } from "../../gdo.js"
import type { PanelRow } from "./ui_element_properties_panel.js"

// Local wealth, its change over the last day, and the tax rate (a slider within the game parameters' limits with on_tax_rate, else read-only text).
export function wealth_section(settlement: EntityData, on_tax_rate?: (tax_rate: number) => void): PanelRow[]
{
    const income = settlement.get_number("finance_income");
    const tax_paid = settlement.get_number("finance_tax_paid");
    const consumption_paid = settlement.get_number("finance_consumption_paid");

    return [
        { kind: "value", id: "wealth", state: {
            label: "Wealth",
            value: String(settlement.get_number("local_wealth")),
            delta: income - tax_paid - consumption_paid,
            tooltip: [
                "Last day",
                `Income +${income} (effective output ${settlement.get_number("finance_effective_output")})`,
                `Tax paid -${tax_paid}`,
                `Consumption paid -${consumption_paid}`,
                `Excess ${settlement.get_number("finance_excess")}`,
            ],
        } },
        on_tax_rate !== undefined
            ? { kind: "slider", id: "tax_rate", state: {
                label: "Tax rate",
                value: settlement.get_number("tax_rate"),
                min: get_game_parameters().tax_rate_min,
                max: get_game_parameters().tax_rate_max,
                unit: "%",
                on_change: on_tax_rate,
            } }
            : { kind: "text", id: "tax_rate", state: { label: "Tax rate", text: `${settlement.get_number("tax_rate")}%` } },
    ];
}

// Total population, its change over the last day, and who it is made of.
export function population_section(settlement: EntityData): PanelRow[]
{
    const population = settlement.get_number("population");
    const peasants = settlement.get_number("peasants");
    // Levies have no home pool (always 0 at home), so the home soldiers are the pooled types.
    const soldiers_home = settlement.get_number("troops_men_at_arms") + settlement.get_number("troops_horsemen") + settlement.get_number("troops_knights");
    // Not a property of its own: the total is peasants + soldiers at home + soldiers raised.
    const soldiers_raised = Math.max(0, population - peasants - soldiers_home);
    const growth_percent = settlement.get_number("finance_growth_per_mille") / 10;

    return [
        { kind: "value", id: "population", state: {
            label: "Population",
            value: String(population),
            delta: settlement.get_number("finance_population_change"),
            tooltip: [
                `Peasants ${peasants}`,
                `Soldiers at home ${soldiers_home}`,
                `Soldiers raised ${soldiers_raised}`,
                `Growth ${growth_percent.toFixed(1)}% per day`,
                `Consumption ${settlement.get_number("finance_consumption_paid")} paid / ${settlement.get_number("finance_consumption_required")} required`,
            ],
        } },
    ];
}
