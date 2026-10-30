// UI module (new style): the army panel, inside the entity frame (ui_element_entity_frame.ts). Home settlement, one gauge per troop type
// (current against original counts with losses, or while raising: current, target slider and what the home settlement can give),
// supplies gauge, consumption with days left (hover tooltip), and a desertion / low supplies warning.
// Static markup in index.html (ids below), extracted once by init. submit(state) shows it, submit(null) hides it.
// Emits "raise_targets" with all four targets (troop order), the released one updated, when a target slider is released.

import { UiEvents } from "../ui_events.js"
import { StaticGauge, HoverTooltip } from "./static_rows.js"
import { supplies_color } from "./section_supplies.js"

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_army_panel",
    origin: "army_panel_origin",
    order: "army_panel_order",
    gold: "army_panel_gold",
    note: "army_panel_note",
    troops: ["army_panel_levies", "army_panel_men_at_arms", "army_panel_horsemen", "army_panel_knights"], // Troop order.
    supplies: "army_panel_supplies",
    consumption: "army_panel_consumption",
    consumption_value: "army_panel_consumption_value",
    consumption_note: "army_panel_consumption_note",
    warning: "army_panel_warning",
    warning_label: "army_panel_warning_label",
    warning_text: "army_panel_warning_text",
};

const TROOP_COLORS = ["#c7cbd1", "#4aa3ff", "#f0b040", "#c060e0"]; // Troop order, as in section_troops.ts.

const CONSUMPTION_TOOLTIP = [
    "Soldiers consume supplies every day.",
    "Armies refill from the treasury inside their owner's territory.",
    "Missing supplies make soldiers desert.",
];

export interface ArmyTroopState
{
    current: number;
    max: number; // Not raising: original count (at least current). Raising: what the home settlement can give now.
    target: number; // Raise target (shown while raising).
    losses: number; // Men lost since raised (shown when not raising).
}

export interface ArmyPanelState
{
    origin: string; // Home settlement, as text.
    order: string; // Current standing order and its target, as text.
    raising: boolean; // Raise gauges (compact, target marker) instead of the count gauges.
    troops: (ArmyTroopState | null)[]; // Troop order (levies, men-at-arms, horsemen, knights). Null hides the gauge.
    targets_editable: boolean; // While raising: the target sliders can be moved (emits raise_targets).
    supplies: number;
    supplies_capacity: number;
    supplies_consumption: number; // Per day.
    gold: number; // Gold the army carries.
    note: string; // Line of text under the rows (e.g. why an order is refused). Empty = nothing.
    desertions: number; // Soldiers deserted last day.
}

const events = new UiEvents<{
    raise_targets: (targets: number[]) => void;
}>();
export const on = events.on.bind(events);

let el: {
    root: HTMLElement;
    origin: HTMLElement;
    order: HTMLElement;
    gold: HTMLElement;
    note: HTMLElement;
    troops: StaticGauge[];
    supplies: StaticGauge;
    consumption_tooltip: HoverTooltip;
    consumption_value: HTMLElement;
    consumption_note: HTMLElement;
    warning: HTMLElement;
    warning_label: HTMLElement;
    warning_text: HTMLElement;
} | null = null;

let shown: ArmyPanelState | null = null; // Last submitted state: the targets the event completes.

function get(id: string): HTMLElement
{
    return document.getElementById(id) as HTMLElement;
}

function init(): void
{
    el = {
        root: get(IDS.root),
        origin: get(IDS.origin),
        order: get(IDS.order),
        gold: get(IDS.gold),
        note: get(IDS.note),
        troops: IDS.troops.map((id, i) => new StaticGauge(get(id), value => emit_targets(i, value))),
        supplies: new StaticGauge(get(IDS.supplies)),
        consumption_tooltip: new HoverTooltip(get(IDS.consumption)),
        consumption_value: get(IDS.consumption_value),
        consumption_note: get(IDS.consumption_note),
        warning: get(IDS.warning),
        warning_label: get(IDS.warning_label),
        warning_text: get(IDS.warning_text),
    };
}

function emit_targets(troop: number, value: number): void
{
    if (shown === null) return;
    const targets = shown.troops.map(state => state?.target ?? 0);
    targets[troop] = value;
    events.emit("raise_targets", targets);
}

export function submit(state: ArmyPanelState | null): void
{
    if (el === null) return;

    shown = state;
    el.root.hidden = state === null;
    if (state === null)
    {
        for (const gauge of el.troops) gauge.submit(null); // Forgets held slider values.
        el.consumption_tooltip.hide();
        return;
    }

    set_text(el.origin, state.origin);
    set_text(el.order, state.order);
    set_text(el.gold, String(state.gold));
    set_text(el.note, state.note);

    for (let i = 0; i < el.troops.length; i++)
    {
        const troop = state.troops[i] ?? null;
        el.troops[i].submit(troop === null ? null : state.raising
            ? { color: TROOP_COLORS[i], current: troop.current, max: troop.max, slider: troop.target, slider_name: "target", compact: true, editable: state.targets_editable }
            : { color: TROOP_COLORS[i], current: troop.current, max: troop.max, losses: troop.losses, editable: false });
    }

    const supplies = state.supplies;
    const capacity = state.supplies_capacity;
    const consumption = state.supplies_consumption;
    el.supplies.submit({
        color: supplies_color(capacity > 0 ? supplies / capacity : 1),
        current: supplies,
        max: capacity > 0 ? capacity : undefined,
        scale: Math.max(supplies, 1),
        editable: false,
    });

    const days = consumption > 0 ? Math.floor(supplies / consumption) : null;
    set_text(el.consumption_value, `${consumption} / day`);
    set_text(el.consumption_note, days === null ? "" : `(${days} ${days === 1 ? "day" : "days"} left)`);
    el.consumption_tooltip.submit(CONSUMPTION_TOOLTIP);

    const warning = state.desertions > 0 ? { label: "Desertion", text: `${state.desertions} soldiers deserted last day` }
        : consumption > 0 && supplies < consumption ? { label: "", text: "LOW ON SUPPLIES !" }
        : null;
    el.warning.hidden = warning === null;
    if (warning !== null)
    {
        set_text(el.warning_label, warning.label);
        set_text(el.warning_text, warning.text);
    }
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

init();
