// UI element: the treasury readout, the local player's gold count, always visible during play (container in index.html, content built here).
// Pure display, emits nothing. submit_treasury can be called every frame (the DOM is only touched when the text changes); null clears it.

let treasuryElement: HTMLElement | null = null;

export function init_ui_element_treasury(): void
{
    treasuryElement = document.getElementById("game_hud_treasury") as HTMLElement;
}

// Makes the readout show the given gold count. Null clears it (hidden).
export function submit_treasury(gold: number | null): void
{
    if (treasuryElement === null) return;

    if (gold === null)
    {
        if (!treasuryElement.hidden) treasuryElement.hidden = true;
        return;
    }

    treasuryElement.hidden = false;
    const text = `Gold ${gold}`;
    if (treasuryElement.textContent !== text) treasuryElement.textContent = text;
}
