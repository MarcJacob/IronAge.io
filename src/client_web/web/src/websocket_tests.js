// WebSocket protocol tests, run on demand from the "Run WebSocket tests" button of the index page.
//
// Every test opens its own connection(s) to the game server's /ws endpoint and checks how the server answers.
// They rely on the server's game message echo (game_server_test_echo_game_client): every valid game message sent is expected back unchanged.
//
// Game message layout (little-endian): u16 message_type_code | u16 payloadSize | payload bytes.

// NOTE(Marc): This entire file is obviously made by my best friend Claude. What a good lad.

// Values mirrored from the server. Update them here if the server constants change.
const SERVER_RECEPTION_BUFFER_SIZE = 2048; // WEB_CLIENT_RECEPTION_BUFFER_SIZE: biggest frame the server accepts, header included.
const SERVER_WEBSOCKET_TIMEOUT_MS = 6000; // WEBSOCKET_CLIENT_TIMEOUT_MS: a client that sends nothing (not even a pong) for this long gets dropped.

const GAME_MESSAGE_HEADER_SIZE = 4;

// Largest game message payload that still fits a frame in the server's reception buffer.
// A browser frame for a payload over 125 bytes has an 8 byte header: 2 base, 2 extended length, 4 masking key.
const MAX_MESSAGE_PAYLOAD = SERVER_RECEPTION_BUFFER_SIZE - 8 - GAME_MESSAGE_HEADER_SIZE;

const DEFAULT_TIMEOUT_MS = 3000;

// Close status codes the server answers with.
const CLOSE_UNSUPPORTED_DATA = 1003;
const CLOSE_INVALID_PAYLOAD = 1007;
const CLOSE_TOO_BIG = 1009;

// BEGIN HELPERS

function websocket_url() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${protocol}//${window.location.host}/ws`;
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

// Builds a game message. declared_size is what goes in the payloadSize field, which can be made to disagree with the actual payload.
function build_message(type_code, payload, declared_size = payload.length) {
    const bytes = new Uint8Array(GAME_MESSAGE_HEADER_SIZE + payload.length);
    const view = new DataView(bytes.buffer);
    view.setUint16(0, type_code, true);
    view.setUint16(2, declared_size, true);
    bytes.set(payload, GAME_MESSAGE_HEADER_SIZE);
    return bytes;
}

// Payload of the given size whose content depends on the seed, so that mix-ups between messages get noticed.
function build_payload(size, seed) {
    const payload = new Uint8Array(size);
    for (let i = 0; i < size; i++) {
        payload[i] = (i * 7 + seed) & 0xFF;
    }
    return payload;
}

function describe_bytes(bytes) {
    const shown = Array.from(bytes.slice(0, 16)).join(',');
    return `[${shown}${bytes.length > 16 ? ',...' : ''}] (${bytes.length} bytes)`;
}

function check(condition, message) {
    if (!condition) throw new Error(message);
}

// Thrown by a test that can't run in the current setup.
class TestSkipped extends Error {}

function check_bytes_equal(actual, expected, what) {
    let equal = actual.length === expected.length;
    for (let i = 0; equal && i < actual.length; i++) {
        equal = actual[i] === expected[i];
    }
    check(equal, `${what}: expected ${describe_bytes(expected)}, got ${describe_bytes(actual)}`);
}

// Connection to the server with received messages queued up so tests can wait on them one after the other.
class TestClient {
    constructor(socket) {
        this.socket = socket;
        this.messages = []; // Received messages nobody asked for yet.
        this.receive_waiter = null;
        this.close_event = null;
        this.close_waiters = [];

        socket.binaryType = 'arraybuffer';

        socket.addEventListener('message', (event) => {
            if (!(event.data instanceof ArrayBuffer)) return; // The server only ever sends binary frames.

            const bytes = new Uint8Array(event.data);
            if (this.receive_waiter) {
                const waiter = this.receive_waiter;
                this.receive_waiter = null;
                waiter.resolve(bytes);
            } else {
                this.messages.push(bytes);
            }
        });

        socket.addEventListener('close', (event) => {
            this.close_event = event;

            if (this.receive_waiter) {
                const waiter = this.receive_waiter;
                this.receive_waiter = null;
                waiter.reject(new Error(`connection closed by the server (code ${event.code}) while waiting for a message`));
            }
            for (const waiter of this.close_waiters) waiter(event);
            this.close_waiters = [];
        });
    }

