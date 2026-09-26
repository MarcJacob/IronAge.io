// Web client entry point: start-up order and the frame loop.

import { load_backend, begin_match, tick_match, set_target_loc, read_render_state, match_info } from './backend.js';
import { init_render, draw, page_to_world } from './render.js';
import { init_input } from './input.js';

const MAX_TICKS_PER_FRAME = 5;

let lastFrameTime = 0; // In seconds.
let timeSinceTick = 0; // In seconds.

function web_frame() {
    const now = performance.now() / 1000;
    timeSinceTick += now - lastFrameTime;
    lastFrameTime = now;

    const tickPeriod = 1 / match_info.tick_rate;

    let ticksThisFrame = 0;
    while (timeSinceTick >= tickPeriod && ticksThisFrame < MAX_TICKS_PER_FRAME) {
        tick_match();

        ticksThisFrame++;
        timeSinceTick -= tickPeriod;
    }

    // If still behind after hitting the cap, the backlog stays in timeSinceTick and is worked off over the next frames (capped catch-up speed).

    if (ticksThisFrame > 0) draw(read_render_state()); // No need to update rendering unless there was a tick. Later this will also be needed if there's view movement.

    requestAnimationFrame(web_frame);
}

function websocket_url() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${protocol}//${window.location.host}/ws`;
}

async function start_after_websocket_open(canvas, socket) {
    console.log("WebSocket connection accepted by server.");

    console.log("Starting local match.");
    if (!begin_match()) {
        socket.close();
        return;
    }

    init_render(canvas, match_info);
    init_input(canvas, page_to_world, set_target_loc);
    canvas.hidden = false;

    lastFrameTime = performance.now() / 1000;
    requestAnimationFrame(web_frame);
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
    const socket = new WebSocket(websocket_url());

    socket.addEventListener("open", async () => {
        await start_after_websocket_open(canvas, socket);
    });

    socket.addEventListener("error", () => {
        console.error("WebSocket connection failed.");
    });

    socket.addEventListener("close", () => {
        console.log("WebSocket connection closed.");
    });
}

start();
