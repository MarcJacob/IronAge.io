// UI module (new style): the engagement panel, inside the entity frame (ui_element_entity_frame.ts). Kind, phase, commanders with their men, main target, location,
// and an optional note (e.g. why the assault cannot begin).
// Static markup in index.html (ids below), extracted once by init. submit(state) shows it, submit(null) hides it. Emits nothing.

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_engagement_panel",
    kind: "engagement_panel_kind",
    phase: "engagement_panel_phase",
    attacker: "engagement_panel_attacker",
    defender: "engagement_panel_defender",
    target: "engagement_panel_target",
    location: "engagement_panel_location",
    note: "engagement_panel_note",
};

export interface EngagementPanelState
{
    kind: string;
    phase: string;
    attacker: string; // Commander name and men, ready to show.
    defender: string;
    target: string; // Main target, ready to show. Empty if none.
    location: { x: number, y: number };
    note: string; // Empty = hidden text.
}

let el: {
    root: HTMLElement;
    kind: HTMLElement;
    phase: HTMLElement;
    attacker: HTMLElement;
    defender: HTMLElement;
    target: HTMLElement;
    location: HTMLElement;
    note: HTMLElement;
} | null = null;

function init(): void
{
    const get = (id: string) => document.getElementById(id) as HTMLElement;
    el = {
        root: get(IDS.root),
        kind: get(IDS.kind),
        phase: get(IDS.phase),
        attacker: get(IDS.attacker),
        defender: get(IDS.defender),
        target: get(IDS.target),
        location: get(IDS.location),
        note: get(IDS.note),
    };
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

export function submit(state: EngagementPanelState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    set_text(el.kind, state.kind);
    set_text(el.phase, state.phase);
    set_text(el.attacker, state.attacker);
    set_text(el.defender, state.defender);
    set_text(el.target, state.target);
    set_text(el.location, `(${state.location.x}, ${state.location.y})`);
    set_text(el.note, state.note);
}

init();
