// In-match UI (HUD overlay on top of the canvas). An extension of input.ts: it works from input.ts's state (selection).
// The debug entity inspection panel (no actions), and the per-frame feed of the calendar and treasury elements (world state, not selection).
// DOM is only touched when the text / visibility actually changed.

import * as Backend from "../backend.js"
import { get_selected_entity_view } from "../input.js"
import { GAME_DATA_PROP_TYPE, get_game_parameters, type EntityData } from "../gdo.js"
import { submit_calendar } from "./ui_element_calendar.js"
import { submit_treasury } from "./ui_element_treasury.js"

let selectedPanelElement: HTMLElement | null = null;
let selectedTextElement: HTMLElement | null = null;
let selectedHeaderElement: HTMLElement | null = null;
let debugPanelCollapsed = false;

export function init_game_ui(): void
{
    selectedPanelElement = document.getElementById("game_hud_selected_panel") as HTMLElement,
    selectedTextElement = document.getElementById("game_hud_selected_text") as HTMLElement;
    selectedHeaderElement = document.getElementById("game_hud_selected_header") as HTMLElement;
    selectedHeaderElement.addEventListener("click", toggle_debug_panel);
    refresh_debug_panel_collapse();
}

function refresh_debug_panel_collapse(): void
{
    if (selectedTextElement === null || selectedHeaderElement === null) return;

    selectedTextElement.hidden = debugPanelCollapsed;
    selectedHeaderElement.textContent = (debugPanelCollapsed ? "[+] " : "[-] ") + "Debug";
}

// Collapses / expands the debug entity text (the header stays). Called by the header click and by the hotkey in the play mode.
export function toggle_debug_panel(): void
{
    debugPanelCollapsed = !debugPanelCollapsed;
    refresh_debug_panel_collapse();
}

// Shows / hides the whole HUD overlay.
export function set_game_ui_visible(visible: boolean): void
{
    const hud = document.getElementById("game_hud") as HTMLElement;
    hud.hidden = !visible;
}

// Sets an element's hidden state, only if it differs.
function set_hidden(element: HTMLElement, hidden: boolean): void
{
    if (element.hidden !== hidden) element.hidden = hidden;
}

// Builds the 'name = value' rows of an entity: every property of its Game Data Object, raw values, C++ property names.
function entity_text(entity: EntityData): string
{
    const rows: [string, string | number][] = [
        ["entity_type", Backend.ENTITY_TYPE[entity.entity_type]],
        ["guid", "0x" + entity.guid.toString(16)],
    ];
    for (const property of entity.properties)
    {
        let value: string | number;
        if (typeof property.value !== "number") value = `(${property.value.x}, ${property.value.y})`;
        else if (property.type === GAME_DATA_PROP_TYPE.ENTITY_GUID) value = "0x" + property.value.toString(16);
        else value = property.value;
        rows.push([property.name, value]);
    }

    return rows.map(([name, value]) => `${name} = ${value}`).join('\n');
}

// Calendar: the day is the world age in ticks over ticks_per_day, shown from day 1. Time left comes from the match's tick rate.
function update_calendar(): void
{
    const ticks_per_day = get_game_parameters().ticks_per_day;
    const tick_rate = Backend.get_match_tick_rate();
    if (ticks_per_day === 0 || tick_rate === 0) { submit_calendar(null); return; }

    const age = Backend.get_world_age();
    const tick_in_day = age % ticks_per_day;
    submit_calendar({
        day: Math.floor(age / ticks_per_day) + 1,
        progress: tick_in_day / ticks_per_day,
        seconds_left: (ticks_per_day - tick_in_day) / tick_rate,
    });
}

// Per-frame refresh. Call once the match is running.
export function update_game_ui(): void
{
    update_calendar();
    submit_treasury(Backend.get_local_player_treasury());

    if (selectedPanelElement === null || selectedTextElement === null) return;

    // Re-query the selected entity every frame so its values stay live. Nothing to show if there is none or it has no Game Data Object.
    const selected = get_selected_entity_view();
    const text = selected !== null ? entity_text(selected) : null;

    set_hidden(selectedPanelElement, text === null);
    if (text === null) return;

    if (selectedTextElement.textContent !== text) selectedTextElement.textContent = text;
}
