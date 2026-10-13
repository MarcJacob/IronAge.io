// Web client entry point: start-up order and the socket-driven message loop.
// Every received websocket message is handed to the backend to parse and apply; the backend's return value tells this
// layer what happened (see MESSAGE_TYPE), which is the only signal it reacts to. There is no separate JS-driven tick loop:
// the server drives ticks, the client just applies whatever it's told.

import * as Core from "./core.js"
import * as Backend from "./backend.js"

import { init_render } from './render.js';
import { init_input, clear_selected_entity } from './input.js';
import { init_camera_system } from './camera.js';
import { init_game_ui, update_game_ui, set_game_ui_visible } from './game_ui.js';
import { init_lobby_ui, show_lobby, show_match_ended, hide_lobby } from './lobby_ui.js';
import { init_placement, enter_placement, exit_placement, set_pick, is_in_placement } from './placement.js';

// Game / Client state

let IN_A_MATCH: boolean = false;

// ID of the player this client controls. Null until a match is joined.
let LOCAL_PLAYER_ID: number | null = null;
// Note: game_ui.ts imports this file (circular). Only call this inside per-frame functions, never at module load.
export function get_local_player_id(): number | null
{
    return LOCAL_PLAYER_ID;
}
// ...

function websocket_url() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${protocol}//${window.location.host}/ws`;
}

// Handler for messages received from the Websocket.
function on_socket_message(event: MessageEvent<Core.ByteBuffer>, canvas: HTMLCanvasElement, socket: WebSocket)
{
    const messageType = Backend.process_net_message(new Core.ByteBuffer(event.data));
    if (messageType === Backend.MESSAGE_TYPE.INVALID) return; // Message could not be processed: nothing changed.

    if (messageType === Backend.MESSAGE_TYPE.LOBBY_JOINED)
    {
        show_lobby(Backend.read_lobby_joined_payload());
    }

    if (messageType === Backend.MESSAGE_TYPE.SERVER_PLACEMENT_PICK)
    {
        set_pick(Backend.read_server_placement_pick()); // Ignored outside placement.
    }

    // The first tick ends the placement phase: the match proceeds as an ordinary one.
    if (messageType === Backend.MESSAGE_TYPE.SERVER_TICK && is_in_placement())
    {
        exit_placement();
        set_game_ui_visible(true);
    }

    // Have the backend drop its local match, clear what the frontend holds of it and fall back to the lobby screen.
    if (messageType === Backend.MESSAGE_TYPE.MATCH_ENDED)
    {
        console.log("Match ended.");

        IN_A_MATCH = false;
        LOCAL_PLAYER_ID = null;
        exit_placement();
        clear_selected_entity();

        canvas.hidden = true;
        set_game_ui_visible(false);
        show_match_ended(Backend.read_match_ended_winner()); // Reads the net buffer: keep before anything that could overwrite it.

        Backend.end_local_match();

        Core.broadcast_on_match_ended();
    }

    // On joining a match, reveal the canvas and start drawing. Rendering/input were already set up at start().
    if (messageType === Backend.MESSAGE_TYPE.MATCH_STARTED)
    {
        console.log("Joined match.");

        hide_lobby();

        // Perform the first render state update on the backend.
        Backend.update_render_state();

        // The local player's id only reaches the frontend through the render state.
        if (Backend.LAST_RENDER_STATE !== null) LOCAL_PLAYER_ID = Backend.LAST_RENDER_STATE.controlled_player_id;

        // Placement phase until the first SERVER_TICK (HUD stays hidden until then).
        if (LOCAL_PLAYER_ID !== null) enter_placement(LOCAL_PLAYER_ID, Backend.read_match_started_placement_countdown_ms());

        // Reveal game canvas
        canvas.hidden = false;

        // Automatically set focus on the foreground canvas.
        canvas.focus();

        // Broadcast to event handlers and flag app as In A Match.
        Core.broadcast_on_match_joined();
        IN_A_MATCH = true;
    }

    // The backend may have built a reply (e.g. pending input) in response to the message just processed.
    // There is no separate send loop: outgoing messages only ever go out piggybacked on a received one.
    const outgoing = Backend.read_pending_output_message();
    if (outgoing !== null) socket.send(outgoing);
}

// ENTRY POINT


function tick(dt: number)
{
    if (IN_A_MATCH)
    {
        Backend.tick_web_client(dt);
        Backend.update_render_state();
        update_game_ui();
    }
}

// Start routine for the webpage.
async function start()
{
    console.log("Loading client backend...");
    try {
        await Backend.load('IronAgeIO_WebClient.wasm');
    } catch (e) {
        console.error("Failed to load client backend.", e);
        return;
    }
    if (!Backend.start()) return;

    // Register our own tick function.
    Core.register_frame_callback(tick);

    // Drives per-frame callbacks: camera input, render/draw, later interpolation.
    Core.start_frame_loop();

    // Get foreground canvas. Rendering/input are set up immediately - both just register callbacks/state, neither
    // needs match data (render state, including world size, is read fresh every draw() call instead).
    const canvas = document.getElementById("game_canvas_foreground") as HTMLCanvasElement;
    init_render(canvas);
    init_input(canvas);
    init_camera_system();

    // Initialize in-match HUD.
    init_game_ui();
    init_lobby_ui();

    // Initialize websocket conneciton.
    const socket = new WebSocket(websocket_url());
    socket.binaryType = 'arraybuffer';
    init_placement((bytes) => socket.send(bytes));

    socket.addEventListener("open", () => {
        console.log("WebSocket connection accepted by server. Waiting to join a match...");
    });

    socket.addEventListener("message", (event:MessageEvent<Core.ByteBuffer>) => on_socket_message(event, canvas, socket));

    socket.addEventListener("error", () => {
        console.error("WebSocket connection failed.");
    });

    socket.addEventListener("close", () => {
        console.log("WebSocket connection closed.");
    });
}

start();
