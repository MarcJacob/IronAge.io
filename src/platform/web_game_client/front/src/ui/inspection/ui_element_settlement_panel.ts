// UI module (new style): the settlement panel, inside the entity frame (ui_element_entity_frame.ts).
// Top left: population with its daily growth, production with the tax rate (a small slider when editable), consumption (goods sold / needed with
// the surplus / deficit), Market Goods. Top right: the garrison (one gauge per pooled troop type: home, raised, ceiling, max). Bottom: one box per
// building type (count, one bar per ongoing project, build button). Value rows and buildings have hover tooltips.
// Static markup in index.html (ids below), extracted once by init. submit(state) shows it, submit(null) hides it.
// Emits, on release / press: "tax_rate"(tax_rate), "export_ratio"(export_ratio),"ceiling"(troop_type, ceiling) with the raw ceiling of SET_SETTLEMENT_TROOP_CEILING
// (troop_type 0 men-at-arms, 1 horsemen, 2 knights; released at max = follows max), "build"(building_type).

import { UiEvents } from "../ui_events.js"
import type { BuildingDefinition } from "../../gdo.js"
import * as TooltipUI from "./ui_element_tooltip.js"
import { StaticGauge, StaticSlider, HoverTooltip } from "./static_rows.js"

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_settlement_panel",
    population: "settlement_panel_population",
    population_value: "settlement_panel_population_value",
    population_growth: "settlement_panel_population_growth",
    population_note: "settlement_panel_population_note",
    peasants: "settlement_panel_peasants",
    urban: "settlement_panel_urban",
    workers: "settlement_panel_workers",
    soldiers: "settlement_panel_soldiers",
    unemployed: "settlement_panel_unemployed",
    caravans: "settlement_panel_caravans",
    caravans_value: "settlement_panel_caravans_value",
    ships: "settlement_panel_ships",
    ships_value: "settlement_panel_ships_value",
    production: "settlement_panel_production",
    production_value: "settlement_panel_production_value",
    tax_value: "settlement_panel_tax_value",
    tax_slider: "settlement_panel_tax_slider",
    export_row: "settlement_panel_export",
    export_value: "settlement_panel_export_value",
    export_slider: "settlement_panel_export_slider",
    foreign_export_checkbox: "settlement_panel_foreign_export_checkbox",
    consumption: "settlement_panel_consumption",
    consumption_value: "settlement_panel_consumption_value",
    consumption_delta: "settlement_panel_consumption_delta",
    market_goods: "settlement_panel_market_goods",
    market_goods_percent: "settlement_panel_market_goods_percent",
    trade_volume: "settlement_panel_trade_volume",
    garrison: ["settlement_panel_men_at_arms", "settlement_panel_horsemen", "settlement_panel_knights"], // troop_type order.
    building_bar_template: "settlement_panel_building_bar_template",
    building_template: "settlement_panel_building_template",
};

const GARRISON_COLORS = ["#4aa3ff", "#f0b040", "#c060e0"]; // troop_type order, as in section_troops.ts.
const MARKET_GOODS_COLOR = "#d4a93c";
const TRADE_VOLUME_COLOR = "#4fc08d";
const EXPORT_COLOR = "#7fd0ff"; // Export pool gauge segment (the export ratio slider's color is in style.css).

// Population growth per day (in %) above 0 but below this is shown yellow instead of green.
const GROWTH_CLOSE_TO_ZERO_PERCENT = 0.5;

const CEILING_SATURATED = 0xFFFF; // Raw ceiling meaning "follows max".

export interface SettlementGarrisonState
{
    home: number; // In the local pool.
    raised: number; // In armies.
    max: number;
    ceiling: number; // Effective ceiling, on the total (home + raised).
}

export interface SettlementBuildingState
{
    definition: BuildingDefinition;
    count: number; // Built so far.
    projects: number[]; // Progress of each ongoing project, 0..255. One bar each.
    build: "enabled" | "disabled" | null; // Build button state. Null: no button (not allowed to build).
}