    static connect(timeout_ms = DEFAULT_TIMEOUT_MS, url = websocket_url()) {
        return new Promise((resolve, reject) => {
            const socket = new WebSocket(url);
            const client = new TestClient(socket);

            const timer = setTimeout(() => reject(new Error(`connection not open after ${timeout_ms} ms`)), timeout_ms);
            socket.addEventListener('open', () => { clearTimeout(timer); resolve(client); });
            socket.addEventListener('error', () => { clearTimeout(timer); reject(new Error('connection failed')); });
        });
    }

    send(data) {
        this.socket.send(data);
    }

    // Waits for the next message from the server.
    receive(timeout_ms = DEFAULT_TIMEOUT_MS) {
        if (this.messages.length > 0) return Promise.resolve(this.messages.shift());
        if (this.close_event) return Promise.reject(new Error(`connection closed (code ${this.close_event.code}) with no message waiting`));

        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this.receive_waiter = null;
                reject(new Error(`no message received within ${timeout_ms} ms`));
            }, timeout_ms);

            this.receive_waiter = {
                resolve: (bytes) => { clearTimeout(timer); resolve(bytes); },
                reject: (error) => { clearTimeout(timer); reject(error); },
            };
        });
    }

    // Waits for the connection to be closed, and returns the close event.
    wait_closed(timeout_ms = DEFAULT_TIMEOUT_MS) {
        if (this.close_event) return Promise.resolve(this.close_event);

        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => reject(new Error(`connection still open after ${timeout_ms} ms`)), timeout_ms);
            this.close_waiters.push((event) => { clearTimeout(timer); resolve(event); });
        });
    }

    close(code) {
        if (code === undefined) this.socket.close();
        else this.socket.close(code);
    }

    close_quietly() {
        if (this.socket.readyState === WebSocket.OPEN || this.socket.readyState === WebSocket.CONNECTING) {
            this.socket.close();
        }
    }

    // Sends the message and checks that the very same bytes come back.
    async check_echo(message, what) {
        this.send(message);
        check_bytes_equal(await this.receive(), message, what);
    }
}

// Sends the data and checks that the server closes the connection with the expected status code.
async function check_rejected(client, data, expected_code, what) {
    client.send(data);
    const event = await client.wait_closed();
    check(event.code === expected_code, `${what}: expected close code ${expected_code}, got ${event.code}`);
}

// END HELPERS

// BEGIN TESTS
// Each test is given a context to open connections with. Connections are closed when the test is done.

