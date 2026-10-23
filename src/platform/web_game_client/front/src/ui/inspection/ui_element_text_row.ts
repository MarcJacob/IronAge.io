// UI element: a text row of the properties panel (the panel owns the row list, see ui_element_properties_panel.ts).
// Shows a label and a text on one line. Pure display, emits nothing.
// The panel creates one TextRowElement per row and calls submit with the row's state every frame (the DOM is only touched when the text changes).

export interface TextRowState
{
    label: string;
    text: string;
    warn?: boolean; // Shows the text as a warning (red).
}

export class TextRowElement
{
    readonly root = document.createElement("div");
    private label = document.createElement("span");
    private text = document.createElement("span");

    constructor()
    {
        this.root.className = "text_row";
        this.label.className = "text_row_label";
        this.root.append(this.label, this.text);
    }

    submit(state: TextRowState): void
    {
        if (this.label.textContent !== state.label) this.label.textContent = state.label;
        if (this.text.textContent !== state.text) this.text.textContent = state.text;
        this.text.classList.toggle("warn", state.warn === true);
    }
}