export interface SettlementPanelState
{
    population: number; // Home population (without the raised troops).
    raised: number; // Troops raised into armies.
    peasants: number;
    peasants_cap: number;
    urban_cap: number; // Capacity for the non-peasant population (workers + home soldiers + unemployed).
    workers: number;
    soldiers_home: number;
    unemployed: number;
    caravans: { active: number, max: number }; // Row hidden when max is 0.
    ships: { active: number, max: number }; // Row hidden when max is 0.
    growth_percent: number; // Last day's change, % of the population.
    production: number; // Last day, before tax.
    tax: number; // Last day.
    tax_rate: number; // %.
    tax_rate_min: number;
    tax_rate_max: number;
    consumption: { sold: number, needed: number, population_change: number } | null; // Last day. Null hides the row.
    export_ratio: { ratio: number, max: number } | null; // % of incoming goods set aside for export, and the highest settable. Null hides the slider.
    allow_foreign_export: boolean; // Shown for any settlement, only changeable when editable.
    market_goods: { goods: number, export_goods: number, capacity: number } | null; // Local pool, export pool, capacity shared by both. Null hides the gauge.
    trade_volume: { volume: number, max: number } | null; // Null hides the gauge.
    garrison: (SettlementGarrisonState | null)[]; // troop_type order. Null hides the gauge.
    buildings: (SettlementBuildingState | null)[]; // Building table order. Null hides the box.
    editable: boolean; // Tax, export and ceiling sliders can be moved (tax and export show disabled otherwise).
}

const events = new UiEvents<{
    tax_rate: (tax_rate: number) => void;
    export_ratio: (export_ratio: number) => void;
    allow_foreign_export: (allow_foreign_export: number) => void;
    ceiling: (troop_type: number, ceiling: number) => void;
    build: (building_type: number) => void;
}>();
export const on = events.on.bind(events);

// A building box of the markup: count, project bars (cloned from the template), build button. Hovering the button shows the cost, the rest the description.
class BuildingBox
{
    private name: HTMLElement;
    private count: HTMLElement;
    private bars: HTMLElement;
    private button: HTMLElement;
    private state: SettlementBuildingState | null = null;
    private hovered = false;
    private over_button = false;
    private mouse_x = 0;
    private mouse_y = 0;

    constructor(private root: HTMLElement, private bar_template: HTMLElement, on_build: () => void)
    {
        this.name = root.querySelector(".building_row_name") as HTMLElement;
        this.count = root.querySelector(".building_row_count") as HTMLElement;
        this.bars = root.querySelector(".building_row_bars") as HTMLElement;
        this.button = root.querySelector(".building_row_button") as HTMLElement;

        // Not "disabled" for the browser (a disabled button gets no mouse events, so no tooltip): a class, and the click is ignored.
        this.button.addEventListener("click", () => { this.button.blur(); if (this.state?.build === "enabled") on_build(); });

        root.addEventListener("mouseenter", (event) => { this.hovered = true; this.track(event); });
        root.addEventListener("mousemove", (event) => this.track(event));
        root.addEventListener("mouseleave", () => { this.hovered = false; TooltipUI.submit(null); });
    }

    // Null hides the box.
    submit(state: SettlementBuildingState | null): void
    {
        this.state = state;
        this.root.hidden = state === null;
        if (state === null)
        {
            if (this.hovered) TooltipUI.submit(null);
            this.hovered = false;
            return;
        }

        set_text(this.name, state.definition.name);
        set_text(this.count, `x${state.count}`);

        // Bars are only added / removed when the number of projects changes, otherwise their width is updated in place.
        while (this.bars.children.length > state.projects.length) this.bars.lastElementChild!.remove();
        while (this.bars.children.length < state.projects.length)
        {
            const bar = this.bar_template.cloneNode(true) as HTMLElement;
            bar.removeAttribute("id");
            bar.hidden = false;
            this.bars.appendChild(bar);
        }
        state.projects.forEach((progress, index) => {
            const fill = this.bars.children[index].firstElementChild as HTMLElement;
            const width = `${Math.round(progress / 255 * 100)}%`;
            if (fill.style.width !== width) fill.style.width = width;
        });

        this.button.hidden = state.build === null;
        this.button.classList.toggle("disabled", state.build !== "enabled");
        this.refresh_tooltip();
    }

