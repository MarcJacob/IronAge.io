// UI module (new style): the in-match HUD overlay root (#game_hud) and its debug entity panel (name = value rows of the selected entity).
// Static markup in index.html (ids below), extracted once by init. submit(state) shows the HUD, submit(null) hides the whole HUD (and so every element in it).
// Emits "toggle_debug" when the debug panel header is clicked; the driver mode (main_play.ts) owns the collapsed flag, since the hotkey toggles it too.
// submit can be called every frame (the DOM is only touched when something changes).

import * as Backend from "../backend.js"
import { GAME_DATA_PROP_TYPE, type EntityData } from "../gdo.js"
import { UiEvents } from "./ui_events.js"

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud",
    debug_panel: "game_hud_selected_panel",
    debug_header: "game_hud_selected_header",
    debug_text: "game_hud_selected_text",
};

export interface GameUiState
{
    debug_text: string | null; // Text of the debug panel. Null hides the panel.
    debug_collapsed: boolean; // Collapsed = only the header shows.
}

const events = new UiEvents<{
    toggle_debug: () => void;
}>();
export const on = events.on.bind(events);

let el: {
    root: HTMLElement;
    debug_panel: HTMLElement;
    debug_header: HTMLElement;
    debug_text: HTMLElement;
} | null = null;

function init(): void
{
    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
        debug_panel: document.getElementById(IDS.debug_panel) as HTMLElement,
        debug_header: document.getElementById(IDS.debug_header) as HTMLElement,
        debug_text: document.getElementById(IDS.debug_text) as HTMLElement,
    };
    el.debug_header.addEventListener("click", () => events.emit("toggle_debug"));
}

export function submit(state: GameUiState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    el.debug_panel.hidden = state.debug_text === null;
    if (state.debug_text === null) return;

    el.debug_text.hidden = state.debug_collapsed;
    set_text(el.debug_header, (state.debug_collapsed ? "[+] " : "[-] ") + "Debug");
    set_text(el.debug_text, state.debug_text);
}

// Builds the 'name = value' rows of an entity: every property of its Game Data Object, raw values, C++ property names.
export function entity_text(entity: EntityData): string
{
    const rows: [string, string | number][] = [
        ["entity_type", Backend.ENTITY_TYPE[entity.entity_type]],
        ["guid", "0x" + entity.guid.toString(16)],
    ];
    for (const property of entity.properties)
    {
        let value: string | number;
        if (property.items !== undefined) value = `[${property.items.map(item => typeof item === "number" ? item : `(${item.x}, ${item.y})`).join(", ")}]`;
        else if (typeof property.value !== "number") value = `(${property.value.x}, ${property.value.y})`;
        else if (property.type === GAME_DATA_PROP_TYPE.ENTITY_GUID) value = "0x" + property.value.toString(16);
        else value = property.value;
        rows.push([property.name, value]);
    }

    return rows.map(([name, value]) => `${name} = ${value}`).join('\n');
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

init();
