// UI module (new style): the settlement's "Raise army" panel, inside the entity frame (ui_element_entity_frame.ts). One compact gauge per troop type
// (target slider against what the settlement can give now) and the cost of raising (red when the treasury cannot pay it).
// Static markup in index.html (ids below), extracted once by init. submit(state) shows it, submit(null) hides it.
// Emits "raise_targets" with all four targets (troop order), the released one updated, when a target slider is released.

import { UiEvents } from "../ui_events.js"
import { StaticGauge } from "./static_rows.js"

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_raise_army_panel",
    troops: ["raise_army_panel_levies", "raise_army_panel_men_at_arms", "raise_army_panel_horsemen", "raise_army_panel_knights"], // Troop order.
    cost: "raise_army_panel_cost",
};

const TROOP_COLORS = ["#c7cbd1", "#4aa3ff", "#f0b040", "#c060e0"]; // Troop order, as in section_troops.ts.

export interface RaiseArmyTroopState
{
    max: number; // What the settlement can give now.
    target: number;
}

export interface RaiseArmyPanelState
{
    troops: (RaiseArmyTroopState | null)[]; // Troop order (levies, men-at-arms, horsemen, knights). Null hides the gauge.
    cost: number; // One day of the troops' consumption, taken from the treasury.
    treasury: number;
}

const events = new UiEvents<{
    raise_targets: (targets: number[]) => void;
}>();
export const on = events.on.bind(events);

let el: {
    root: HTMLElement;
    troops: StaticGauge[];
    cost: HTMLElement;
} | null = null;

let shown: RaiseArmyPanelState | null = null; // Last submitted state: the targets the event completes.

function get(id: string): HTMLElement
{
    return document.getElementById(id) as HTMLElement;
}

function init(): void
{
    el = {
        root: get(IDS.root),
        troops: IDS.troops.map((id, i) => new StaticGauge(get(id), value => emit_targets(i, value))),
        cost: get(IDS.cost),
    };
}

function emit_targets(troop: number, value: number): void
{
    if (shown === null) return;
    const targets = shown.troops.map(state => state?.target ?? 0);
    targets[troop] = value;
    events.emit("raise_targets", targets);
}

export function submit(state: RaiseArmyPanelState | null): void
{
    if (el === null) return;

    shown = state;
    el.root.hidden = state === null;
    if (state === null)
    {
        for (const gauge of el.troops) gauge.submit(null); // Forgets held slider values.
        return;
    }

    for (let i = 0; i < el.troops.length; i++)
    {
        const troop = state.troops[i] ?? null;
        el.troops[i].submit(troop === null ? null
            : { color: TROOP_COLORS[i], current: 0, max: troop.max, slider: troop.target, slider_name: "target", compact: true, editable: true });
    }

    set_text(el.cost, `${state.cost} (1 day of supplies)`);
    el.cost.classList.toggle("warn", state.cost > state.treasury);
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

init();
