// UI module (new style): the settlement's "Found settlement" panel, inside the entity frame (ui_element_entity_frame.ts).
// The colonists the new settlement takes (and the peasants there are), and what the hovered target tile would give.
// Static markup in index.html (ids below), extracted once by init. submit(state) shows it, submit(null) hides it. Emits nothing.

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_found_settlement_panel",
    colonists: "found_settlement_panel_colonists",
    target: "found_settlement_panel_target",
};

export interface FoundSettlementPanelState
{
    colonists: number; // Peasants taken by the founding.
    peasants: number; // Peasants of the settlement.
    target: string | null; // Verdict on the hovered tile, in words (from the order's validity feedback, worded by the mode). Null: nothing hovered.
}

let el: {
    root: HTMLElement;
    colonists: HTMLElement;
    target: HTMLElement;
} | null = null;

function init(): void
{
    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
        colonists: document.getElementById(IDS.colonists) as HTMLElement,
        target: document.getElementById(IDS.target) as HTMLElement,
    };
}

export function submit(state: FoundSettlementPanelState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    set_text(el.colonists, `${state.colonists} (peasants ${state.peasants})`);
    set_text(el.target, state.target ?? "Pick a location");
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

init();
