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

// Settlement export pool (funds its caravans) and import pool (receives caravan revenue), each against its maximum, as read-only gauges (no on_change, so no slider).
// Gauges have no tooltip support. Own settlements only: the caller decides.
export function trade_pool_section(settlement: EntityData): PanelRow[]
{
    return [
        { kind: "gauge", id: "export_pool", state: {
            label: "Export pool",
            color: "#d4a93c",
            current: settlement.get_number("trading_export_pool"),
            max: settlement.get_number("trading_export_pool_max"),
            compact: true,
        } },
        { kind: "gauge", id: "import_pool", state: {
            label: "Import pool",
            color: "#4fa3a8",
            current: settlement.get_number("trading_import_pool"),
            max: settlement.get_number("trading_import_pool_max"),
            compact: true,
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
