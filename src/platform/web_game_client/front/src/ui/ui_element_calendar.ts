// UI module (new style): the calendar at the top of the screen. Shows the current day as text and how far it has gone as a bar.
// Static markup in index.html (ids below), extracted once by init. submit(state) shows it, submit(null) hides it. Pure display, emits nothing.
// submit can be called every frame (the DOM is only touched when something changes).

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_calendar",
    text: "game_hud_calendar_text",
    fill: "game_hud_calendar_fill",
};

export interface CalendarState
{
    day: number; // Day to display.
    progress: number; // 0..1, how far the day has gone.
}

let el: {
    root: HTMLElement;
    text: HTMLElement;
    fill: HTMLElement;
} | null = null;

function init(): void
{
    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
        text: document.getElementById(IDS.text) as HTMLElement,
        fill: document.getElementById(IDS.fill) as HTMLElement,
    };
}

export function submit(state: CalendarState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    set_text(el.text, `Day ${state.day}`);
    const width = `${Math.round(100 * state.progress)}%`;
    if (el.fill.style.width !== width) el.fill.style.width = width;
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

init();
