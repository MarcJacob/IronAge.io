// UI element: the entity properties panel (container in index.html, content built here).
// Shows general information about what is selected, as a title and an ordered list of rows. A row is one reusable element
// (gauge: ui_element_gauge.ts, text: ui_element_text_row.ts, value with delta and hover tooltip: ui_element_value_row.ts). Pure display: rows only emit their own callbacks.
// A mode keeps its callbacks, submits the state with submit_properties_panel (every frame is fine: the panel updates in place) and submits null to clear it.
// Rows are matched across submits by key (kind + id): the same key is updated in place (a slider mid-drag survives), a new key is built, a missing one is removed.
// Rows may be put in an area of the panel (PanelRow.area, in_area): main (no area, full width on top), left and right (halves of one line), bottom (full width).
// To add a row type: add its state type and element file, add a case in PanelRow and in make_row / update_row below.

import { GaugeElement, type GaugeState } from "./ui_element_gauge.js"
import { TextRowElement, type TextRowState } from "./ui_element_text_row.js"
import { ValueRowElement, type ValueRowState } from "./ui_element_value_row.js"
import { SliderRowElement, type SliderRowState } from "./ui_element_slider_row.js"
import { BuildingRowElement, type BuildingRowState } from "./ui_element_building_row.js"

// Where a row goes in the panel. Rows without an area go in the main part, full width at the top. Then "left" and "right" share a line (halves), and "bottom" is full width below.
export type PanelArea = "left" | "right" | "bottom";

export type PanelRow = (
    | { kind: "gauge", id: string, state: GaugeState }
    | { kind: "text", id: string, state: TextRowState }
    | { kind: "value", id: string, state: ValueRowState }
    | { kind: "slider", id: string, state: SliderRowState }
    | { kind: "building", id: string, state: BuildingRowState }
) & { area?: PanelArea };

// Puts the rows in an area of the panel (convenience for the inspectors composing sections).
export function in_area(area: PanelArea, rows: PanelRow[]): PanelRow[]
{
    return rows.map(row => ({ ...row, area }));
}

export interface PropertiesPanelState
{
    title: string;
    rows: PanelRow[];
}

type RowElement = { kind: "gauge", element: GaugeElement } | { kind: "text", element: TextRowElement } | { kind: "value", element: ValueRowElement }
    | { kind: "slider", element: SliderRowElement } | { kind: "building", element: BuildingRowElement };

let panelElement: HTMLElement | null = null;
let titleElement: HTMLElement | null = null;
let rowsElement: HTMLElement | null = null;

let built = new Map<string, RowElement>(); // By row key.
let areas = new Map<string, HTMLElement>(); // Containers of the rows, by area name ("main" for rows without one). Built when first needed, laid out by the CSS (panel_area_*).

// The container of an area, created inside the rows element if it doesn't exist yet.
function get_area(name: string): HTMLElement
{
    let area = areas.get(name);
    if (area === undefined)
    {
        area = document.createElement("div");
        area.className = "panel_area panel_area_" + name;
        rowsElement!.appendChild(area);
        areas.set(name, area);
    }
    return area;
}

export function init_ui_element_properties_panel(): void
{
    panelElement = document.getElementById("game_hud_properties_panel") as HTMLElement;
    titleElement = document.getElementById("game_hud_properties_title") as HTMLElement;
    rowsElement = document.getElementById("game_hud_properties_rows") as HTMLElement;
}

function make_row(row: PanelRow): RowElement
{
    switch (row.kind)
    {
    case "gauge": return { kind: "gauge", element: new GaugeElement() };
    case "text": return { kind: "text", element: new TextRowElement() };
    case "value": return { kind: "value", element: new ValueRowElement() };
    case "slider": return { kind: "slider", element: new SliderRowElement() };
    case "building": return { kind: "building", element: new BuildingRowElement() };
    }
}

function update_row(built_row: RowElement, row: PanelRow): void
{
    if (built_row.kind === "gauge" && row.kind === "gauge") built_row.element.submit(row.state);
    else if (built_row.kind === "text" && row.kind === "text") built_row.element.submit(row.state);
    else if (built_row.kind === "value" && row.kind === "value") built_row.element.submit(row.state);
    else if (built_row.kind === "slider" && row.kind === "slider") built_row.element.submit(row.state);
    else if (built_row.kind === "building" && row.kind === "building") built_row.element.submit(row.state);
}

// Value and building rows may be showing the tooltip: hide it with them.
function dispose_rows(rows: Iterable<RowElement>): void
{
    for (const row of rows) if (row.kind === "value" || row.kind === "building") row.element.dispose();
}

// Makes the panel show the given state. Null clears it (hidden).
export function submit_properties_panel(state: PropertiesPanelState | null): void
{
    if (panelElement === null || titleElement === null || rowsElement === null) return;

    if (state === null)
    {
        if (!panelElement.hidden) panelElement.hidden = true;
        if (built.size > 0)
        {
            dispose_rows(built.values());
            rowsElement.replaceChildren();
            built.clear();
            areas.clear();
        }
        return;
    }

    panelElement.hidden = false;
    if (titleElement.textContent !== state.title) titleElement.textContent = state.title;

    // Rows no longer present go away.
    const keys = new Set(state.rows.map(row => row.kind + ":" + row.id));
    for (const [key, built_row] of built)
    {
        if (keys.has(key)) continue;
        dispose_rows([built_row]);
        built_row.element.root.remove();
        built.delete(key);
    }

    const placed = new Map<string, number>(); // Rows placed so far in each area.
    state.rows.forEach((row) => {
        const key = row.kind + ":" + row.id;
        let built_row = built.get(key);
        if (built_row === undefined)
        {
            built_row = make_row(row);
            built.set(key, built_row);
        }

        // Only moves the DOM node when its place changed (moving a slider mid-drag would interrupt it).
        const area_name = row.area ?? "main";
        const area = get_area(area_name);
        const index = placed.get(area_name) ?? 0;
        placed.set(area_name, index + 1);
        const root = built_row.element.root;
        if (area.children[index] !== root) area.insertBefore(root, area.children[index] ?? null);

        update_row(built_row, row);
    });
}
