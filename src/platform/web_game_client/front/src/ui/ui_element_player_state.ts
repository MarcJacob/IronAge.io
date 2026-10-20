// UI element: the player state panel, bottom left, always visible during play (container in index.html, rows built here).
// Shows facts about the local player, one row per fact (label + value). Only Gold for now; add a field to PlayerState and a line to ROWS
// for the next ones (player name, total population, military strength...). Pure display, emits nothing.
// submit_player_state can be called every frame (the DOM is only touched when a text changes); null clears it.

export interface PlayerState
{
    gold: number;
}

// One row per displayed fact, top to bottom. big: large type (the headline figure).
const ROWS: { label: string, big: boolean, text: (state: PlayerState) => string }[] = [
    { label: "Gold", big: true, text: (state) => String(state.gold) },
];

let panelElement: HTMLElement | null = null;
let valueElements: HTMLElement[] = [];

export function init_ui_element_player_state(): void
{
    panelElement = document.getElementById("game_hud_player_state") as HTMLElement;
    panelElement.replaceChildren();
    valueElements = ROWS.map((row) => {
        const root = document.createElement("div");
        root.className = "player_state_row" + (row.big ? " big" : "");
        const label = document.createElement("span");
        label.className = "player_state_label";
        label.textContent = row.label;
        const value = document.createElement("span");
        value.className = "player_state_value";
        root.append(label, value);
        panelElement!.appendChild(root);
        return value;
    });
}

// Makes the panel show the given state. Null clears it (hidden).
export function submit_player_state(state: PlayerState | null): void
{
    if (panelElement === null) return;

    if (state === null)
    {
        if (!panelElement.hidden) panelElement.hidden = true;
        return;
    }

    panelElement.hidden = false;
    ROWS.forEach((row, i) => {
        const text = row.text(state);
        if (valueElements[i].textContent !== text) valueElements[i].textContent = text;
    });
}
