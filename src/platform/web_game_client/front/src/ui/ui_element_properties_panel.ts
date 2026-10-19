// UI element: the entity properties panel, left of the bottom action bar (container in index.html, content built here).
// Shows general information about what is selected, as a title, a list of stat rows (label / value) and a list of gauges. A gauge shows a current value against a max,
// and optionally a slider (a ceiling, a target...) the player can move. Pure display: a gauge only emits on_change, when the slider is released.
// A mode keeps its own state, submits it with submit_properties_panel (every frame is fine: the panel updates in place) and submits null to clear it.
// The per-entity-type states are built in entity_panels.ts. PanelView (below) renders a PanelState into any panel DOM: other panels reuse it (advanced action panel).

export interface GaugeState
{
    label: string;
    color: string; // CSS color of the filled part.
    current: number;
    max?: number; // Without it no max is shown (nor allowed with a slider): the bar is drawn against scale instead.
    scale?: number; // Bar scale when there is no max (e.g. the largest current among the gauges).
    slider?: number; // Position of the slider / marker (e.g. a ceiling or a target), 0..max. Without it only current and max are shown.
    on_change?: (value: number) => void; // Makes the slider interactive. Called with the released value (not while dragging).
    warn_below_current?: boolean; // Warn (red) while the slider is below current: releasing there discards the difference.
}

// One line of text: a label and its value (e.g. "Population 120").
export interface StatRow
{
    label: string;
    value: string | number;
}

export interface PanelState
{
    title: string;
    stats?: StatRow[]; // Shown first, under the title.
    gauges_title?: string; // Heading above the gauges.
    slider_name?: string; // What the slider means, shown in the gauge text ("ceiling", "target").
    gauges?: GaugeState[];
}

// How long a released value is kept shown while waiting for the game state to catch up with it.
const PENDING_MS = 1500;

// One gauge: a text line, a bar (fill = current, marker = slider position) and a range input when interactive.
class Gauge
{
    private text = document.createElement("div");
    private fill = document.createElement("div");
    private marker = document.createElement("div");
    private slider: HTMLInputElement | null = null;
    private dragging = false;
    private pending: { value: number, time: number } | null = null; // Released, not yet in the state.
    private state: GaugeState;
    private slider_name = "";

    constructor(parent: HTMLElement, state: GaugeState)
    {
        this.state = state;

        const root = document.createElement("div");
        root.className = "gauge";
        this.text.className = "gauge_text";

        const bar = document.createElement("div");
        bar.className = "gauge_bar";
        this.fill.className = "gauge_fill";
        this.marker.className = "gauge_marker";
        bar.append(this.fill, this.marker);
        root.append(this.text, bar);

        if (state.on_change !== undefined)
        {
            const slider = document.createElement("input");
            slider.type = "range";
            slider.min = "0";
            slider.step = "1";
            slider.addEventListener("input", () => { this.dragging = true; this.render(Number(slider.value)); });
            slider.addEventListener("change", () => {
                this.dragging = false;
                this.pending = { value: Number(slider.value), time: performance.now() };
                slider.blur(); // Keys keep moving the camera.
                this.state.on_change?.(this.pending.value);
            });
            root.appendChild(slider);
            this.slider = slider;
        }
        parent.appendChild(root);
    }

    update(state: GaugeState, slider_name: string): void
    {
        this.state = state;
        this.slider_name = slider_name;

        let shown = state.slider;
        if (this.slider !== null && shown !== undefined)
        {
            if (this.pending !== null && (this.pending.value === shown || performance.now() - this.pending.time > PENDING_MS)) this.pending = null;

            this.slider.max = String(state.max ?? 0);
            if (this.pending !== null) shown = this.pending.value;
            else if (this.dragging) shown = Number(this.slider.value);
            this.slider.value = String(shown);
        }
        this.render(shown);
    }

    private render(shown: number | undefined): void
    {
        const state = this.state;
        const scale = Math.max(state.max ?? state.scale ?? 0, state.current, shown ?? 0, 1);
        const warn = state.warn_below_current === true && shown !== undefined && shown < state.current;

        this.fill.style.width = `${100 * state.current / scale}%`;
        this.fill.style.background = state.color;
        this.marker.hidden = shown === undefined;
        this.marker.style.left = `${100 * (shown ?? 0) / scale}%`;
        this.marker.classList.toggle("warn", warn);

        this.text.textContent = `${state.label} ${state.current}` + (shown !== undefined ? ` / ${this.slider_name} ${shown}` : "") + (state.max !== undefined ? ` / max ${state.max}` : "")
            + (warn ? `  (discards ${state.current - (shown ?? 0)})` : "");
        this.text.classList.toggle("warn", warn);
        if (this.slider !== null) this.slider.classList.toggle("warn", warn);
    }
}

// Renders a PanelState into a panel: root (hidden when cleared), title and content elements, the rows and gauges being built inside content.
export class PanelView
{
    private builtKey: string | null = null; // What the built rows and gauges are made for. A different key rebuilds them, the same key updates them in place (a slider mid-drag survives).
    private statValues: HTMLElement[] = [];
    private gauges: Gauge[] = [];

    constructor(private root: HTMLElement, private title: HTMLElement, private content: HTMLElement) {}

    // Makes the panel show the given state. Null clears it (hidden).
    submit(state: PanelState | null): void
    {
        this.root.hidden = state === null;
        if (state === null)
        {
            this.content.replaceChildren();
            this.statValues = [];
            this.gauges = [];
            this.builtKey = null;
            return;
        }

        this.title.textContent = state.title;

        const stats = state.stats ?? [];
        const gauges = state.gauges ?? [];
        const key = [state.title, state.gauges_title ?? "", ...stats.map(stat => stat.label), "#", ...gauges.map(gauge => gauge.label + (gauge.on_change !== undefined ? "+" : ""))].join("|");
        if (key !== this.builtKey) this.build(state.gauges_title, stats, gauges, key);

        stats.forEach((stat, i) => {
            const text = String(stat.value);
            if (this.statValues[i].textContent !== text) this.statValues[i].textContent = text;
        });
        gauges.forEach((gauge, i) => this.gauges[i].update(gauge, state.slider_name ?? ""));
    }

    private build(gauges_title: string | undefined, stats: StatRow[], gauges: GaugeState[], key: string): void
    {
        this.content.replaceChildren();

        this.statValues = stats.map(stat => {
            const row = document.createElement("div");
            row.className = "panel_stat";
            const label = document.createElement("span");
            label.textContent = stat.label;
            const value = document.createElement("span");
            row.append(label, value);
            this.content.appendChild(row);
            return value;
        });

        if (gauges_title !== undefined)
        {
            const heading = document.createElement("div");
            heading.className = "panel_heading";
            heading.textContent = gauges_title;
            this.content.appendChild(heading);
        }

        this.gauges = gauges.map(gauge => new Gauge(this.content, gauge));
        this.builtKey = key;
    }
}

let panelView: PanelView | null = null;

export function init_ui_element_properties_panel(): void
{
    panelView = new PanelView(
        document.getElementById("game_hud_properties_panel") as HTMLElement,
        document.getElementById("game_hud_properties_title") as HTMLElement,
        document.getElementById("game_hud_properties_content") as HTMLElement);
}

// Makes the panel show the given state. Null clears it (hidden).
export function submit_properties_panel(state: PanelState | null): void
{
    panelView?.submit(state);
}
