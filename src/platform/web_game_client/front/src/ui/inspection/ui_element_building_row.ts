// UI element: a building row of the properties panel (the panel owns the row list, see ui_element_properties_panel.ts).
// A small box showing one building type of a settlement: its name (stands in for an icon until there are some), how many are built, and one thin
// progress bar per ongoing project. A small button at the top right starts a new project. Hovering the box shows the description in the tooltip
// (ui_element_tooltip.ts), hovering the button shows what a new project costs. Boxes sit side by side, wrapping.
// Emits on_build when the button is pressed. The panel calls submit with the row's state every frame (the DOM is only touched when something changes).

import { submit_tooltip } from "./ui_element_tooltip.js"

export interface BuildingRowState
{
    name: string;
    count: number; // Built so far.
    projects: number[]; // Progress of each ongoing project, 0..255 (255 = done). One bar each.
    description: string[]; // Tooltip lines of the box.
    build_tooltip: string[]; // Tooltip lines of the button (cost, workers, duration...).
    build?: "enabled" | "disabled"; // State of the button. Without it the row has no button (not allowed to build).
    on_build?: () => void; // Called when the (enabled) button is pressed.
}

export class BuildingRowElement
{
    readonly root = document.createElement("div");
    private name = document.createElement("span");
    private count = document.createElement("span");
    private bars = document.createElement("div");
    private button = document.createElement("button");
    private state: BuildingRowState | null = null;
    private hovered = false;
    private over_button = false;
    private mouse_x = 0;
    private mouse_y = 0;

    constructor()
    {
        this.root.className = "building_row";
        this.name.className = "building_row_name";
        this.count.className = "building_row_count";
        this.bars.className = "building_row_bars";
        this.button.className = "building_row_button";
        this.button.textContent = "+";
        this.root.append(this.name, this.count, this.bars, this.button);

        // Not "disabled" for the browser (a disabled button gets no mouse events, so no tooltip): a class, and the click is ignored.
        this.button.addEventListener("click", () => { this.button.blur(); if (this.state?.build === "enabled") this.state.on_build?.(); });

        // One set of listeners for the whole box: the button shows its own tooltip, the rest of the box the description.
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
        this.over_button = event.target === this.button;
        this.refresh_tooltip();
    }

    private refresh_tooltip(): void
    {
        if (!this.hovered || this.state === null) return;
        const lines = this.over_button ? this.state.build_tooltip : this.state.description;
        submit_tooltip({ lines, x: this.mouse_x, y: this.mouse_y });
    }

    submit(state: BuildingRowState): void
    {
        this.state = state;
        if (this.name.textContent !== state.name) this.name.textContent = state.name;

        const count_text = `x${state.count}`;
        if (this.count.textContent !== count_text) this.count.textContent = count_text;

        // Bars are only rebuilt when the number of projects changes, otherwise their width is updated in place.
        while (this.bars.children.length > state.projects.length) this.bars.lastElementChild!.remove();
        while (this.bars.children.length < state.projects.length)
        {
            const bar = document.createElement("div");
            bar.className = "building_row_bar";
            bar.append(document.createElement("div"));
            this.bars.appendChild(bar);
        }
        state.projects.forEach((progress, index) => {
            const fill = this.bars.children[index].firstElementChild as HTMLElement;
            const width = `${Math.round(progress / 255 * 100)}%`;
            if (fill.style.width !== width) fill.style.width = width;
        });

        this.button.hidden = state.build === undefined;
        this.button.classList.toggle("disabled", state.build !== "enabled");

        this.refresh_tooltip();
    }
}
