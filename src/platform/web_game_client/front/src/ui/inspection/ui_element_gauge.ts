// UI element: a gauge row of the properties panel (the panel owns the row list, see ui_element_properties_panel.ts).
// A gauge shows a current value against a max, and optionally a slider (a ceiling, a target...) the player can move.
// Pure display: it only emits on_change, when the slider is released.
// The panel creates one GaugeElement per row and calls submit_gauge with the row's state every frame (updates in place, null hides it).

export interface GaugeState
{
    label: string;
    color: string; // CSS color of the filled part.
    current: number;
    max?: number; // Without it no max is shown (nor allowed with a slider): the bar is drawn against scale instead.
    scale?: number; // Bar scale when there is no max (e.g. the largest current among the gauges).
    slider?: number; // Position of the slider / marker (e.g. a ceiling or a target), 0..max. Without it only current and max are shown.
    slider_name?: string; // What the slider means, shown in the text ("ceiling", "target").
    on_change?: (value: number) => void; // Makes the slider interactive. Called with the released value (not while dragging).
    warn_below_current?: boolean; // Warn (red) while the slider is below current: releasing there discards the difference.
}

// How long a released value is kept shown while waiting for the game state to catch up with it.
const PENDING_MS = 1500;

export class GaugeElement
{
    readonly root = document.createElement("div");
    private text = document.createElement("div");
    private fill = document.createElement("div");
    private marker = document.createElement("div");
    private slider: HTMLInputElement | null = null;
    private dragging = false;
    private pending: { value: number, time: number } | null = null; // Released, not yet in the state.
    private state: GaugeState | null = null;

    constructor()
    {
        this.root.className = "gauge";
        this.text.className = "gauge_text";

        const bar = document.createElement("div");
        bar.className = "gauge_bar";
        this.fill.className = "gauge_fill";
        this.marker.className = "gauge_marker";
        bar.append(this.fill, this.marker);
        this.root.append(this.text, bar);
    }

    // The range input exists only while the state is interactive.
    private set_interactive(interactive: boolean): void
    {
        if (interactive === (this.slider !== null)) return;

        if (!interactive)
        {
            this.slider?.remove();
            this.slider = null;
            this.dragging = false;
            this.pending = null;
            return;
        }

        const slider = document.createElement("input");
        slider.type = "range";
        slider.min = "0";
        slider.step = "1";
        slider.addEventListener("input", () => { this.dragging = true; this.render(Number(slider.value)); });
        slider.addEventListener("change", () => {
            this.dragging = false;
            this.pending = { value: Number(slider.value), time: performance.now() };
            slider.blur(); // Keys keep moving the camera.
            this.state?.on_change?.(this.pending.value);
        });
        this.root.appendChild(slider);
        this.slider = slider;
    }

    submit(state: GaugeState): void
    {
        this.state = state;
        this.set_interactive(state.on_change !== undefined);

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
        if (state === null) return;

        const scale = Math.max(state.max ?? state.scale ?? 0, state.current, shown ?? 0, 1);
        const warn = state.warn_below_current === true && shown !== undefined && shown < state.current;

        this.fill.style.width = `${100 * state.current / scale}%`;
        this.fill.style.background = state.color;
        this.marker.hidden = shown === undefined;
        this.marker.style.left = `${100 * (shown ?? 0) / scale}%`;
        this.marker.classList.toggle("warn", warn);

        this.text.textContent = `${state.label} ${state.current}` + (shown !== undefined ? ` / ${state.slider_name ?? ""} ${shown}` : "") + (state.max !== undefined ? ` / max ${state.max}` : "")
            + (warn ? `  (discards ${state.current - (shown ?? 0)})` : "");
        this.text.classList.toggle("warn", warn);
        if (this.slider !== null) this.slider.classList.toggle("warn", warn);
    }
}
