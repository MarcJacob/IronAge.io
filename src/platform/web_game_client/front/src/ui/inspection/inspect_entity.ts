// Entity inspection: the entry point modes call to fill the properties panel for a selected entity.
//   submit_properties_panel(inspect_entity(view, context))
// The inspector of the entity's type composes sections (section_*.ts, each a function returning panel rows) into a title and a row list.
// Display only: whether the local player may act on the entity is decided by the caller, which passes callbacks in the context
// only for what it allows (without a callback the matching sliders are read-only). Everything inspection-related lives in this folder.
// To add a section: write a function returning PanelRow[] in a section_*.ts file and call it from the inspectors below.
// To give an entity type its own inspector: write a function (view, context) => PropertiesPanelState and add it to the switch in inspect_entity.

import * as Backend from "../../backend.js"
import { query_entity, type EntityData } from "../../gdo.js"
import { in_area, type PropertiesPanelState } from "./ui_element_properties_panel.js"
import { basic_section } from "./section_basic.js"
import { production_section,consumption_section, population_section } from "./section_economy.js"
import { settlement_pool_section, army_raise_section, army_count_section, army_raise_targets } from "./section_troops.js"
import { army_supplies_section, army_raise_cost_section } from "./section_supplies.js"
import { construction_section } from "./section_construction.js"
import { caravan_section, trade_pool_section } from "./section_trade.js"

export interface InspectionContext
{
    // Settlement: makes the ceiling sliders interactive. Called on release with the troop_type of SET_SETTLEMENT_TROOP_CEILING and the raw ceiling to send.
    on_ceiling?: (troop_type: number, ceiling: number) => void;

    // Settlement: turns the tax rate row into a slider. Called on release with the tax_rate of SET_SETTLEMENT_TAX_RATE to send.
    on_tax_rate?: (tax_rate: number) => void;

    // Settlement: gives the building rows a button starting a project of the given building type (row of the building table).
    on_build_building?: (building_type: number) => void;

    // Settlement: whether a project of the building type would be accepted now (the button is disabled otherwise). Without it the buttons are always enabled.
    can_build_building?: (building_type: number) => boolean;

    // Settlement: shows the troops an army raised from it would have ("Raise army") against these targets instead of its own troops.
    raise_targets?: number[];

    // Army being raised (or settlement with raise_targets): makes the target sliders interactive. Called on release with all the targets, the released one updated.
    on_raise_targets?: (targets: number[]) => void;
}

function inspect_settlement(view: EntityData, context: InspectionContext): PropertiesPanelState
{
    if (context.raise_targets !== undefined)
        return { title: "Raise army", rows: [
            ...army_raise_section(null, view, context.raise_targets, context.on_raise_targets),
            ...army_raise_cost_section(context.raise_targets),
        ] };

    // Top left: population then economy. Top right: troops. Bottom: buildings.
    return { title: "Settlement", rows: [
        ...in_area("left", [...population_section(view), ...production_section(view, context.on_tax_rate),
            // Own settlements only: on_tax_rate is given only for those.
            ...(context.on_tax_rate !== undefined ? [...consumption_section(view), ...trade_pool_section(view)] : [])]),
        ...in_area("right", settlement_pool_section(view, context.on_ceiling)),
        ...in_area("bottom", construction_section(view, context.on_build_building, context.can_build_building)),
    ] };
}

function inspect_army(view: EntityData, context: InspectionContext): PropertiesPanelState
{
    // While raising: current / target slider / max. Otherwise just the current counts (no max, no target).
    if (view.get_number("raising") === 1)
        return { title: "Army troops", rows: [
            ...army_raise_section(view, query_entity(view.get_number("home_settlement")), army_raise_targets(view), context.on_raise_targets),
            ...army_supplies_section(view),
        ] };

    return { title: "Army troops", rows: [...army_count_section(view), ...army_supplies_section(view)] };
}

function inspect_caravan(view: EntityData): PropertiesPanelState
{
    return { title: "Caravan", rows: caravan_section(view) };
}

// Entity types without sections of their own (worker, battle...).
function inspect_generic(view: EntityData): PropertiesPanelState
{
    const name = Backend.ENTITY_TYPE[view.entity_type] ?? "Entity";
    return { title: name.charAt(0) + name.slice(1).toLowerCase(), rows: basic_section(view) };
}

export function inspect_entity(view: EntityData, context: InspectionContext): PropertiesPanelState
{
    switch (view.entity_type)
    {
    case Backend.ENTITY_TYPE.SETTLEMENT: return inspect_settlement(view, context);
    case Backend.ENTITY_TYPE.ARMY: return inspect_army(view, context);
    case Backend.ENTITY_TYPE.CARAVAN: return inspect_caravan(view);
    default: return inspect_generic(view);
    }
}
