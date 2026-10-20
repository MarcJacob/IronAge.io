// UI element: the generic tooltip, one floating window of text lines (container in index.html, content built here).
// Anything hoverable submits it: submit_tooltip({ lines, x, y }) shows it near the given mouse position, submit_tooltip(null) hides it.
// Pure display, it never takes mouse input (so it can't steal the hover that opened it). Submitting the same state again does nothing:
// the DOM is only touched when the lines or the position change, so resubmitting every frame is fine.

export interface TooltipState
{
    lines: string[];
    x: number; // Mouse position in client coordinates.
    y: number;
}

const MOUSE_OFFSET = 14; // Gap between the mouse and the window.
const SCREEN_MARGIN = 8;

let tooltipElement: HTMLElement | null = null;
let shown_text = "";
let shown_x = -1;
let shown_y = -1;

export function init_ui_element_tooltip(): void
{
    tooltipElement = document.getElementById("game_hud_tooltip") as HTMLElement;
}

// Makes the tooltip show the given state. Null hides it.
export function submit_tooltip(state: TooltipState | null): void
{
    if (tooltipElement === null) return;

    if (state === null || state.lines.length === 0)
    {
        if (!tooltipElement.hidden) tooltipElement.hidden = true;
        shown_text = "";
        return;
    }

    const text = state.lines.join("\n");
    if (!tooltipElement.hidden && text === shown_text && state.x === shown_x && state.y === shown_y) return;

    tooltipElement.hidden = false;
    if (text !== shown_text) tooltipElement.textContent = text;
    shown_text = text;
    shown_x = state.x;
    shown_y = state.y;

    // Right and below the mouse, flipped to the left / above where it would leave the screen.
    const width = tooltipElement.offsetWidth;
    const height = tooltipElement.offsetHeight;
    let left = state.x + MOUSE_OFFSET;
    let top = state.y + MOUSE_OFFSET;
    if (left + width > window.innerWidth - SCREEN_MARGIN) left = state.x - MOUSE_OFFSET - width;
    if (top + height > window.innerHeight - SCREEN_MARGIN) top = state.y - MOUSE_OFFSET - height;
    tooltipElement.style.left = `${Math.max(SCREEN_MARGIN, left)}px`;
    tooltipElement.style.top = `${Math.max(SCREEN_MARGIN, top)}px`;
}
