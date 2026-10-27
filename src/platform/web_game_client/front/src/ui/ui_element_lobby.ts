// UI module (new style): the lobby screen. Title, status line (joining / countdown / waiting), player line, Leave button,
// and in a test lobby only the admin controls (player count, map size, seed, Start).
// Static markup in index.html (ids below), extracted once by init. submit(state) shows it, submit(null) hides it.
// Emits user actions (events below). Never talks to the network.
// Admin controls show the server's values, except while the user edits one: a control that has focus, or was changed
// less than EDIT_HOLD_MS ago, keeps the user's value (so server updates don't fight a drag, and the reply has time to arrive).

import * as Backend from "../backend.js"
import { UiEvents } from "./ui_events.js"

// Every DOM id this module uses (index.html).
const IDS = {
    root: "lobby",
    title: "lobby_title",
    status: "lobby_status",
    players: "lobby_players",
    leave: "lobby_leave",
    admin: "lobby_admin",
    player_count: "lobby_admin_player_count",
    player_count_value: "lobby_admin_player_count_value",
    map_width: "lobby_admin_map_width",
    map_width_value: "lobby_admin_map_width_value",
    map_height: "lobby_admin_map_height",
    map_height_value: "lobby_admin_map_height_value",
    seed: "lobby_admin_seed",
    tick_rate: "lobby_admin_tick_rate",
    tick_rate_value: "lobby_admin_tick_rate_value",
    start: "lobby_admin_start",
};

const EDIT_HOLD_MS = 1000;
const SEED_MAX = 0xFFFFFFFF;

export interface LobbyState
{
    lobby_id: number | null; // Null while joining (no lobby state yet): only the status line shows.
    test_mode: boolean; // Shows the admin controls, no countdown.
    human_count: number;
    player_cap: number;
    map_width: number; // Regions.
    map_height: number;
    seed: number;
    tick_rate: number; // Ticks per second.
    countdown_ms: number | null; // Remaining before the match starts. Null = no countdown running.
}

const events = new UiEvents<{
    leave: () => void;
    start: () => void;
    set_player_count: (player_count: number) => void;
    set_map_size: (width_regions: number, height_regions: number) => void;
    set_seed: (seed: number) => void;
    set_tick_rate: (tick_rate: number) => void;
}>();
export const on = events.on.bind(events);

let el: {
    root: HTMLElement;
    title: HTMLElement;
    status: HTMLElement;
    players: HTMLElement;
    leave: HTMLButtonElement;
    admin: HTMLElement;
    player_count: HTMLInputElement;
    player_count_value: HTMLElement;
    map_width: HTMLInputElement;
    map_width_value: HTMLElement;
    map_height: HTMLInputElement;
    map_height_value: HTMLElement;
    seed: HTMLInputElement;
    tick_rate: HTMLInputElement;
    tick_rate_value: HTMLElement;
    start: HTMLButtonElement;
} | null = null;

// performance.now() until which a control keeps the user's value, per control.
const editHoldUntilMs = new Map<HTMLInputElement, number>();

