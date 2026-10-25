// UI module (new style): the worker panel, inside the entity frame (ui_element_entity_frame.ts). The settlement a worker (settlers, road crew...)
// comes from and where it is. Static markup in index.html (ids below), extracted once by init.
// submit(state) shows it, submit(null) hides it. Emits nothing.

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_worker_panel",
    origin: "worker_panel_origin",
    location: "worker_panel_location",
};

export interface WorkerPanelState
{
    origin: string; // Origin settlement, as text.
    location: { x: number, y: number };
}

let el: {
    root: HTMLElement;
    origin: HTMLElement;
    location: HTMLElement;
} | null = null;

function init(): void
{
    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
        origin: document.getElementById(IDS.origin) as HTMLElement,
        location: document.getElementById(IDS.location) as HTMLElement,
    };
}

export function submit(state: WorkerPanelState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    set_text(el.origin, state.origin);
    set_text(el.location, `(${state.location.x}, ${state.location.y})`);
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

init();
