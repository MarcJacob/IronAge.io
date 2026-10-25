// UI module (new style): the battle panel, inside the entity frame (ui_element_entity_frame.ts). Where the battle is.
// Static markup in index.html (ids below), extracted once by init. submit(state) shows it, submit(null) hides it. Emits nothing.

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_battle_panel",
    location: "battle_panel_location",
};

export interface BattlePanelState
{
    location: { x: number, y: number };
}

let el: {
    root: HTMLElement;
    location: HTMLElement;
} | null = null;

function init(): void
{
    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
        location: document.getElementById(IDS.location) as HTMLElement,
    };
}

export function submit(state: BattlePanelState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    const text = `(${state.location.x}, ${state.location.y})`;
    if (el.location.textContent !== text) el.location.textContent = text;
}

init();