function init(): void
{
    const get = (id: string) => document.getElementById(id) as HTMLElement;
    el = {
        root: get(IDS.root),
        title: get(IDS.title),
        status: get(IDS.status),
        players: get(IDS.players),
        leave: get(IDS.leave) as HTMLButtonElement,
        admin: get(IDS.admin),
        player_count: get(IDS.player_count) as HTMLInputElement,
        player_count_value: get(IDS.player_count_value),
        map_width: get(IDS.map_width) as HTMLInputElement,
        map_width_value: get(IDS.map_width_value),
        map_height: get(IDS.map_height) as HTMLInputElement,
        map_height_value: get(IDS.map_height_value),
        seed: get(IDS.seed) as HTMLInputElement,
        tick_rate: get(IDS.tick_rate) as HTMLInputElement,
        tick_rate_value: get(IDS.tick_rate_value),
        start: get(IDS.start) as HTMLButtonElement,
    };
    const e = el;

    e.player_count.max = String(Backend.LOBBY_MAX_PLAYER_COUNT);
    e.map_width.max = String(Backend.LOBBY_MAX_MAP_SIZE_REGIONS);
    e.map_height.max = String(Backend.LOBBY_MAX_MAP_SIZE_REGIONS);
    e.tick_rate.min = String(Backend.LOBBY_MIN_TICK_RATE);
    e.tick_rate.max = String(Backend.LOBBY_MAX_TICK_RATE);

    e.leave.addEventListener("click", () => { e.leave.blur(); events.emit("leave"); });
    e.start.addEventListener("click", () => { e.start.blur(); events.emit("start"); });

    // Sliders: the value label follows the drag, the event is sent on release only.
    bind_slider(e.player_count, e.player_count_value, () => events.emit("set_player_count", e.player_count.valueAsNumber));
    bind_slider(e.map_width, e.map_width_value, () => events.emit("set_map_size", e.map_width.valueAsNumber, e.map_height.valueAsNumber));
    bind_slider(e.map_height, e.map_height_value, () => events.emit("set_map_size", e.map_width.valueAsNumber, e.map_height.valueAsNumber));

    // Seed: sent when committed (Enter or leaving the field). Invalid values are not sent (the server's value comes back once the hold ends).
    e.seed.addEventListener("change", () => {
        hold(e.seed);
        const seed = Number(e.seed.value);
        if (e.seed.value !== "" && Number.isInteger(seed) && seed >= 0 && seed <= SEED_MAX) events.emit("set_seed", seed);
    });

    bind_slider(e.tick_rate, e.tick_rate_value, () => events.emit("set_tick_rate", e.tick_rate.valueAsNumber));
}

function bind_slider(input: HTMLInputElement, label: HTMLElement, on_release: () => void): void
{
    input.addEventListener("input", () => { hold(input); set_text(label, input.value); });
    // Blur: gives the keys back to the camera / modes, and lets server values in again once the hold ends.
    input.addEventListener("change", () => { hold(input); input.blur(); on_release(); });
}

function hold(input: HTMLInputElement): void
{
    editHoldUntilMs.set(input, performance.now() + EDIT_HOLD_MS);
}

function is_being_edited(input: HTMLInputElement): boolean
{
    return document.activeElement === input || performance.now() < (editHoldUntilMs.get(input) ?? 0);
}

// Shows the server's value in a control (and its label), unless the user is editing it.
function set_control(input: HTMLInputElement, label: HTMLElement | null, value: number): void
{
    if (!is_being_edited(input) && input.value !== String(value)) input.value = String(value);
    if (label !== null) set_text(label, input.value);
}

export function submit(state: LobbyState | null): void
{
    if (el === null) return;

    el.root.hidden = state === null;
    if (state === null) return;

    const joining = state.lobby_id === null;
    el.title.hidden = joining;
    el.players.hidden = joining;
    el.leave.hidden = joining;
    el.admin.hidden = joining || !state.test_mode;

    set_text(el.status, status_text(state));
    if (state.lobby_id === null) return;

    set_text(el.title, `${state.test_mode ? "Test lobby" : "Lobby"} ${state.lobby_id + 1}`);
    set_text(el.players, `Humans ${state.human_count} / ${state.player_cap} players (AI fills the rest) - Map ${state.map_width}x${state.map_height}`);

    if (!state.test_mode) return;
    set_control(el.player_count, el.player_count_value, state.player_cap);
    set_control(el.map_width, el.map_width_value, state.map_width);
    set_control(el.map_height, el.map_height_value, state.map_height);
    set_control(el.seed, null, state.seed);
    set_control(el.tick_rate, el.tick_rate_value, state.tick_rate);
}

function status_text(state: LobbyState): string
{
    if (state.lobby_id === null) return "Joining lobby...";
    if (state.test_mode) return "Waiting for an admin to start";
    if (state.countdown_ms === null) return "Waiting for players";

    const remainingS = Math.ceil(Math.max(0, state.countdown_ms) / 1000);
    const seconds = remainingS % 60;
    return `Starting in ${Math.floor(remainingS / 60)}:${seconds < 10 ? "0" : ""}${seconds}`;
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

init();
