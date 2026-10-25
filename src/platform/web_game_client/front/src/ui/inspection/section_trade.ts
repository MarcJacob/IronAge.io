// Inspection helper: settlement text (settlements have no names, so one is identified by its location), used by the panels' origin / destination rows.

import { query_entity } from "../../gdo.js"

// How a settlement is shown: by location, or the fallback when the reference is not a live settlement (never set, destroyed or its slot reused).
export function settlement_text(guid: number, fallback: string): string
{
    const settlement = guid === 0 ? null : query_entity(guid);
    if (settlement === null) return fallback;

    const location = settlement.get_location("location");
    return `Settlement (${location.x}, ${location.y})`;
}
