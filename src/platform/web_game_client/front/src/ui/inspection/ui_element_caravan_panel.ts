// UI module (new style): the caravan panel, inside the entity frame (ui_element_entity_frame.ts). The value a trade caravan carries
// and the settlements it comes from and goes to. Static markup in index.html (ids below), extracted once by init.
// submit(state) shows it, submit(null) hides it. Emits nothing.

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_caravan_panel",
    value: "caravan_panel_value",
    origin: "caravan_panel_origin",
    destination: "caravan_panel_destination",
};

export interface CaravanPanelState
{
    value: number;
    origin: string; // Origin settlement, as text.
    destination: string; // Destination settlement, as text.
}

let el: {
    root: HTMLElement;
    value: HTMLElement;
    origin: HTMLElement;
    destination: HTMLElement;
} | null = null;

function init(): void
{
    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
        value: document.getElementById(IDS.value) as HTMLElement,
        origin: document.getElementById(IDS.origin) as HTMLElement,
        destination: document.getElementById(IDS.destination) as HTMLElement,
    };
}

export function submit(state: CaravanPanelState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    set_text(el.value, String(state.value));
    set_text(el.origin, state.origin);
    set_text(el.destination, state.destination);
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

init();
