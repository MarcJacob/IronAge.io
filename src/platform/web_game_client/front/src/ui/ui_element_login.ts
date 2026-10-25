// UI module (new style): the login screen. Name field + log in button, a status line and an error line.
// Static markup in index.html (ids below), extracted once by init. submit(state) shows it, submit(null) hides it.
// Emits "login" with the entered name (trimmed, may be empty: the server then picks one). Never talks to the network.

import { UiEvents } from "./ui_events.js"

// Every DOM id this module uses (index.html).
const IDS = {
    root: "login",
    form: "login_form",
    name: "login_name",
    button: "login_button",
    status: "login_status",
    error: "login_error",
};

export interface LoginState
{
    can_log_in: boolean; // Whether the button is enabled (connected and not logging in yet).
    status: string; // e.g. "Connecting...". Empty hides it.
    error: string | null; // Null hides it.
}

const events = new UiEvents<{
    login: (name: string) => void;
}>();
export const on = events.on.bind(events);

let el: {
    root: HTMLElement;
    name: HTMLInputElement;
    button: HTMLButtonElement;
    status: HTMLElement;
    error: HTMLElement;
} | null = null;

export function init(): void
{
    const form = document.getElementById(IDS.form) as HTMLFormElement;
    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
        name: document.getElementById(IDS.name) as HTMLInputElement,
        button: document.getElementById(IDS.button) as HTMLButtonElement,
        status: document.getElementById(IDS.status) as HTMLElement,
        error: document.getElementById(IDS.error) as HTMLElement,
    };

    // A form, so Enter in the name field logs in too.
    form.addEventListener("submit", (event) => {
        event.preventDefault();
        if (el === null || el.button.disabled) return;
        events.emit("login", el.name.value.trim());
    });
}

export function submit(state: LoginState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    el.button.disabled = !state.can_log_in;
    set_text(el.status, state.status);
    el.status.hidden = state.status === "";
    set_text(el.error, state.error ?? "");
    el.error.hidden = state.error === null;
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}