const TESTS = [
    // Echo: valid game messages come back unchanged.

    {
        name: 'echo: basic message',
        run: async (ctx) => {
            const client = await ctx.connect();
            await client.check_echo(build_message(7, [1, 2, 3]), 'echoed message');
        },
    },
    {
        name: 'echo: message without payload',
        run: async (ctx) => {
            const client = await ctx.connect();
            await client.check_echo(build_message(9, []), 'echoed message');
        },
    },
    {
        name: `echo: largest message that fits (${MAX_MESSAGE_PAYLOAD} bytes of payload)`,
        run: async (ctx) => {
            const client = await ctx.connect();
            await client.check_echo(build_message(3, build_payload(MAX_MESSAGE_PAYLOAD, 5)), 'echoed message');
        },
    },
    {
        name: 'echo: messages come back in the order they were sent',
        run: async (ctx) => {
            const client = await ctx.connect();
            const messages = [];
            for (let i = 1; i <= 5; i++) messages.push(build_message(i, build_payload(i * 3, i)));

            for (const message of messages) client.send(message);
            for (const message of messages) check_bytes_equal(await client.receive(), message, 'echoed message');
        },
    },
    {
        name: 'echo: burst of 200 messages sent at once',
        run: async (ctx) => {
            const client = await ctx.connect();
            const messages = [];
            for (let i = 0; i < 200; i++) messages.push(build_message(i & 0xFFFF, build_payload(10 + (i % 5), i)));

            for (const message of messages) client.send(message);
            for (let i = 0; i < messages.length; i++) {
                check_bytes_equal(await client.receive(), messages[i], `echoed message ${i}`);
            }
        },
    },
    {
        name: 'echo: 100 messages of varied sizes sent at once',
        run: async (ctx) => {
            const client = await ctx.connect();

            let seed = 12345;
            const random = () => { seed = (Math.imul(seed, 1103515245) + 12345) & 0x7FFFFFFF; return seed; };

            const messages = [];
            for (let i = 0; i < 100; i++) messages.push(build_message(i, build_payload(random() % 400, i)));

            for (const message of messages) client.send(message);
            for (let i = 0; i < messages.length; i++) {
                check_bytes_equal(await client.receive(), messages[i], `echoed message ${i}`);
            }
        },
    },
    {
        name: 'echo: several connections at once each get their own messages back',
        run: async (ctx) => {
            const clients = [await ctx.connect(), await ctx.connect(), await ctx.connect()];
            const messages = clients.map((_, i) => build_message(100 + i, build_payload(20, i)));

            clients.forEach((client, i) => client.send(messages[i]));
            for (let i = 0; i < clients.length; i++) {
                check_bytes_equal(await clients[i].receive(), messages[i], `echoed message of connection ${i}`);
            }
        },
    },
    {
        name: 'echo: connection slots can be re-used after connections are closed',
        run: async (ctx) => {
            for (let i = 0; i < 6; i++) {
                const client = await ctx.connect();
                await client.check_echo(build_message(i, build_payload(8, i)), `echoed message on connection ${i}`);

                client.close(1000);
                await client.wait_closed();
            }
        },
    },

    // Rejections: frames that can't be accepted get a Close frame with the right status code.

    {
        name: 'reject: payload size field larger than the actual payload -> 1007',
        run: async (ctx) => {
            const client = await ctx.connect();
            await check_rejected(client, build_message(7, [1, 2, 3], 5), CLOSE_INVALID_PAYLOAD, 'message declaring too much payload');
        },
    },
    {
        name: 'reject: payload size field smaller than the actual payload -> 1007',
        run: async (ctx) => {
            const client = await ctx.connect();
            await check_rejected(client, build_message(7, [1, 2, 3], 1), CLOSE_INVALID_PAYLOAD, 'message declaring too little payload');
        },
    },
    {
        name: 'reject: message shorter than a game message header -> 1007',
        run: async (ctx) => {
            const client = await ctx.connect();
            await check_rejected(client, new Uint8Array([1, 0]), CLOSE_INVALID_PAYLOAD, 'two byte message');
        },
    },
    {
        name: 'reject: text message -> 1003',
        run: async (ctx) => {
            const client = await ctx.connect();
            await check_rejected(client, 'hello', CLOSE_UNSUPPORTED_DATA, 'text message');
        },
    },
    {
        name: `reject: message one byte over the largest that fits (${MAX_MESSAGE_PAYLOAD + 1} bytes of payload) -> 1009`,
        run: async (ctx) => {
            const client = await ctx.connect();
            await check_rejected(client, build_message(3, build_payload(MAX_MESSAGE_PAYLOAD + 1, 5)), CLOSE_TOO_BIG, 'message just over the limit');
        },
    },
    {
        name: 'reject: much larger than the reception buffer -> 1009',
        run: async (ctx) => {
            const client = await ctx.connect();
            await check_rejected(client, new Uint8Array(SERVER_RECEPTION_BUFFER_SIZE + 1000), CLOSE_TOO_BIG, 'oversized message');
        },
    },

    // Closing.

    {
        name: 'close: client close with a status code is answered with the same code',
        run: async (ctx) => {
            const client = await ctx.connect();
            client.close(1000);

            const event = await client.wait_closed();
            check(event.code === 1000, `expected close code 1000, got ${event.code}`);
        },
    },
    {
        name: 'close: client close without a status code is answered without one (1005)',
        run: async (ctx) => {
            const client = await ctx.connect();
            client.close();

            const event = await client.wait_closed();
            check(event.code === 1005, `expected close code 1005, got ${event.code}`);
        },
    },

    // Keepalive. The server pings connections it hasn't sent anything to for a while, and browsers answer pings on their own, without any script involved.
    // The pings themselves can't be seen from a page, so this only checks that a silent connection outlives the timeout.

    {
        name: `keepalive: a silent connection stays open past the server's timeout (${SERVER_WEBSOCKET_TIMEOUT_MS} ms)`,
        run: async (ctx) => {
            const client = await ctx.connect();
            await sleep(SERVER_WEBSOCKET_TIMEOUT_MS + 2000);

            check(client.close_event === null, `connection was closed by the server (code ${client.close_event && client.close_event.code})`);
            await client.check_echo(build_message(1, [1, 2, 3]), 'echoed message after the silence');
        },
    },

    // Origin.

    {
        name: 'origin: a connection from a page served under a different host name than the one it connects to is refused',
        run: async (ctx) => {
            // The page's origin is the host it was loaded from. Reaching the same server under its other loopback name makes Origin and Host disagree.
            const other_host = { 'localhost': '127.0.0.1', '127.0.0.1': 'localhost' }[window.location.hostname];
            if (other_host === undefined) throw new TestSkipped(`page not loaded from localhost or 127.0.0.1 (${window.location.hostname})`);

            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const port = window.location.port === '' ? '' : `:${window.location.port}`;

            let connected = false;
            try {
                await ctx.connect(`${protocol}//${other_host}${port}/ws`);
                connected = true;
            } catch (error) {
                // Expected: the server answers the upgrade with a 403.
            }
            check(!connected, 'the connection was accepted although Origin and Host differ');
        },
    },
];

