// Placement phase (between MATCH_STARTED and the first SERVER_TICK): the picks of every player, the click-to-pick input and the placement UI
// (message + emptying countdown bar). Picks are not secret and arrive from the server, own picks included, so dots only ever show server state.
// The countdown is indicative only: leaving placement is up to main.ts, on the first SERVER_TICK.

import * as Core from "./core.js"
import * as Backend from "./backend.js"

// Mirrors world_terrain::REGION_SIZE (include/game_common/match/world.h).
const REGION_SIZE = 128;

// Other players' picks within 0 tiles of the local pick are fully red, those further than half a region are gray, linear in between.
const NEAR_DISTANCE = 24;
const FAR_DISTANCE = REGION_SIZE / 2;
const DOT_ALPHA = 0.25;
const COLOR_OWN = [0, 255, 0];
const COLOR_NEAR = [255, 0, 0];
const COLOR_FAR = [128, 128, 128];

export interface PlacementDot
{
    x: number; // Tile.
    y: number;
    color: string; // CSS color, alpha included.
}

let IN_PLACEMENT = false;
let LOCAL_PLAYER_ID: number | null = null;
const PICKS = new Map<number, { x: number, y: number }>(); // Per player id.
let DOTS: PlacementDot[] = []; // Recomputed whenever a pick changes.

let SEND_MESSAGE: (bytes: Core.ByteBuffer) => void = () => {};

// performance.now() timestamp at which the countdown reaches 0, and its total duration.
let countdownEndMs = 0;
let countdownTotalMs = 1;

let uiElement: HTMLElement | null = null;
let barFillElement: HTMLElement | null = null;

export function init_placement(send_message: (bytes: Core.ByteBuffer) => void): void
{
    SEND_MESSAGE = send_message;
    uiElement = document.getElementById("placement_ui") as HTMLElement;
    barFillElement = document.getElementById("placement_ui_bar_fill") as HTMLElement;

    Core.register_frame_callback(update_placement_ui);
}

export function is_in_placement(): boolean
{
    return IN_PLACEMENT;
}

export function get_placement_dots(): PlacementDot[]
{
    return DOTS;
}

export function enter_placement(local_player_id: number, countdown_ms: number): void
{
    IN_PLACEMENT = true;
    LOCAL_PLAYER_ID = local_player_id;
    countdownTotalMs = Math.max(1, countdown_ms);
    countdownEndMs = performance.now() + countdown_ms;
    if (uiElement !== null) uiElement.hidden = false;
}

// Leaves placement mode and forgets all picks. Safe to call when not in placement.
export function exit_placement(): void
{
    IN_PLACEMENT = false;
    LOCAL_PLAYER_ID = null;
    PICKS.clear();
    DOTS = [];
    if (uiElement !== null) uiElement.hidden = true;
}

// A pick rebroadcast by the server. Ignored outside placement.
export function set_pick(pick: Backend.PlacementPick): void
{
    if (!IN_PLACEMENT) return;

    PICKS.set(pick.player_id, { x: pick.x, y: pick.y });
    recompute_dots();
}

// Asks the server to start at the given tile. The dot moves once the server rebroadcasts it.
export function send_pick(tile: { x: number, y: number }): void
{
    if (!IN_PLACEMENT || LOCAL_PLAYER_ID === null) return;

    SEND_MESSAGE(Backend.build_client_placement_pick(LOCAL_PLAYER_ID, tile.x, tile.y));
}

function recompute_dots(): void
{
    DOTS = [];
    const own = LOCAL_PLAYER_ID === null ? undefined : PICKS.get(LOCAL_PLAYER_ID);

    for (const [playerId, pick] of PICKS)
    {
        let color = COLOR_OWN;
        if (playerId !== LOCAL_PLAYER_ID)
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
        DOTS.push({ x: pick.x, y: pick.y, color: `rgba(${color[0]}, ${color[1]}, ${color[2]}, ${DOT_ALPHA})` });
    }
}

// Per-frame refresh of the countdown bar.
function update_placement_ui(): void
{
    if (!IN_PLACEMENT || barFillElement === null) return;

    const fraction = Math.min(1, Math.max(0, (countdownEndMs - performance.now()) / countdownTotalMs));
    barFillElement.style.width = `${fraction * 100}%`;
}