    private track(event: MouseEvent): void
    {
        this.mouse_x = event.clientX;
        this.mouse_y = event.clientY;
        this.over_button = event.target === this.button;
        this.refresh_tooltip();
    }

    private refresh_tooltip(): void
    {
        if (!this.hovered || this.state === null) return;
        const definition = this.state.definition;
        const lines = this.over_button
            ? [
                `Build ${definition.name}`,
                `Cost: ${definition.cost_per_day * definition.build_days} (${definition.cost_per_day} per day)`,
                `Workers: ${definition.workers}`,
                `Duration: ${definition.build_days} days`,
                ...(definition.requires_coast && this.state.build === "disabled" ? ["Coastal settlements only"] : []),
            ]
            : [definition.name, definition.description, `Built: ${this.state.count}`];
        TooltipUI.submit({ lines, x: this.mouse_x, y: this.mouse_y });
    }
}

let el: {
    root: HTMLElement;
    population_value: HTMLElement;
    population_growth: HTMLElement;
    population_note: HTMLElement;
    peasants: HTMLElement;
    urban: HTMLElement;
    workers: HTMLElement;
    soldiers: HTMLElement;
    unemployed: HTMLElement;
    caravans: HTMLElement;
    caravans_value: HTMLElement;
    ships: HTMLElement;
    ships_value: HTMLElement;
    production_tooltip: HoverTooltip;
    production_value: HTMLElement;
    tax: StaticSlider;
    export_row: HTMLElement;
    export_ratio: StaticSlider;
    export_tooltip: HoverTooltip;
    foreign_export_checkbox: HTMLInputElement;
    consumption: HTMLElement;
    consumption_tooltip: HoverTooltip;
    consumption_value: HTMLElement;
    consumption_delta: HTMLElement;
    market_goods: StaticGauge;
    market_goods_percent: HTMLElement;
    market_goods_tooltip: HoverTooltip;
    trade_volume: StaticGauge;
    trade_volume_tooltip: HoverTooltip;
    garrison: StaticGauge[];
    buildings: BuildingBox[]; // Grown on demand to the number of building types submitted.
    building_template: HTMLElement;
} | null = null;

let shown: SettlementPanelState | null = null; // Last submitted state: the max the ceiling event needs.

function get(id: string): HTMLElement
{
    return document.getElementById(id) as HTMLElement;
}

function init(): void
{
    const consumption = get(IDS.consumption);
    el = {
        root: get(IDS.root),
        population_value: get(IDS.population_value),
        population_growth: get(IDS.population_growth),
        population_note: get(IDS.population_note),
        peasants: get(IDS.peasants),
        urban: get(IDS.urban),
        workers: get(IDS.workers),
        soldiers: get(IDS.soldiers),
        unemployed: get(IDS.unemployed),
        caravans: get(IDS.caravans),
        caravans_value: get(IDS.caravans_value),
        ships: get(IDS.ships),
        ships_value: get(IDS.ships_value),
        production_tooltip: new HoverTooltip(get(IDS.production)),
        production_value: get(IDS.production_value),
        tax: new StaticSlider(get(IDS.tax_value), get(IDS.tax_slider) as HTMLInputElement, value => events.emit("tax_rate", value)),
        export_row: get(IDS.export_row),
        export_ratio: new StaticSlider(get(IDS.export_value), get(IDS.export_slider) as HTMLInputElement, value => events.emit("export_ratio", value)),
        export_tooltip: new HoverTooltip(get(IDS.export_row)),
        foreign_export_checkbox: get(IDS.foreign_export_checkbox) as HTMLInputElement,
        consumption,
        consumption_tooltip: new HoverTooltip(consumption),
        consumption_value: get(IDS.consumption_value),
        consumption_delta: get(IDS.consumption_delta),
        market_goods: new StaticGauge(get(IDS.market_goods)),
        market_goods_percent: get(IDS.market_goods_percent),
        market_goods_tooltip: new HoverTooltip(get(IDS.market_goods)),
        trade_volume: new StaticGauge(get(IDS.trade_volume)),
        trade_volume_tooltip: new HoverTooltip(get(IDS.trade_volume)),
        garrison: IDS.garrison.map((id, troop_type) => new StaticGauge(get(id), value => emit_ceiling(troop_type, value))),
        buildings: [],
        building_template: get(IDS.building_template),
    };

    // The browser toggles the box on click; blurring it (keys keep reaching the game) lets submit_economy resync it with the real state.
    el.foreign_export_checkbox.addEventListener("change", () => {
        const checkbox = el!.foreign_export_checkbox;
        checkbox.blur();
        events.emit("allow_foreign_export", checkbox.checked ? 1 : 0);
    });
}

