// UI element: the entity properties panel, left of the bottom action bar (container in index.html, content built here).
// Shows general information about what is selected, as a title, a list of stat rows (label / value), an optional split bar, an optional list of (clickable) rows
// and a list of gauges. A gauge shows a current value against a max,
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

// One part of a split bar: its share of the bar is its value over the sum of the bar's values.
export interface BarSegment
{
    color: string;
    value: number;
}

// One line of a list: a text and an optional small split bar. Pure display: a row only emits on_click (e.g. to toggle its selection).
export interface ListRow
{
    text: string;
    segments?: BarSegment[];
    selected?: boolean; // Highlighted.
    on_click?: () => void; // Makes the row clickable.
}

export interface PanelState
{
    title: string;
    stats?: StatRow[]; // Shown first, under the title.
    split_bar?: BarSegment[]; // One bar under the stats, divided between the segments in proportion to their values.
    list_title?: string; // Heading above the list.
    list?: ListRow[]; // Shown after the split bar, before the gauges.
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

// A split bar inside parent with count segments. Returns the segment elements, to update with update_split_bar.
function create_split_bar(parent: HTMLElement, count: number): HTMLElement[]
{
    const bar = document.createElement("div");
    bar.className = "split_bar";
    const parts: HTMLElement[] = [];
    for (let i = 0; i < count; i++) parts.push(bar.appendChild(document.createElement("div")));
    parent.appendChild(bar);
    return parts;
}

// Sizes each segment element by its value's share of the sum (all empty while the sum is 0).
function update_split_bar(parts: HTMLElement[], segments: BarSegment[]): void
{
    const total = segments.reduce((sum, segment) => sum + segment.value, 0);
    segments.forEach((segment, i) => {
        parts[i].style.width = `${total > 0 ? 100 * segment.value / total : 0}%`;
        parts[i].style.background = segment.color;
    });
}

// One built list row: its element, text and split bar segments.
interface BuiltRow
{
    root: HTMLElement;
    text: HTMLElement;
    parts: HTMLElement[];
}

// Renders a PanelState into a panel: root (hidden when cleared), title and content elements, the rows and gauges being built inside content.
export class PanelView
{
    private builtKey: string | null = null; // What the built rows and gauges are made for. A different key rebuilds them, the same key updates them in place (a slider mid-drag survives).
    private statValues: HTMLElement[] = [];
    private barParts: HTMLElement[] = [];
    private rows: BuiltRow[] = [];
    private list: ListRow[] = []; // Last submitted list: row clicks call its callbacks.
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
            this.barParts = [];
            this.rows = [];
            this.list = [];
            this.gauges = [];
            this.builtKey = null;
            return;
        }

        this.title.textContent = state.title;

        const stats = state.stats ?? [];
        const list = state.list ?? [];
        const gauges = state.gauges ?? [];
        const key = [state.title, state.gauges_title ?? "", ...stats.map(stat => stat.label),
            "bar", state.split_bar?.length ?? -1,
            "list", state.list_title ?? "", ...list.map(row => (row.segments?.length ?? 0) + (row.on_click !== undefined ? "+" : "")),
            "#", ...gauges.map(gauge => gauge.label + (gauge.on_change !== undefined ? "+" : ""))].join("|");
        if (key !== this.builtKey) this.build(state, key);

        stats.forEach((stat, i) => {
            const text = String(stat.value);
            if (this.statValues[i].textContent !== text) this.statValues[i].textContent = text;
        });
        if (state.split_bar !== undefined) update_split_bar(this.barParts, state.split_bar);
        this.list = list;
        list.forEach((row, i) => {
            const built = this.rows[i];
            if (built.text.textContent !== row.text) built.text.textContent = row.text;
            built.root.classList.toggle("selected", row.selected === true);
            if (row.segments !== undefined) update_split_bar(built.parts, row.segments);
        });
        gauges.forEach((gauge, i) => this.gauges[i].update(gauge, state.slider_name ?? ""));
    }

    private build(state: PanelState, key: string): void
    {
        this.content.replaceChildren();

        this.statValues = (state.stats ?? []).map(stat => {
            const row = document.createElement("div");
            row.className = "panel_stat";
            const label = document.createElement("span");
            label.textContent = stat.label;
            const value = document.createElement("span");
            row.append(label, value);
            this.content.appendChild(row);
            return value;
        });

        this.barParts = state.split_bar !== undefined ? create_split_bar(this.content, state.split_bar.length) : [];

        if (state.list_title !== undefined) this.add_heading(state.list_title);
        this.rows = (state.list ?? []).map((row, i) => {
            const root = document.createElement("div");
            root.className = "panel_list_row";
            const text = root.appendChild(document.createElement("div"));
            const parts = row.segments !== undefined ? create_split_bar(root, row.segments.length) : [];
            if (row.on_click !== undefined)
            {
                root.classList.add("clickable");
                root.addEventListener("click", () => this.list[i]?.on_click?.()); // The latest submitted row's callback.
            }
            this.content.appendChild(root);
            return { root, text, parts };
        });

        if (state.gauges_title !== undefined) this.add_heading(state.gauges_title);
        this.gauges = (state.gauges ?? []).map(gauge => new Gauge(this.content, gauge));
        this.builtKey = key;
    }

    private add_heading(text: string): void
    {
        const heading = document.createElement("div");
        heading.className = "panel_heading";
        heading.textContent = text;
        this.content.appendChild(heading);
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
