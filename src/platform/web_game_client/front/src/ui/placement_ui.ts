// UI module (new style): the placement phase overlay. A message and an emptying countdown bar.
// Static markup in index.html (ids below, the message text is in the markup), extracted once by init. submit(state) shows it, submit(null) hides it.
// Pure display, emits nothing. The countdown itself is kept by the driver mode (spawn_pick.ts), which submits the remaining fraction every frame.

// Every DOM id this module uses (index.html).
const IDS = {
    root: "placement_ui",
    bar_fill: "placement_ui_bar_fill",
};

export interface PlacementState
{
    remaining: number; // 0..1, fraction of the countdown left (the bar empties).
}

let el: {
    root: HTMLElement;
    bar_fill: HTMLElement;
} | null = null;

function init(): void
{
    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
        bar_fill: document.getElementById(IDS.bar_fill) as HTMLElement,
    };
}

export function submit(state: PlacementState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    const width = `${Math.round(state.remaining * 1000) / 10}%`;
    if (el.bar_fill.style.width !== width) el.bar_fill.style.width = width;
}

init();
