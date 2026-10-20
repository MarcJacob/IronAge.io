// UI element: the player state panel, bottom left, always visible during play (container in index.html, rows built here).
// Shows facts about the local player, one row per fact (label + value, and optionally a change next to it), all read by property name from the
// player state GDO (world_player_state, see gdo.ts get_local_player_state). Pure display, emits nothing.
// To add a row, add one line to ROWS reading its property, e.g. { label: "Income", big: false, value: (state) => String(state.get_number("income")) }
// (new properties appear in the GDO by adding them to world_player_state in world.h). Add a delta to show a signed change next to the value.
// submit_player_state can be called every frame (the DOM is only touched when a text or tone changes); null clears it.

import type { GDOData } from "../gdo.js"

type Tone = "up" | "down" | "flat";

// A signed per-day change: "+N / day" green, "-N / day" red, grey at 0. get_number gives the I32 property already signed.
function per_day_delta(change: number): { text: string, tone: Tone }
{
    if (change > 0) return { text: `+${change} / day`, tone: "up" };
    if (change < 0) return { text: `${change} / day`, tone: "down" };
    return { text: "+0 / day", tone: "flat" };
}

// One row per displayed fact, top to bottom. big: large type (the headline figure).
// value: the row's text. delta: optional text + color shown next to the value.
const ROWS: { label: string, big: boolean, value: (state: GDOData) => string, delta?: (state: GDOData) => { text: string, tone: Tone } }[] = [
    { label: "Gold", big: true, value: (state) => String(state.get_number("treasury")), delta: (state) => per_day_delta(state.get_number("income") - state.get_number("expenses")) },
    { label: "Settlements", big: false, value: (state) => String(state.get_number("settlement_count")) },
    { label: "Pop", big: false, value: (state) => String(state.get_number("total_population")) },
    { label: "Soldiers", big: false, value: (state) => String(state.get_number("total_troops")) },
];

let panelElement: HTMLElement | null = null;
let valueElements: HTMLElement[] = [];
let deltaElements: (HTMLElement | null)[] = [];

export function init_ui_element_player_state(): void
{
    panelElement = document.getElementById("game_hud_player_state") as HTMLElement;
    panelElement.replaceChildren();
    valueElements = [];
    deltaElements = [];
    for (const row of ROWS) {
        const root = document.createElement("div");
        root.className = "player_state_row" + (row.big ? " big" : "");
        const label = document.createElement("span");
        label.className = "player_state_label";
        label.textContent = row.label;
        const line = document.createElement("div");
        line.className = "player_state_line";
        const value = document.createElement("span");
        value.className = "player_state_value";
        line.appendChild(value);
        let delta: HTMLElement | null = null;
        if (row.delta !== undefined) {
            delta = document.createElement("span");
            delta.className = "player_state_delta";
            line.appendChild(delta);
        }
        root.append(label, line);
        panelElement.appendChild(root);
        valueElements.push(value);
        deltaElements.push(delta);
    }
}

// Makes the panel show the given state. Null clears it (hidden).
export function submit_player_state(state: GDOData | null): void
{
    if (panelElement === null) return;

    if (state === null)
    {
        if (!panelElement.hidden) panelElement.hidden = true;
        return;
    }

    panelElement.hidden = false;
    ROWS.forEach((row, i) => {
        const text = row.value(state);
        if (valueElements[i].textContent !== text) valueElements[i].textContent = text;

        const deltaElement = deltaElements[i];
        if (row.delta !== undefined && deltaElement !== null) {
            const delta = row.delta(state);
            if (deltaElement.textContent !== delta.text) deltaElement.textContent = delta.text;
            const className = "player_state_delta " + delta.tone;
            if (deltaElement.className !== className) deltaElement.className = className;
        }
    });
}