function emit_ceiling(troop_type: number, value: number): void
{
    const garrison = shown?.garrison[troop_type] ?? null;
    if (garrison === null) return;
    events.emit("ceiling", troop_type, value >= garrison.max ? CEILING_SATURATED : value);
}

export function submit(state: SettlementPanelState | null): void
{
    if (el === null) return;

    shown = state;
    el.root.hidden = state === null;
    if (state === null)
    {
        // Forgets held slider values, closes tooltips (no mouseleave once hidden).
        el.tax.submit(null);
        el.export_ratio.submit(null);
        el.export_tooltip.hide();
        el.market_goods_tooltip.hide();
        for (const gauge of el.garrison) gauge.submit(null);
        for (const box of el.buildings) box.submit(null);
        el.production_tooltip.hide();
        el.consumption_tooltip.hide();
        el.trade_volume_tooltip.hide();
        return;
    }

    submit_economy(state);

    for (let i = 0; i < el.garrison.length; i++)
    {
        const garrison = state.garrison[i] ?? null;
        el.garrison[i].submit(garrison === null ? null : {
            color: GARRISON_COLORS[i],
            current: garrison.home,
            raised: garrison.raised,
            max: garrison.max,
            slider: garrison.ceiling,
            slider_name: "ceiling",
            warn_below_current: true, // Lowering a ceiling below the pool discards the excess troops.
            editable: state.editable,
        });
    }

    // One box per building type given, cloned from the template when missing (the table can grow without touching the markup).
    while (el.buildings.length < state.buildings.length)
    {
        const building_type = el.buildings.length;
        const root = el.building_template.cloneNode(true) as HTMLElement;
        root.removeAttribute("id");
        el.building_template.parentElement!.appendChild(root);
        el.buildings.push(new BuildingBox(root, get(IDS.building_bar_template), () => events.emit("build", building_type)));
    }
    for (let i = 0; i < el.buildings.length; i++) el.buildings[i].submit(state.buildings[i] ?? null);
}

