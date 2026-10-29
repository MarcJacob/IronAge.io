// Helpers of the new-style entity panels (ui_element_*_panel.ts): bind row markup written in index.html to a state.
// StaticGauge: a .gauge element (label static in the markup, values written here, optional range input for a slider, optional .gauge_raised segment).
// StaticSlider: a value text and a range input (a setting such as a tax rate), colored by sign.
// HoverTooltip: shows lines in the generic tooltip while an element is hovered.
// The row look comes from index.html and style.css: nothing here builds DOM.

import * as TooltipUI from "./ui_element_tooltip.js"

export interface StaticGaugeState
{
    color: string; // CSS color of the filled part.
    current: number;
    raised?: number; // Amount out in the field (in armies): "(X Raised)" in the text and a darker segment after current (needs a .gauge_raised in the markup). The slider is then on the total.
    right?: { amount: number, color: string }; // A second segment filling from the right end (needs a .gauge_right in the markup). Shares the scale with current: beyond max the two split the bar in proportion. Compact text becomes "current + amount".
    right_cap?: number; // Size the right segment may reach: the marker (when there is no slider) sits where it would end if it were full, that much from the right end of the bar.
    max?: number; // Without it no max is shown: the bar is drawn against scale instead.
    scale?: number; // Bar scale when there is no max.
    slider?: number; // Position of the marker (e.g. a target), 0..max. Without it only current and max are shown.
    slider_name?: string; // What the slider means, shown in the text ("target").
    editable: boolean; // Shows the range input (needs slider and an input in the markup). Its released value goes to the on_change given to the constructor.
    losses?: number; // Shown next to the max when above 0.
    compact?: boolean; // Text reduced to the slider value (the current value without slider), the max alone at the right end of the line above the bar.
    warn_below_current?: boolean; // Warn (red) while the slider is below current + raised: releasing there discards current (never raised) down to the slider.
}

// How long a released value is kept shown while waiting for the game state to catch up with it.
const PENDING_MS = 1500;

export class StaticGauge
{
    private text: HTMLElement; // .gauge_text: static label + values.
    private values: HTMLElement; // After the static label in .gauge_text.
    private max_text: HTMLElement;
    private fill: HTMLElement;
    private raised_fill: HTMLElement | null;
    private right_fill: HTMLElement | null;
    private marker: HTMLElement;
    private slider: HTMLInputElement | null;
    private dragging = false;
    private pending: { value: number, time: number } | null = null; // Released, not yet in the state.
    private state: StaticGaugeState | null = null;

    // root: a .gauge element of index.html, parts found by class. on_change: called with the released slider value (not while dragging).
    constructor(private root: HTMLElement, on_change?: (value: number) => void)
    {
        this.text = root.querySelector(".gauge_text") as HTMLElement;
        this.values = root.querySelector(".gauge_values") as HTMLElement;
        this.max_text = root.querySelector(".gauge_max") as HTMLElement;
        this.fill = root.querySelector(".gauge_fill") as HTMLElement;
        this.raised_fill = root.querySelector(".gauge_raised");
        this.right_fill = root.querySelector(".gauge_right");
        this.marker = root.querySelector(".gauge_marker") as HTMLElement;
        this.slider = root.querySelector<HTMLInputElement>("input[type=range]");

        const slider = this.slider;
        if (slider === null) return;
        slider.addEventListener("input", () => { this.dragging = true; this.render(Number(slider.value)); });
        slider.addEventListener("change", () => {
            this.dragging = false;
            this.pending = { value: Number(slider.value), time: performance.now() };
            slider.blur(); // Keys keep moving the camera.
            on_change?.(this.pending.value);
        });
    }

    // Null hides the gauge (and forgets a held value).
    submit(state: StaticGaugeState | null): void
    {
        this.state = state;
        this.root.hidden = state === null;

        const editable = state !== null && state.editable && state.slider !== undefined && this.slider !== null;
        if (!editable)
        {
            this.dragging = false;
            this.pending = null;
        }
        if (this.slider !== null && this.slider.hidden === editable) this.slider.hidden = !editable;
        if (state === null) return;

        let shown = state.slider;
        if (editable && this.slider !== null && shown !== undefined)
        {
            if (this.pending !== null && (this.pending.value === shown || performance.now() - this.pending.time > PENDING_MS)) this.pending = null;

            const max = String(state.max ?? 0);
            if (this.slider.max !== max) this.slider.max = max;
            if (this.pending !== null) shown = this.pending.value;
            else if (this.dragging) shown = Number(this.slider.value);
            if (this.slider.value !== String(shown)) this.slider.value = String(shown);
        }
        this.render(shown);
    }

