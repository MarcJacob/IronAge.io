// UI module (new style): the lobby list screen. One static entry per lobby slot (lobby id = slot index = entry index),
// the own name, a notice line (match ended, kicked), an error line (join refused) and an empty-list text.
// Static markup in index.html (ids / classes below), extracted once by init. submit(state) shows it, submit(null) hides it.
// Emits "join" with the lobby id of the entry whose Join button was pressed. Never talks to the network.

import { UiEvents } from "./ui_events.js"

// Every DOM id / class this module uses (index.html). Entry i is ENTRY_ID_PREFIX + i, its parts are found by class inside it.
const IDS = {
    root: "lobby_list",
    own_name: "lobby_list_own_name",
    notice: "lobby_list_notice",
    error: "lobby_list_error",
    empty: "lobby_list_empty",
    entry_prefix: "lobby_list_entry_",
};
const ENTRY_CLASSES = {
    players: "lobby_list_entry_players",
    map: "lobby_list_entry_map",
    test: "lobby_list_entry_test",
    join: "lobby_list_entry_join",
};

// Static entries in index.html: one per lobby slot of the server (4 for now).
const ENTRY_COUNT = 4;

export interface LobbyListEntryState
{
    human_count: number;
    player_cap: number;
    map_width: number; // Regions.
    map_height: number;
    test_mode: boolean;
    full: boolean; // Join disabled.
}

export interface LobbyListState
{
    own_name: string;
    lobbies: (LobbyListEntryState | null)[]; // Index = lobby id. Null / missing = no lobby open in that slot (entry hidden). Beyond ENTRY_COUNT: ignored.
    notice: string | null; // Null hides it.
    error: string | null; // Null hides it.
}

const events = new UiEvents<{
    join: (lobby_id: number) => void;
}>();
export const on = events.on.bind(events);

interface EntryElements
{
    root: HTMLElement;
    players: HTMLElement;
    map: HTMLElement;
    test: HTMLElement;
    join: HTMLButtonElement;
}

let el: {
    root: HTMLElement;
    own_name: HTMLElement;
    notice: HTMLElement;
    error: HTMLElement;
    empty: HTMLElement;
    entries: EntryElements[];
} | null = null;

function init(): void
{
    const entries: EntryElements[] = [];
    for (let i = 0; i < ENTRY_COUNT; i++)
    {
        const root = document.getElementById(IDS.entry_prefix + i) as HTMLElement;
        const part = (cls: string) => root.querySelector("." + cls) as HTMLElement;
        const entry: EntryElements = {
            root,
            players: part(ENTRY_CLASSES.players),
            map: part(ENTRY_CLASSES.map),
            test: part(ENTRY_CLASSES.test),
            join: part(ENTRY_CLASSES.join) as HTMLButtonElement,
        };
        entry.join.addEventListener("click", () => { entry.join.blur(); events.emit("join", i); });
        entries.push(entry);
    }

    el = {
        root: document.getElementById(IDS.root) as HTMLElement,
        own_name: document.getElementById(IDS.own_name) as HTMLElement,
        notice: document.getElementById(IDS.notice) as HTMLElement,
        error: document.getElementById(IDS.error) as HTMLElement,
        empty: document.getElementById(IDS.empty) as HTMLElement,
        entries,
    };
}

export function submit(state: LobbyListState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    set_text(el.own_name, `Logged in as ${state.own_name}`);
    set_text(el.notice, state.notice ?? "");
    el.notice.hidden = state.notice === null;
    set_text(el.error, state.error ?? "");
    el.error.hidden = state.error === null;

    let shownCount = 0;
    for (let i = 0; i < ENTRY_COUNT; i++)
    {
        const entry = el.entries[i];
        const lobby = state.lobbies[i] ?? null;

        entry.root.hidden = lobby === null;
        if (lobby === null) continue;
        shownCount++;

        set_text(entry.players, `Players ${lobby.human_count} / ${lobby.player_cap}`);
        entry.players.classList.toggle("full", lobby.full);
        set_text(entry.map, `Map ${lobby.map_width}x${lobby.map_height}`);
        entry.test.hidden = !lobby.test_mode;
        set_text(entry.join, lobby.full ? "Full" : "Join");
        entry.join.disabled = lobby.full;
    }
    el.empty.hidden = shownCount > 0;
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

init();
