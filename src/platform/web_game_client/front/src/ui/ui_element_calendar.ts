// UI element: the calendar at the top of the screen (container in index.html, content built here).
// Shows the current day as text and how far it has gone as a bar. Pure display, emits nothing.
// submit_calendar can be called every frame (the DOM is only touched when something changes); null clears it.

export interface CalendarState
{
    day: number; // Day to display.
    progress: number; // 0..1, how far the day has gone.
}

let calendarElement: HTMLElement | null = null;
let textElement: HTMLElement | null = null;
let fillElement: HTMLElement | null = null;

export function init_ui_element_calendar(): void
{
    calendarElement = document.getElementById("game_hud_calendar") as HTMLElement;
    textElement = document.getElementById("game_hud_calendar_text") as HTMLElement;
    fillElement = document.getElementById("game_hud_calendar_fill") as HTMLElement;
}

// Makes the calendar show the given state. Null clears it (hidden).
export function submit_calendar(state: CalendarState | null): void
{
    if (calendarElement === null || textElement === null || fillElement === null) return;

    if (state === null)
    {
        if (!calendarElement.hidden) calendarElement.hidden = true;
        return;
    }

    calendarElement.hidden = false;
    const text = `Day ${state.day}`;
    if (textElement.textContent !== text) textElement.textContent = text;
    const width = `${Math.round(100 * state.progress)}%`;
    if (fillElement.style.width !== width) fillElement.style.width = width;
}