    private render(shown: number | undefined): void
    {
        const state = this.state;
        if (state === null) return;

        const raised = state.raised ?? 0;
        const right = state.right?.amount ?? 0;
        const scale = Math.max(state.max ?? state.scale ?? 0, state.current + raised + right, shown ?? 0, 1);
        const discards = shown === undefined ? 0 : Math.max(0, state.current - Math.max(0, shown - raised));
        const warn = state.warn_below_current === true && discards > 0;

        this.fill.style.width = `${100 * state.current / scale}%`;
        this.fill.style.background = state.color;
        if (this.raised_fill !== null)
        {
            this.raised_fill.style.width = `${100 * raised / scale}%`;
            this.raised_fill.style.background = `color-mix(in srgb, ${state.color} 45%, black)`;
        }
        if (this.right_fill !== null)
        {
            this.right_fill.style.width = `${100 * right / scale}%`;
            this.right_fill.style.background = state.right?.color ?? "";
        }
        const markerAt = shown ?? (state.right_cap === undefined ? undefined : scale - Math.min(state.right_cap, scale));
        this.marker.hidden = markerAt === undefined;
        this.marker.style.left = `${100 * (markerAt ?? 0) / scale}%`;
        this.marker.classList.toggle("warn", warn);

        const lost = state.losses !== undefined && state.losses > 0 ? ` (-${state.losses})` : "";
        const text = state.compact === true
            ? `${shown ?? state.current}` + (state.right !== undefined ? ` + ${state.right.amount}` : "")
            : `${state.current}` + (state.raised !== undefined ? ` (${state.raised} Raised)` : "") + (shown !== undefined ? ` / ${state.slider_name ?? ""} ${shown}` : "") + (state.max !== undefined ? ` / max ${state.max}${lost}` : "")
                + (warn ? `  (discards ${discards})` : "");
        const max_text = state.compact === true && state.max !== undefined ? `${state.max}${lost}` : "";
        if (this.values.textContent !== text) this.values.textContent = text;
        if (this.max_text.textContent !== max_text) this.max_text.textContent = max_text;
        this.text.classList.toggle("warn", warn);
        this.slider?.classList.toggle("warn", warn);
    }
}

export interface StaticSliderState
{
    value: number; // Current value in the game state.
    min: number;
    max: number;
    unit?: string; // Appended to the shown value (e.g. "%").
    editable: boolean; // Lets the range input be moved. Without it the input is shown disabled (read-only).
}

// A value text (.slider_row_value classes: negative blue, positive yellow, zero neutral) and a range input. Same hold behavior as StaticGauge:
// the released value stays shown until the state catches up with it (or a timeout), and a drag survives resubmits.
export class StaticSlider
{
    private dragging = false;
    private pending: { value: number, time: number } | null = null; // Released, not yet in the state.
    private state: StaticSliderState | null = null;

    private editor: HTMLInputElement | null = null; // Typed entry, open while the value text is clicked into.

    // text: where the value is written (click it to type a value when editable). on_change: called with the released / typed value (not while dragging).
    constructor(private text: HTMLElement, private slider: HTMLInputElement, private on_change: (value: number) => void)
    {
        slider.addEventListener("input", () => { this.dragging = true; this.pending = null; this.render(Number(slider.value)); });
        slider.addEventListener("change", () => {
            this.dragging = false;
            slider.blur(); // Keys keep moving the camera. Also lifts the focus guard of submit.
            this.commit(Number(slider.value));
        });
        text.addEventListener("click", () => this.open_editor());
    }

    private commit(value: number): void
    {
        this.pending = { value, time: performance.now() };
        this.on_change(value);
    }

