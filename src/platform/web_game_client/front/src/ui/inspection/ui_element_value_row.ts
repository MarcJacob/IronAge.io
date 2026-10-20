// UI element: a value row of the properties panel (the panel owns the row list, see ui_element_properties_panel.ts).
// Shows a label, a value and optionally a signed delta (green above 0, red below, neutral at 0) on one line.
// With tooltip lines, hovering the row shows them in the tooltip (ui_element_tooltip.ts) near the mouse; leaving hides it.
// The panel creates one ValueRowElement per row and calls submit with the row's state every frame (the DOM is only touched when something changes).
// While hovered, a submit refreshes the tooltip lines in place, so live values don't flicker or close it.

import { submit_tooltip } from "./ui_element_tooltip.js"

export interface ValueRowState
{
    label: string;
    value: string;
    delta?: number; // Signed change, shown after the value. Without it nothing is shown.
    tooltip?: string[]; // Lines of the hover window. Without them the row has none.
}

export class ValueRowElement
{
    readonly root = document.createElement("div");
    private label = document.createElement("span");
    private value = document.createElement("span");
    private delta = document.createElement("span");
    private state: ValueRowState | null = null;
    private hovered = false;
    private mouse_x = 0;
    private mouse_y = 0;

    constructor()
    {
        this.root.className = "text_row value_row";
        this.label.className = "text_row_label";
        this.delta.className = "value_row_delta";
        this.root.append(this.label, this.value, this.delta);

        this.root.addEventListener("mouseenter", (event) => { this.hovered = true; this.track(event); });
        this.root.addEventListener("mousemove", (event) => this.track(event));
        this.root.addEventListener("mouseleave", () => { this.hovered = false; submit_tooltip(null); });
    }

    // Call before dropping the row: its mouseleave won't fire once it is gone.
    dispose(): void
    {
        if (this.hovered) submit_tooltip(null);
        this.hovered = false;
    }

    private track(event: MouseEvent): void
    {
        this.mouse_x = event.clientX;
        this.mouse_y = event.clientY;
        this.refresh_tooltip();
    }

    private refresh_tooltip(): void
    {
        if (!this.hovered) return;
        const lines = this.state?.tooltip;
        submit_tooltip(lines === undefined ? null : { lines, x: this.mouse_x, y: this.mouse_y });
    }

    submit(state: ValueRowState): void
    {
        this.state = state;
        if (this.label.textContent !== state.label) this.label.textContent = state.label;
        if (this.value.textContent !== state.value) this.value.textContent = state.value;

        const delta = state.delta;
        const delta_text = delta === undefined ? "" : (delta > 0 ? "+" : "") + delta;
        if (this.delta.textContent !== delta_text) this.delta.textContent = delta_text;
        const sign = delta === undefined ? "" : delta > 0 ? "positive" : delta < 0 ? "negative" : "neutral";
        const class_name = "value_row_delta " + sign;
        if (this.delta.className !== class_name) this.delta.className = class_name;

        this.root.classList.toggle("has_tooltip", state.tooltip !== undefined);
        this.refresh_tooltip();
    }
}
