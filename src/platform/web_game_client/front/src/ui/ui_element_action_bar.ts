// UI element: the bottom action bar (container in index.html, buttons built here). Shows the actions of the current mode.
// A mode keeps its own full ActionBarState, submits it with submit_action_bar when it should be shown (entry, resume) and submits null to clear the bar.
// Only generates signals: pressing a button calls the item's on_activate. Hotkeys are handled by the modes, the hotkey of an item is only shown.

// How an item renders: selectable = normal, unselectable = disabled, selected = highlighted (e.g. the action whose mode is active).
export type ActionBarItemState = 'selectable' | 'unselectable' | 'selected';

export interface ActionBarItem
{
    name: string;
    hotkey?: string; // KeyboardEvent.code. Shown in the button text, not handled here.
    state: ActionBarItemState;
    on_activate: () => void; // Called when the button is pressed.
}

export interface ActionBarState
{
    items: ActionBarItem[];
}

let barElement: HTMLElement | null = null;
let hudElement: HTMLElement | null = null;

export function init_ui_element_action_bar(): void
{
    barElement = document.getElementById("game_hud_action_bar") as HTMLElement;
    hudElement = document.getElementById("game_hud") as HTMLElement;
}

// Text of a key for display: "Digit1" -> "1", "KeyR" -> "R", "NumpadAdd" -> "Num Add". Other codes are shown as they are.
export function hotkey_text(hotkey: string): string
{
    if (hotkey.startsWith("Digit")) return hotkey.slice("Digit".length);
    if (hotkey.startsWith("Key")) return hotkey.slice("Key".length);
    if (hotkey.startsWith("Numpad")) return "Num " + hotkey.slice("Numpad".length);
    return hotkey;
}

// Makes the bar show the given state, replacing whatever it showed. Null clears the bar (hidden).
export function submit_action_bar(state: ActionBarState | null): void
{
    if (barElement === null || hudElement === null) return;

    barElement.replaceChildren();
    barElement.hidden = state === null;
    hudElement.classList.toggle("has_bottom_bar", state !== null); // Makes room for the bar.
    if (state === null) return;

    for (const item of state.items)
    {
        const button = document.createElement("button");
        button.textContent = item.hotkey !== undefined ? `${item.name} [${hotkey_text(item.hotkey)}]` : item.name;
        button.disabled = item.state === 'unselectable';
        button.classList.toggle("selected", item.state === 'selected');

        // Placeholder until the bar has named sections: items with a hotkey on the left, items without one on the right.
        button.classList.toggle("bar_right", item.hotkey === undefined);

        button.addEventListener('click', () => { button.blur(); item.on_activate(); });
        barElement.appendChild(button);
    }
}
