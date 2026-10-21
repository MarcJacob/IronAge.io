// Inspection section: basics. Text rows for what any entity can show: its owner and its location, when it has them
// (or when the entity it is tied to, its origin_entity, has them: workers).

import { query_entity, type EntityData } from "../../gdo.js"
import type { PanelRow } from "./ui_element_properties_panel.js"

function has_property(entity: EntityData, name: string): boolean
{
    return entity.properties.some(property => property.name === name);
}

export function basic_section(entity: EntityData): PanelRow[]
{
    const rows: PanelRow[] = [];
    const origin = has_property(entity, "origin_entity") ? query_entity(entity.get_number("origin_entity")) : null;

    const owner_source = has_property(entity, "owner") ? entity : origin;
    if (owner_source !== null && has_property(owner_source, "owner"))
        rows.push({ kind: "text", id: "owner", state: { label: "Owner", text: `Player ${owner_source.get_number("owner")}` } });

    const location_source = has_property(entity, "location") ? entity : origin;
    if (location_source !== null && has_property(location_source, "location"))
    {
        const location = location_source.get_location("location");
        rows.push({ kind: "text", id: "location", state: { label: "Location", text: `(${location.x}, ${location.y})` } });
    }
    return rows;
}
