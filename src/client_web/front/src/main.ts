// Web client entry point: start-up order and the socket-driven message loop.
// Every received websocket message is handed to the backend to parse and apply; the backend's return value tells this
// layer what happened (see MESSAGE_TYPE), which is the only signal it reacts to. There is no separate JS-driven tick loop:
// the server drives ticks, the client just applies whatever it's told.

import * as Core from "./core.js"
import * as Backend from "./backend.js"

import { init_render, draw, page_to_world } from './render.js';
import { init_input } from './input.js';
import { init_debug_panel, update_debug_panel } from './debug.js';

function websocket_url() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${protocol}//${window.location.host}/ws`;
}

// Handler for messages received from the Websocket.
function on_socket_message(event: MessageEvent<Core.ByteBuffer>, canvas: HTMLCanvasElement, socket: WebSocket)
{
    const messageType = Backend.process_net_message(new Core.ByteBuffer(event.data));
    if (messageType === Backend.MESSAGE_TYPE.INVALID) return; // Message could not be processed: nothing changed.

    // On joining a match, initialize rendering & input systems.
    if (messageType === Backend.MESSAGE_TYPE.MATCH_JOINED)
    {
        console.log("Joined match as player", Backend.read_controlled_player_id());

        let matchInfo = Backend.read_match_info();
        init_render(canvas, matchInfo);
        init_input(canvas);
        canvas.hidden = false;
    }

    const renderState = Backend.read_render_state();
    draw(renderState);
    update_debug_panel(renderState);

    // The backend may have built a reply (e.g. pending input) in response to the message just processed.
    // There is no separate send loop: outgoing messages only ever go out piggybacked on a received one.
    const outgoing = Backend.read_pending_output_message();
    if (outgoing !== null) socket.send(outgoing);
}

// ENTRY POINT

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

    // Get foreground canvas.
    const canvas = document.getElementById("game_canvas_foreground") as HTMLCanvasElement;

    // Initialize debug panel / text container.
    init_debug_panel(document.getElementById("debug_entity_states") as HTMLElement);

    // Initialize websocket conneciton.
    const socket = new WebSocket(websocket_url());
    socket.binaryType = 'arraybuffer';

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
