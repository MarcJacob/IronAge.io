// UI module (new style): the entity frame, bottom center of the HUD. Shared by the panels (ui_element_*_panel.ts: one per entity type,
// plus the settlement's Raise army and Found settlement panels), which live inside it: a title, a small grey owner line, and the container of the panels.
// Static markup in index.html (ids below), extracted once by init. submit(state) shows it, submit(null) hides it. Emits nothing.
// Which panel shows inside it is decided by the routing in inspect_entity.ts, not here.

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_entity_frame",
    title: "game_hud_entity_frame_title",
    subtitle: "game_hud_entity_frame_subtitle",
};

export interface EntityFrameState
{
    title: string;
    subtitle: string; // The owner's name. Empty hides the line.
}

let el: {
    root: HTMLElement;
    title: HTMLElement;
    subtitle: HTMLElement;
} | null = null;

function init(): void
{
    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
        title: document.getElementById(IDS.title) as HTMLElement,
        subtitle: document.getElementById(IDS.subtitle) as HTMLElement,
    };
}

export function submit(state: EntityFrameState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    set_text(el.title, state.title);
    set_text(el.subtitle, state.subtitle);
    el.subtitle.hidden = state.subtitle === "";
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

init();
