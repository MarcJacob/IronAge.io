// UI element: one line of text near the top of the screen telling the player what the current mode is and what to do (static DOM in index.html).
// A mode submits its state with submit_hint when it should be shown and submits null to clear it.

export interface HintState
{
    text: string;
}

let hintElement: HTMLElement | null = null;

export function init_ui_element_hint(): void
{
    hintElement = document.getElementById("game_hud_mode_hint") as HTMLElement;
}

// Makes the hint show the given state. Null clears it (hidden).
export function submit_hint(state: HintState | null): void
{
    if (hintElement === null) return;

    hintElement.textContent = state !== null ? state.text : "";
    hintElement.hidden = state === null;
}
