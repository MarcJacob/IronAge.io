// UI module (new style): the action bar. Shows the actions of the current mode, one button each.
// Static container and ONE template button in index.html (ids below), extracted once by init. The buttons are the dynamic part: cloned from the
// template for every item of the submitted state, and rebuilt when the items change (the bar is small; this is the example of the dynamic-element approach).
// submit(state) shows the bar, submit(null) hides it. Emits "action" with the item's id when its button is pressed. Hotkeys are handled by the modes,
// the hotkey of an item is only shown.

import { UiEvents } from "./ui_events.js"

// Every DOM id this module uses (index.html).
const IDS = {
    root: "game_hud_action_bar",
    button_template: "game_hud_action_bar_template",
};

// How an item renders: selectable = normal, unselectable = disabled, selected = highlighted (e.g. the action whose mode is active).
export type ActionBarItemState = 'selectable' | 'unselectable' | 'selected';

export interface ActionBarItem
{
    id: string; // Given back by the "action" event.
    name: string;
    hotkey?: string; // KeyboardEvent.code. Shown in the button text, not handled here.
    state: ActionBarItemState;
}

export interface ActionBarState
{
    items: ActionBarItem[];
}

const events = new UiEvents<{
    action: (id: string) => void;
}>();
export const on = events.on.bind(events);

let el: {
    root: HTMLElement;
    button_template: HTMLElement;
} | null = null;

let buttons: HTMLElement[] = []; // The cloned buttons currently in the bar.
let shown = ""; // Description of the items the buttons were built from.

function init(): void
{
    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
        button_template: document.getElementById(IDS.button_template) as HTMLElement,
    };
}

// Text of a key for display: "Digit1" -> "1", "KeyR" -> "R", "NumpadAdd" -> "Num Add". Other codes are shown as they are.
function hotkey_text(hotkey: string): string
{
    if (hotkey.startsWith("Digit")) return hotkey.slice("Digit".length);
    if (hotkey.startsWith("Key")) return hotkey.slice("Key".length);
    if (hotkey.startsWith("Numpad")) return "Num " + hotkey.slice("Numpad".length);
    return hotkey;
}

export function submit(state: ActionBarState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;

    const items = state === null ? [] : state.items;
    const description = items.map(item => `${item.id}|${item.name}|${item.hotkey}|${item.state}`).join("\n");
    if (description === shown) return;
    shown = description;

    for (const button of buttons) button.remove();
    buttons = items.map(item => {
        const button = el!.button_template.cloneNode(true) as HTMLButtonElement;
        button.removeAttribute("id");
        button.hidden = false;
        button.textContent = item.hotkey !== undefined ? `${item.name} [${hotkey_text(item.hotkey)}]` : item.name;
        button.disabled = item.state === 'unselectable';
        button.classList.toggle("selected", item.state === 'selected');

        // Placeholder until the bar has named sections: items with a hotkey on the top, items without one on the bottom.
        button.classList.toggle("bar_bottom", item.hotkey === undefined);

        button.addEventListener("click", () => { button.blur(); events.emit("action", item.id); });
        el!.root.appendChild(button);
        return button;
    });
}

init();
