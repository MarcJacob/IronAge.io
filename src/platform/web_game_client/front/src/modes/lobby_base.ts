// Base mode while no match is running: the connection flow. Shows the screen matching the client state
// (connecting / authenticating -> login, lobby list -> lobby list, joining / in lobby -> lobby) and turns the screens' events into requests.
// Screens are refreshed every frame from the backend's client state. Starts the game when a match starts.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import { get_socket_status } from "../main.js"
import * as LoginUI from "../ui/ui_element_login.js"
import * as LobbyListUI from "../ui/ui_element_lobby_list.js"
import * as LobbyUI from "../ui/ui_element_lobby.js"
import { GameBase } from "./game_base.js"

const CLIENT_STATE = Backend.CLIENT_STATE;

// Sends a request built by the backend. Null = the client state does not allow it: nothing to send.
function send(request: Core.ByteBuffer | null): void
{
    if (request !== null) Core.send_message(request);
}

function join_error_text(result: Backend.JOIN_LOBBY_RESULT): string | null
{
    switch (result)
    {
        case Backend.JOIN_LOBBY_RESULT.SUCCESS: return null;
        case Backend.JOIN_LOBBY_RESULT.NOT_AUTHENTICATED: return "Could not join: not logged in.";
        case Backend.JOIN_LOBBY_RESULT.INVALID_LOBBY: return "Could not join: this lobby is not available anymore.";
        case Backend.JOIN_LOBBY_RESULT.LOBBY_FULL: return "Could not join: this lobby is full.";
        case Backend.JOIN_LOBBY_RESULT.ALREADY_IN_LOBBY: return "Could not join: already in a lobby.";
        default: return "Could not join the lobby.";
    }
}

function kick_text(reason: Backend.LOBBY_KICK_REASON): string
{
    switch (reason)
    {
        case Backend.LOBBY_KICK_REASON.PLAYER_CAP_LOWERED: return "You were removed from the lobby: its player cap was lowered.";
        default: return "You were removed from the lobby.";
    }
}

export class LobbyBase extends Core.Mode
{
    // Notice of the previous match's end, shown on the lobby list until the next join.
    private match_ended_notice: string | null;
    // performance.now() of the last lobby state received, to run the countdown locally.
    private lobby_state_received_ms: number = 0;
    private unsubscribe: (() => void)[] = [];

    // match_ended: set when entering because of a match end. winner_id null = no winner.
    constructor(match_ended: { winner_id: number | null } | null = null)
    {
        super();
        this.match_ended_notice = match_ended === null ? null
            : match_ended.winner_id === null ? "Match ended. No winner." : `Match ended. Winner: player ${match_ended.winner_id}.`;
    }

    on_entry(): void
    {
        this.unsubscribe = [
            LoginUI.on("login", (name) => send(Backend.build_request_authentication(name))),
            LobbyListUI.on("join", (lobby_id) => { this.match_ended_notice = null; send(Backend.build_request_join_lobby(lobby_id)); }),
            LobbyUI.on("leave", () => send(Backend.build_signal_leave_lobby())),
            LobbyUI.on("start", () => send(Backend.build_lobby_admin_start_match())),
            LobbyUI.on("set_player_count", (player_count) => send(Backend.build_lobby_admin_set_player_count(player_count))),
            LobbyUI.on("set_map_size", (width, height) => send(Backend.build_lobby_admin_set_map_size(width, height))),
            LobbyUI.on("set_seed", (seed) => send(Backend.build_lobby_admin_set_seed(seed))),
        ];
        this.refresh();
    }

    on_exit(): void
    {
        for (const unsubscribe of this.unsubscribe) unsubscribe();
        this.unsubscribe = [];

        LoginUI.submit(null);
        LobbyListUI.submit(null);
        LobbyUI.submit(null);
    }

    on_frame_update(delta_time_s: number): void
    {
        this.refresh();
    }

    // The base mode can't be escaped out of.
    on_escape(): boolean { return true; }

    on_net_message(message_type: number): void
    {
        if (message_type === Backend.MESSAGE_TYPE.LOBBY_JOINED)
        {
            this.lobby_state_received_ms = performance.now();
        }
        else if (message_type === Backend.MESSAGE_TYPE.MATCH_STARTED)
        {
            // The local player's id only reaches the frontend through the render state (main.ts updated it before dispatching).
            if (Backend.LAST_RENDER_STATE === null) return;

            Core.start_mode_stack(new GameBase(Backend.LAST_RENDER_STATE.controlled_player_id, Backend.read_match_started_placement_countdown_ms()));
        }
    }

    // Shows the screen of the current client state, hides the others.
    private refresh(): void
    {
        const state = Backend.get_client_state();

        LoginUI.submit(state === CLIENT_STATE.CONNECTING || state === CLIENT_STATE.AUTHENTICATING ? this.login_state(state) : null);
        LobbyListUI.submit(state === CLIENT_STATE.LOBBY_LIST ? this.lobby_list_state() : null);
        LobbyUI.submit(state === CLIENT_STATE.JOINING_LOBBY || state === CLIENT_STATE.IN_LOBBY ? this.lobby_state(state) : null);
    }

    private login_state(state: Backend.CLIENT_STATE): LoginUI.LoginState
    {
        const socket = get_socket_status();
        if (socket === 'closed') return { can_log_in: false, status: "", error: "Connection lost. Reload the page to reconnect." };
        if (socket === 'connecting') return { can_log_in: false, status: "Connecting to the server...", error: null };
        if (state === CLIENT_STATE.AUTHENTICATING) return { can_log_in: false, status: "Logging in...", error: null };
        return { can_log_in: true, status: "", error: null };
    }

    private lobby_list_state(): LobbyListUI.LobbyListState
    {
        const lobbies: LobbyListUI.LobbyListEntryState[] = [];
        for (const lobby of Backend.read_lobby_list())
        {
            lobbies[lobby.match_slot_index] = {
                human_count: lobby.player_count,
                player_cap: lobby.match_start_params.player_count,
                map_width: lobby.match_start_params.world_size_regions_x,
                map_height: lobby.match_start_params.world_size_regions_y,
                test_mode: lobby.test_mode,
                full: Backend.is_lobby_full(lobby),
            };
        }

        const kickReason = Backend.read_kick_notice();
        return {
            own_name: Backend.read_own_name(),
            lobbies,
            notice: kickReason !== null ? kick_text(kickReason) : this.match_ended_notice,
            error: get_socket_status() === 'closed' ? "Connection lost. Reload the page to reconnect." : join_error_text(Backend.read_join_response().result),
        };
    }

    private lobby_state(state: Backend.CLIENT_STATE): LobbyUI.LobbyState
    {
        const lobby = Backend.read_lobby_joined_payload();
        // The lobby state follows the join response: until it arrives it is zeroed (player cap 0), so the screen still shows "joining".
        const joining = state === CLIENT_STATE.JOINING_LOBBY || lobby.match_start_params.player_count === 0;

        return {
            lobby_id: joining ? null : lobby.match_slot_index,
            test_mode: lobby.test_mode,
            human_count: lobby.player_count,
            player_cap: lobby.match_start_params.player_count,
            map_width: lobby.match_start_params.world_size_regions_x,
            map_height: lobby.match_start_params.world_size_regions_y,
            seed: lobby.match_start_params.random_seed,
            countdown_ms: lobby.countdown_remaining_ms === 0 ? null : lobby.countdown_remaining_ms - (performance.now() - this.lobby_state_received_ms),
        };
    }
}
