// Base mode while a match is running. Lasts until the match ends. Starts in the spawn pick (placement) mode, which main play replaces.
// Owns what is common to the whole match: camera input.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import { update_camera_input, clear_selected_entity } from "../input.js"
import { LobbyBase } from "./lobby_base.js"
import { SpawnPick } from "./spawn_pick.js"

export class GameBase extends Core.Mode
{
    constructor(private local_player_id: number, private placement_countdown_ms: number) { super(); }

    on_entry(): void
    {
        Core.push_mode(new SpawnPick(this.local_player_id, this.placement_countdown_ms), this);
    }

    on_exit(): void
    {
        clear_selected_entity();
    }

    on_frame_update(delta_time_s: number): void
    {
        update_camera_input(delta_time_s);
    }

    // The base mode can't be escaped out of.
    on_escape(): boolean { return true; }

    on_net_message(message_type: number): void
    {
        if (message_type === Backend.MESSAGE_TYPE.MATCH_ENDED)
        {
            // Reads the net buffer: main.ts dispatches this before dropping the match.
            Core.start_mode_stack(new LobbyBase({ winner_id: Backend.read_match_ended_winner() }));
        }
    }
}
