// UI module (new style): the generic tooltip, one floating window of text lines (static element in index.html).
// Anything hoverable submits it: submit({ lines, x, y }) shows it near the given mouse position, submit(null) hides it.
// Pure display, emits nothing, and it never takes mouse input (so it can't steal the hover that opened it). Submitting the same state again does nothing:
// the DOM is only touched when the lines or the position change, so resubmitting every frame is fine.

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_tooltip",
};

export interface TooltipState
{
    lines: string[];
    x: number; // Mouse position in client coordinates.
    y: number;
}

const MOUSE_OFFSET = 14; // Gap between the mouse and the window.
const SCREEN_MARGIN = 8;

let el: {
    root: HTMLElement;
} | null = null;
let shown_text = "";
let shown_x = -1;
let shown_y = -1;

function init(): void
{
    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
    };
}

export function submit(state: TooltipState | null): void
{
    if (el === null) return;

    if (state === null || state.lines.length === 0)
    {
        el.root.hidden = true;
        shown_text = "";
        return;
    }

    const text = state.lines.join("\n");
    if (!el.root.hidden && text === shown_text && state.x === shown_x && state.y === shown_y) return;

    el.root.hidden = false;
    if (text !== shown_text) el.root.textContent = text;
    shown_text = text;
    shown_x = state.x;
    shown_y = state.y;

    // Right and below the mouse, flipped to the left / above where it would leave the screen.
    const width = el.root.offsetWidth;
    const height = el.root.offsetHeight;
    let left = state.x + MOUSE_OFFSET;
    let top = state.y + MOUSE_OFFSET;
    if (left + width > window.innerWidth - SCREEN_MARGIN) left = state.x - MOUSE_OFFSET - width;
    if (top + height > window.innerHeight - SCREEN_MARGIN) top = state.y - MOUSE_OFFSET - height;
    el.root.style.left = `${Math.max(SCREEN_MARGIN, left)}px`;
    el.root.style.top = `${Math.max(SCREEN_MARGIN, top)}px`;
}

init();
