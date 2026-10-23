// Inspection section: economy. Panel rows for the income, consumption and the population of a settlement, with last day's change beside the value
// and the breakdown of that change in the hover tooltip. Reads entity snapshots only (the economy_* properties are last day's figures), so it works for any owner.

import { get_game_parameters, type EntityData } from "../../gdo.js"
import type { PanelRow } from "./ui_element_properties_panel.js"

// Last day's income (production + trade + pay) with the breakdown of where it goes in the tooltip, and the tax rate taken on it
// (a slider within the game parameters' limits with on_tax_rate, else read-only text).
export function income_section(settlement: EntityData, on_tax_rate?: (tax_rate: number) => void): PanelRow[]
{
    const consumption_budget = settlement.get_number("economy_consumption_budget");
    const trade_release = settlement.get_number("economy_trade_release");
    const trade_overflow = settlement.get_number("economy_trade_overflow");
    const trade_other = settlement.get_number("economy_trade_income") - trade_release - trade_overflow; // E.g. refunds.

    return [
        { kind: "value", id: "income", state: {
            label: "Income",
            value: `${settlement.get_number("economy_income")} / day`,
            tooltip: [
                "Last day",
                `Production +${settlement.get_number("economy_production")}`,
                `Trade (import release) +${trade_release}`,
                `Trade (arrivals) +${trade_overflow}`,
                ...(trade_other > 0 ? [`Trade (other) +${trade_other}`] : []),
                `Worker pay +${settlement.get_number("economy_worker_pay")}`,
                `Soldiers income +${settlement.get_number("economy_soldier_pay")}`,
                `Tax -${settlement.get_number("economy_tax")}`,
                `Trade pool set-aside -${settlement.get_number("economy_trade_set_aside")}`,
                `Available for consumption ${consumption_budget}`,
            ],
        } },
        on_tax_rate !== undefined
            ? { kind: "slider", id: "tax_rate", state: {
                label: "Tax rate",
                value: settlement.get_number("tax_rate"),
                min: get_game_parameters().get_number("tax_rate_min"),
                max: get_game_parameters().get_number("tax_rate_max"),
                unit: "%",
                on_change: on_tax_rate,
            } }
            : { kind: "text", id: "tax_rate", state: { label: "Tax rate", text: `${settlement.get_number("tax_rate")}%` } },
    ];
}

// Last day's consumption budget against what the population requires, with the population growth it produced beside it.
export function consumption_section(settlement: EntityData): PanelRow[]
{
    const budget = settlement.get_number("economy_consumption_budget");
    const required = settlement.get_number("economy_consumption_required");
    const growth = settlement.get_number("population_change");

    return [
        { kind: "value", id: "consumption", state: {
            label: "Consumption",
            value: `${budget} / ${required}`,
            delta: growth,
            tooltip: [
                "Last day",
                `Budget ${budget}`,
                `Required ${required}`,
                `Growth ${growth} per day`,
            ],
        } },
    ];
}

// Home population (the total without the troops raised into armies), its change over the last day, the raised count after it, and who is at home.
export function population_section(settlement: EntityData): PanelRow[]
{
    const peasants = settlement.get_number("peasants");
    // Levies have no home pool (always 0 at home), so the home soldiers are the pooled types.
    const soldiers_home = settlement.get_number("troops_men_at_arms") + settlement.get_number("troops_horsemen") + settlement.get_number("troops_knights");
    const raised = settlement.get_number("raised_levies") + settlement.get_number("raised_men_at_arms")
        + settlement.get_number("raised_horsemen") + settlement.get_number("raised_knights");
    const population = Math.max(0, settlement.get_number("population") - raised);
    const growth_percent = settlement.get_number("population_growth_rate") / 10;

    return [
        { kind: "value", id: "population", state: {
            label: "Population",
            value: String(population),
            delta: settlement.get_number("population_change"),
            note: raised > 0 ? `(+ ${raised} Raised)` : ``,
            tooltip: [
                `Home population ${population}`,
                `Peasants ${peasants}`,
                `Soldiers (at home) ${soldiers_home}`,
                raised > 0 ? `Soldiers (raised) ${raised}` : "",
                `Growth ${growth_percent.toFixed(1)}% per day`,
            ],
        } },
    ];
}
