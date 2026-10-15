// Placement phase UI: message + emptying countdown bar (static DOM in index.html, refreshed from here).
// The countdown is indicative only: leaving the placement phase is up to the server's first tick.

let uiElement: HTMLElement | null = null;
let barFillElement: HTMLElement | null = null;

// performance.now() timestamp at which the countdown reaches 0, and its total duration.
let countdownEndMs = 0;
let countdownTotalMs = 1;
let shown = false;

export function init_placement_ui(): void
{
    uiElement = document.getElementById("placement_ui") as HTMLElement;
    barFillElement = document.getElementById("placement_ui_bar_fill") as HTMLElement;
}

// Shows the placement UI with a countdown starting now.
export function show_placement_ui(countdown_ms: number): void
{
    shown = true;
    countdownTotalMs = Math.max(1, countdown_ms);
    countdownEndMs = performance.now() + countdown_ms;
    if (uiElement !== null) uiElement.hidden = false;
}

// Safe to call when not shown.
export function hide_placement_ui(): void
{
    shown = false;
    if (uiElement !== null) uiElement.hidden = true;
}

// Per-frame refresh of the countdown bar.
export function update_placement_ui(): void
{
    if (!shown || barFillElement === null) return;

    const fraction = Math.min(1, Math.max(0, (countdownEndMs - performance.now()) / countdownTotalMs));
    barFillElement.style.width = `${fraction * 100}%`;
}
