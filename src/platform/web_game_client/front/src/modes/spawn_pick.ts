// Placement phase (between MATCH_STARTED and the first SERVER_TICK): the picks of every player, the click-to-pick input and the placement UI.
// Picks are not secret and arrive from the server, own picks included, so dots only ever show server state.
// The countdown is indicative only: the mode ends on the first SERVER_TICK, whatever the countdown says.

import * as Core from "../core.js"
import * as Backend from "../backend.js"
import * as Render from "../render.js"
import { show_placement_ui, hide_placement_ui, update_placement_ui } from "../ui/placement_ui.js"
import { MainPlay } from "./main_play.js"

// Mirrors world_terrain::REGION_SIZE (include/game_common/match/world/world_terrain.h).
const REGION_SIZE = 128;

// Other players' picks within 0 tiles of the local pick are fully red, those further than half a region are gray, linear in between.
const NEAR_DISTANCE = 24;
const FAR_DISTANCE = REGION_SIZE / 2;
const DOT_ALPHA = 0.25;
const COLOR_OWN = [0, 255, 0];
const COLOR_NEAR = [255, 0, 0];
const COLOR_FAR = [128, 128, 128];

export class SpawnPick extends Core.Mode
{
    private picks = new Map<number, { x: number, y: number }>(); // Per player id.

    constructor(private local_player_id: number, private countdown_ms: number) { super(); }

    on_entry(): void
    {
        show_placement_ui(this.countdown_ms);
        Render.RENDER_CONFIG.overlay_dots = [];
    }

    on_exit(): void
    {
        hide_placement_ui();
        Render.RENDER_CONFIG.overlay_dots = [];
    }

    on_frame_update(delta_time_s: number): void
    {
        update_placement_ui();
    }

    // Placement can't be escaped out of.
    on_escape(): boolean { return true; }

    // A left click anywhere on the map picks the start location, nothing else is available.
    on_click(event: MouseEvent): boolean
    {
        if (event.button !== 0) return true;

        const tile = Render.page_to_tile(event.clientX, event.clientY);
        if (Render.is_tile_in_world(tile)) this.send_pick(tile);
        return true;
    }

    on_net_message(message_type: number): void
    {
        if (message_type === Backend.MESSAGE_TYPE.SERVER_PLACEMENT_PICK)
        {
            const pick = Backend.read_server_placement_pick();
            this.picks.set(pick.player_id, { x: pick.x, y: pick.y });
            this.update_dots();
        }
        else if (message_type === Backend.MESSAGE_TYPE.SERVER_TICK)
        {
            // The first tick ends the placement phase: the match proceeds as an ordinary one.
            Core.replace_top_mode(new MainPlay());
        }
    }

    // Asks the server to start at the given tile. The dot moves once the server rebroadcasts it.
    private send_pick(tile: { x: number, y: number }): void
    {
        Core.send_message(Backend.build_client_placement_pick(this.local_player_id, tile.x, tile.y));
    }

    private update_dots(): void
    {
        const dots: Render.OverlayDot[] = [];
        const own = this.picks.get(this.local_player_id);

        for (const [playerId, pick] of this.picks)
        {
            let color = COLOR_OWN;
            if (playerId !== this.local_player_id)
            {
                // Without an own pick (yet), others are plain red.
                let farness = 0;
                if (own !== undefined)
                {
                    const distance = Math.hypot(pick.x - own.x, pick.y - own.y);
                    farness = Math.min(1, Math.max(0, (distance - NEAR_DISTANCE) / (FAR_DISTANCE - NEAR_DISTANCE)));
                }
                color = COLOR_NEAR.map((near, i) => near + (COLOR_FAR[i] - near) * farness);
            }
            dots.push({ x: pick.x, y: pick.y, color: `rgba(${color[0]}, ${color[1]}, ${color[2]}, ${DOT_ALPHA})` });
        }

        Render.RENDER_CONFIG.overlay_dots = dots;
    }
}