// END TESTS

async function run_test(test) {
    const clients = [];
    const ctx = {
        connect: async (url = websocket_url()) => {
            const client = await TestClient.connect(DEFAULT_TIMEOUT_MS, url);
            clients.push(client);
            return client;
        },
    };

    try {
        await test.run(ctx);
        return null;
    } catch (error) {
        return error;
    } finally {
        for (const client of clients) client.close_quietly();
    }
}

// Runs every test one after the other. log is called with each line of output.
export async function run_websocket_tests(log) {
    let passed = 0;
    let failed = 0;
    let skipped = 0;

    log(`Running ${TESTS.length} WebSocket tests against ${websocket_url()}`);

    for (const test of TESTS) {
        const start = performance.now();
        const error = await run_test(test);
        const duration = Math.round(performance.now() - start);

        if (error === null) {
            passed++;
            log(`[PASS] ${test.name} (${duration} ms)`);
        } else if (error instanceof TestSkipped) {
            skipped++;
            log(`[SKIP] ${test.name}\n       ${error.message}`);
        } else {
            failed++;
            log(`[FAIL] ${test.name}\n       ${error.message}`);
        }

        await sleep(50); // Leave the server a moment to process the closed connections.
    }

    log(`\n${passed} passed, ${failed} failed, ${skipped} skipped.`);
    return failed === 0;
}

// Hooks up the button and output area of the index page.
function init_websocket_tests() {
    const button = document.getElementById('run_websocket_tests');
    const output = document.getElementById('websocket_test_output');
    if (button === null || output === null) return;

    button.addEventListener('click', async () => {
        button.disabled = true;
        output.textContent = '';

        try {
            await run_websocket_tests((line) => {
                output.textContent += line + '\n';
                console.log(line);
            });
        } finally {
            button.disabled = false;
        }
    });
}

init_websocket_tests();
