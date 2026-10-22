// Inspection section: trade. Panel rows for a trade caravan: the value it carries and the settlements it comes from and goes to.
// Settlements have no names, so one is identified by its location.

import { query_entity, type EntityData } from "../../gdo.js"
import type { PanelRow } from "./ui_element_properties_panel.js"

// How a settlement is shown: by location, or the fallback when the reference is not a live settlement (never set, destroyed or its slot reused).
function settlement_text(guid: number, fallback: string): string
{
    const settlement = guid === 0 ? null : query_entity(guid);
    if (settlement === null) return fallback;

    const location = settlement.get_location("location");
    return `Settlement (${location.x}, ${location.y})`;
}

// Settlement trade pool (funds its caravans) and what was set aside into it last day. Own settlements only: the caller decides.
export function trade_pool_section(settlement: EntityData): PanelRow[]
{
    return [
        { kind: "value", id: "trade_pool", state: {
            label: "Trade pool",
            value: `${settlement.get_number("trading_pool")} / ${settlement.get_number("trading_pool_max")}`, // The value row has no max field.
            delta: settlement.get_number("finance_trade_set_aside"),
        } },
    ];
}

export function caravan_section(caravan: EntityData): PanelRow[]
{
    return [
        { kind: "value", id: "caravan_value", state: { label: "Value", value: String(caravan.get_number("value")) } },
        { kind: "text", id: "caravan_origin", state: { label: "Origin", text: settlement_text(caravan.get_number("origin_settlement"), "Lost") } },
        { kind: "text", id: "caravan_destination", state: { label: "Destination", text: settlement_text(caravan.get_number("destination_settlement"), "None") } },
    ];
}
