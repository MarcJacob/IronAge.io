// Web client entry point: start-up order and the socket-driven message loop.
// Every received websocket message is handed to the backend to parse and apply; the backend's return value tells this
// layer what happened (see MESSAGE_TYPE), which is the only signal it reacts to. There is no separate JS-driven tick loop:
// the server drives ticks, the client just applies whatever it's told.

import { load_backend, process_net_message, read_pending_output_message, read_match_info, read_controlled_player_id, set_target_loc, read_render_state, match_info, MESSAGE_TYPE } from './backend.js';
import { init_render, draw, page_to_world } from './render.js';
import { init_input } from './input.js';
import { init_debug_panel, update_debug_panel } from './debug.js';

function websocket_url() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${protocol}//${window.location.host}/ws`;
}

function on_socket_message(event, canvas, socket) {
    const messageType = process_net_message(new Uint8Array(event.data));
    if (messageType === null) return; // Message could not be processed: nothing changed.

    if (messageType === MESSAGE_TYPE.MATCH_JOINED) {
        console.log("Joined match as player", read_controlled_player_id());

        read_match_info();
        init_render(canvas, match_info);
        init_input(canvas, page_to_world, set_target_loc);
        canvas.hidden = false;
    }

    const renderState = read_render_state();
    draw(renderState);
    update_debug_panel(renderState);

    // The backend may have built a reply (e.g. pending input) in response to the message just processed.
    // There is no separate send loop: outgoing messages only ever go out piggybacked on a received one.
    const outgoing = read_pending_output_message();
    if (outgoing !== null) socket.send(outgoing);
}

async function start() {
    console.log("Loading client backend...");
    try {
        await load_backend('IronAgeIO_WebClient.wasm');
    } catch (e) {
        console.error("Failed to load client backend.", e);
        return;
    }

    const canvas = document.getElementById("game_canvas_foreground");
    init_debug_panel(document.getElementById("debug_entity_states"));

    const socket = new WebSocket(websocket_url());
    socket.binaryType = 'arraybuffer';

    socket.addEventListener("open", () => {
        console.log("WebSocket connection accepted by server. Waiting to join a match...");
    });

    socket.addEventListener("message", (event) => on_socket_message(event, canvas, socket));

    socket.addEventListener("error", () => {
        console.error("WebSocket connection failed.");
    });

    socket.addEventListener("close", () => {
        console.log("WebSocket connection closed.");
    });
}

start();
