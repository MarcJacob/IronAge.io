// One line of text near the top of the screen telling the player what the current mode is and what to do (static DOM in index.html).

let hintElement: HTMLElement | null = null;

export function init_mode_hint_ui(): void
{
    hintElement = document.getElementById("game_hud_mode_hint") as HTMLElement;
}

export function show_mode_hint(text: string): void
{
    if (hintElement === null) return;

    hintElement.textContent = text;
    hintElement.hidden = false;
}

// Safe to call when not shown.
export function hide_mode_hint(): void
{
    if (hintElement !== null) hintElement.hidden = true;
}
