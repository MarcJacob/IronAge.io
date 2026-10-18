// Base mode while no match is running: shows the lobby (countdown, player count) and the previous match's end notice if any.
// Starts the game when a match starts.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import { show_lobby, show_match_ended, hide_lobby, update_lobby_ui } from "../ui/lobby_ui.js"
import { GameBase } from "./game_base.js"

export class LobbyBase extends Core.Mode
{
    // match_ended: set when entering the lobby because of a match end. winner_id null = no winner.
    constructor(private match_ended: { winner_id: number | null } | null = null) { super(); }

    on_entry(): void
    {
        if (this.match_ended !== null) show_match_ended(this.match_ended.winner_id);
    }

    on_exit(): void
    {
        hide_lobby();
    }

    on_frame_update(delta_time_s: number): void
    {
        update_lobby_ui();
    }

    // The base mode can't be escaped out of.
    on_escape(): boolean { return true; }

    on_net_message(message_type: number): void
    {
        if (message_type === Backend.MESSAGE_TYPE.LOBBY_JOINED)
        {
            show_lobby(Backend.read_lobby_joined_payload());
        }
        else if (message_type === Backend.MESSAGE_TYPE.MATCH_STARTED)
        {
            // The local player's id only reaches the frontend through the render state (main.ts updated it before dispatching).
            if (Backend.LAST_RENDER_STATE === null) return;

            Core.start_mode_stack(new GameBase(Backend.LAST_RENDER_STATE.controlled_player_id, Backend.read_match_started_placement_countdown_ms()));
        }
    }
}
