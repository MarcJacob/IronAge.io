// UI element: a slider row of the properties panel (the panel owns the row list, see ui_element_properties_panel.ts).
// Shows a label, a whole number the player can set with a slider between min and max (which may be negative), colored by sign (negative blue, positive yellow, zero neutral).
// Pure display: it only emits on_change, when the slider is released.
// The panel creates one SliderRowElement per row and calls submit with the row's state every frame (updates in place).
// Same hold behavior as the gauge slider: the released value stays shown until the state catches up with it (or a timeout), and a drag survives resubmits.

export interface SliderRowState
{
    label: string;
    value: number; // Current value in the game state.
    min: number;
    max: number;
    unit?: string; // Appended to the shown value (e.g. "%").
    on_change: (value: number) => void; // Called with the released value (not while dragging).
}

// How long a released value is kept shown while waiting for the game state to catch up with it.
const PENDING_MS = 1500;

export class SliderRowElement
{
    readonly root = document.createElement("div");
    private label = document.createElement("span");
    private text = document.createElement("span");
    private slider = document.createElement("input");
    private dragging = false;
    private pending: { value: number, time: number } | null = null; // Released, not yet in the state.
    private state: SliderRowState | null = null;

    constructor()
    {
        this.root.className = "slider_row";
        this.label.className = "text_row_label";

        const line = document.createElement("div");
        line.append(this.label, this.text);

        this.slider.type = "range";
        this.slider.step = "1";
        this.slider.addEventListener("input", () => { this.dragging = true; this.render(Number(this.slider.value)); });
        this.slider.addEventListener("change", () => {
            this.dragging = false;
            this.pending = { value: Number(this.slider.value), time: performance.now() };
            this.slider.blur(); // Keys keep moving the camera.
            this.state?.on_change(this.pending.value);
        });

        this.root.append(line, this.slider);
    }

    submit(state: SliderRowState): void
    {
        this.state = state;

        if (this.pending !== null && (this.pending.value === state.value || performance.now() - this.pending.time > PENDING_MS)) this.pending = null;

        let shown = state.value;
        if (this.pending !== null) shown = this.pending.value;
        else if (this.dragging) shown = Number(this.slider.value);

        this.slider.min = String(state.min);
        this.slider.max = String(state.max);
        this.slider.value = String(shown);
        this.render(shown);
    }

    private render(shown: number): void
    {
        const state = this.state;
        if (state === null) return;

        const text = `${shown}${state.unit ?? ""}`;
        if (this.label.textContent !== state.label) this.label.textContent = state.label;
        if (this.text.textContent !== text) this.text.textContent = text;

        const sign = shown < 0 ? "negative" : shown > 0 ? "positive" : "zero";
        this.text.className = "slider_row_value " + sign;
        this.slider.className = sign;
    }
}