    private open_editor(): void
    {
        if (this.editor !== null || this.state === null || !this.state.editable) return;
        const editor = document.createElement("input");
        editor.type = "text";
        editor.inputMode = "numeric";
        editor.className = "slider_row_editor";
        editor.value = String(Number(this.slider.value));
        this.editor = editor;
        this.text.hidden = true;
        this.text.after(editor);

        // Enter / blur confirm, Escape cancels. Removing the input blurs it again, hence the guard.
        const close = (confirm: boolean) => {
            if (this.editor !== editor) return;
            this.editor = null;
            const typed = Math.round(Number(editor.value));
            editor.remove();
            this.text.hidden = false;
            if (confirm && editor.value.trim() !== "" && Number.isFinite(typed) && this.state !== null && this.state.editable)
            {
                this.commit(Math.min(this.state.max, Math.max(this.state.min, typed)));
                this.submit(this.state);
            }
        };
        editor.addEventListener("keydown", (event) => {
            if (event.key === "Enter") close(true);
            else if (event.key === "Escape") close(false);
            else return;
            event.preventDefault();
            editor.blur();
        });
        editor.addEventListener("blur", () => close(true));
        editor.focus();
        editor.select();
    }

    // Null forgets a held value (the caller hides the markup).
    submit(state: StaticSliderState | null): void
    {
        this.state = state;
        const editable = state !== null && state.editable;
        if (!editable)
        {
            this.dragging = false;
            this.pending = null;
        }
        if (!editable) this.editor?.blur(); // Closes an open typed entry (nothing is sent: not editable).
        if (this.slider.hidden !== (state === null)) this.slider.hidden = state === null;
        if (state === null) return;
        if (this.slider.disabled === editable) this.slider.disabled = !editable; // Not editable: shown, read-only.

        if (this.pending !== null && (this.pending.value === state.value || performance.now() - this.pending.time > PENDING_MS)) this.pending = null;

        // Focus guard: while the range input has focus (a drag, or keys on it) the state does not move it.
        // A change blurs it, after which it resyncs (the pending hold covers the wait for the command).
        let shown = state.value;
        if (document.activeElement === this.slider) shown = Number(this.slider.value);
        else if (this.pending !== null) shown = this.pending.value;
        else if (this.dragging) shown = Number(this.slider.value);

        if (this.slider.min !== String(state.min)) this.slider.min = String(state.min);
        if (this.slider.max !== String(state.max)) this.slider.max = String(state.max);
        if (this.slider.value !== String(shown)) this.slider.value = String(shown);
        this.render(shown);
    }

    private render(shown: number): void
    {
        const state = this.state;
        if (state === null) return;

        const text = `${shown}${state.unit ?? ""}`; // (The text is hidden while the typed entry is open.)
        if (this.text.textContent !== text) this.text.textContent = text;

        const sign = shown < 0 ? "negative" : shown > 0 ? "positive" : "zero";
        const text_class = "slider_row_value " + sign;
        if (this.text.className !== text_class) this.text.className = text_class;
        if (this.slider.className !== sign) this.slider.className = sign;
    }
}

export class HoverTooltip
{
    private lines: string[] | null = null;
    private hovered = false;
    private mouse_x = 0;
    private mouse_y = 0;

    constructor(element: HTMLElement)
    {
        element.addEventListener("mouseenter", (event) => { this.hovered = true; this.track(event); });
        element.addEventListener("mousemove", (event) => this.track(event));
        element.addEventListener("mouseleave", () => { this.hovered = false; TooltipUI.submit(null); });
    }

    // The lines to show (refreshed in place while hovered). Null: no tooltip.
    submit(lines: string[] | null): void
    {
        this.lines = lines;
        this.refresh();
    }

    // Call when the element gets hidden: its mouseleave won't fire.
    hide(): void
    {
        if (this.hovered) TooltipUI.submit(null);
        this.hovered = false;
    }

    private track(event: MouseEvent): void
    {
        this.mouse_x = event.clientX;
        this.mouse_y = event.clientY;
        this.refresh();
    }

    private refresh(): void
    {
        if (!this.hovered) return;
        TooltipUI.submit(this.lines === null ? null : { lines: this.lines, x: this.mouse_x, y: this.mouse_y });
    }
}