function submit_economy(state: SettlementPanelState): void
{
    if (el === null) return;

    // Population and its growth: + green, - red, yellow above but close to 0.
    const growth = state.growth_percent;
    set_text(el.population_value, String(state.population));
    set_text(el.population_growth, (growth > 0 ? "+" : "") + growth.toFixed(1) + "%");
    set_class(el.population_growth, "value_row_delta " + (growth < 0 ? "negative" : growth === 0 ? "neutral" : growth < GROWTH_CLOSE_TO_ZERO_PERCENT ? "warning" : "positive"));
    set_text(el.population_note, state.raised > 0 ? `(+ ${state.raised} Raised)` : "");
    set_text(el.peasants, `${state.peasants} / ${state.peasants_cap}`);
    set_text(el.urban, `${state.workers + state.soldiers_home + state.unemployed} / ${state.urban_cap}`);
    set_text(el.workers, String(state.workers));
    set_text(el.soldiers, String(state.soldiers_home));
    set_text(el.unemployed, String(state.unemployed));

    // Slot rows (active / max), hidden when the settlement cannot have any.
    el.caravans.hidden = state.caravans.max === 0;
    set_text(el.caravans_value, `${state.caravans.active} / ${state.caravans.max}`);
    el.ships.hidden = state.ships.max === 0;
    set_text(el.ships_value, `${state.ships.active} / ${state.ships.max}`);

    set_text(el.production_value, `${state.production} / day`);
    el.production_tooltip.submit([
        "Last day",
        `Production ${state.production}`,
        `Tax -${state.tax}`,
        `Added to goods (local + export) ${state.production - state.tax}`,
    ]);
    el.tax.submit({ value: state.tax_rate, min: state.tax_rate_min, max: state.tax_rate_max, unit: "%", editable: state.editable });

    const exporting = state.export_ratio;
    el.export_row.hidden = exporting === null;
    if (exporting === null)
    {
        el.export_ratio.submit(null);
        el.export_tooltip.hide();
    }
    else
    {
        el.export_ratio.submit({ value: exporting.ratio, min: 0, max: exporting.max, unit: "%", editable: state.editable });
        el.export_tooltip.submit([
            "Share of all incoming goods (production and imports)",
            "set aside for export, until the export pool holds",
            "that share of the capacity (the rest goes to the local pool).",
            "The export pool is never consumed locally.",
            "Caravans take up to 50% of it.",
        ]);
    }

    // Not rewritten while focused (a click blurs it, so only between press and release). Afterwards it follows the state, so a rejected command reverts.
    if (el.foreign_export_checkbox.checked !== state.allow_foreign_export && document.activeElement !== el.foreign_export_checkbox) el.foreign_export_checkbox.checked = state.allow_foreign_export;
    el.foreign_export_checkbox.disabled = !state.editable;

    const consumption = state.consumption;
    el.consumption.hidden = consumption === null;
    if (consumption === null) el.consumption_tooltip.hide();
    else
    {
        const surplus = consumption.sold - consumption.needed;
        set_text(el.consumption_value, `${consumption.sold} / ${consumption.needed}`);
        set_text(el.consumption_delta, (surplus > 0 ? "+" : "") + surplus);
        set_class(el.consumption_delta, "value_row_delta " + (surplus > 0 ? "positive" : surplus < 0 ? "negative" : "neutral"));
        el.consumption_tooltip.submit([
            "Last day",
            `Sold ${consumption.sold}`,
            `Needed ${consumption.needed}`,
            "Surplus / deficit: goods sold minus goods needed",
            `Growth ${consumption.population_change} per day`,
            "When goods run short, the unemployed",
            "die first, then peasants, then workers.",
            "Soldiers do not starve.",
        ]);
    }

    // Both pools share the capacity and may exceed it: the bar then stays full (split in proportion) and the percentage shows it.
    const market = state.market_goods;
    // The marker shows where the export pool ends when full: export cap = capacity x export ratio.
    const exportCap = market === null || state.export_ratio === null ? undefined : Math.floor(market.capacity * state.export_ratio.ratio / 100);
    el.market_goods.submit(market === null ? null : { color: MARKET_GOODS_COLOR, current: market.goods, right: { amount: market.export_goods, color: EXPORT_COLOR }, right_cap: exportCap, max: market.capacity, compact: true, editable: false });
    if (market === null) el.market_goods_tooltip.hide();
    else
    {
        const percent = market.capacity > 0 ? Math.round(100 * (market.goods + market.export_goods) / market.capacity) : 0;
        set_text(el.market_goods_percent, `(${percent}%)`);
        el.market_goods_tooltip.submit([
            `Local goods ${market.goods}`,
            `Export goods ${market.export_goods}` + (exportCap === undefined ? "" : ` (cap ${exportCap})`),
            `Capacity ${market.capacity} (shared), ${percent}% full`,
        ]);
    }

    const trade = state.trade_volume;
    el.trade_volume.submit(trade === null ? null : { color: TRADE_VOLUME_COLOR, current: trade.max > 0 ? Math.min(trade.volume, trade.max) : 0, max: trade.max, compact: true, editable: false });
    if (trade === null) el.trade_volume_tooltip.hide();
    else el.trade_volume_tooltip.submit(["Volume of recent exports and imports in this settlement. Provides a bonus to production and to further trade profits."]);
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

function set_class(element: HTMLElement, class_name: string): void
{
    if (element.className !== class_name) element.className = class_name;
}

init();
