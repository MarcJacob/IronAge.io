// Lobby screen: countdown to match start, lobby player count, and the notice of the previous match's end.
// Same approach as game_ui.ts (static DOM in index.html, refreshed from here). DOM is only touched when the text actually changed.

import * as Backend from "../backend.js"

let lobbyElement: HTMLElement | null = null;
let noticeElement: HTMLElement | null = null;
let countdownElement: HTMLElement | null = null;
let playersElement: HTMLElement | null = null;

// performance.now() timestamp at which the countdown reaches 0. Null when the lobby is not shown.
let countdownEndMs: number | null = null;

export function init_lobby_ui(): void
{
    lobbyElement = document.getElementById("lobby_ui") as HTMLElement;
    noticeElement = document.getElementById("lobby_ui_notice") as HTMLElement;
    countdownElement = document.getElementById("lobby_ui_countdown") as HTMLElement;
    playersElement = document.getElementById("lobby_ui_players") as HTMLElement;
}

function set_text(element: HTMLElement, text: string): void
{
    if (element.textContent !== text) element.textContent = text;
}

// Shows the lobby screen with the state the server just sent. The countdown then runs locally until the next message.
// The match ended notice, if any, stays until the lobby is hidden by a match start.
export function show_lobby(state: Backend.LobbyState): void
{
    if (lobbyElement === null || playersElement === null) return;

    countdownEndMs = performance.now() + state.countdown_remaining_ms;
    // Stopgap: high-level game state should later be managed by the client backend instead of read from TS.
    set_text(playersElement, `Lobby #${state.match_slot_index} - Players ${state.player_count} / ${state.match_start_params.player_count}`);
    lobbyElement.hidden = false;
    update_lobby_ui();
}

// Shows only the match ended notice (no countdown yet), until the next show_lobby.
export function show_match_ended(winner_id: number | null): void
{
    if (lobbyElement === null || noticeElement === null || countdownElement === null || playersElement === null) return;

    countdownEndMs = null;
    set_text(noticeElement, winner_id === null ? "Match ended. No winner." : `Match ended. Winner: player ${winner_id}.`);
    noticeElement.hidden = false;
    set_text(countdownElement, "");
    set_text(playersElement, "Returning to lobby...");
    lobbyElement.hidden = false;
}

export function hide_lobby(): void
{
    if (lobbyElement === null || noticeElement === null) return;

    countdownEndMs = null;
    noticeElement.hidden = true;
    lobbyElement.hidden = true;
}

// Per-frame refresh of the countdown (mm:ss, rounded up).
export function update_lobby_ui(): void
{
    if (countdownElement === null || countdownEndMs === null) return;

    const remainingS = Math.ceil(Math.max(0, countdownEndMs - performance.now()) / 1000);
    const seconds = remainingS % 60;
    set_text(countdownElement, `${Math.floor(remainingS / 60)}:${seconds < 10 ? "0" : ""}${seconds}`);
}
