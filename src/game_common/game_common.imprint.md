# GAME COMMON SOURCES

Game Common code, which mostly covers starting and simulating a match.

Used by client and game server to synchronize with eachother using a Lockstep strategy.

The Game Common Code is for now directly included (Unity built) into whatever needs it. It is possible that later we will use static or dynamic linkage.

## Guidelines

The common code must be useable by any host / app, including the web client WASM backend which is free-standing, meaning it is not allowed to make any use
of the C / C++ standard library. Any and all functions it calls must exist in this codebase.

Any system must work with cross-machine determinism in mind: avoid randommess, or using data specific to the local machine.

## Match system

The common code holds the implementation of the match system on a low level, integrating the passage of time into the match's world simulation
using a set of inputs as a way of impacting the world in non-deterministic ways (mostly Player / AI input, received from the network or any source).

Therefore matches are basically a simple tick-based machine:

-> Receive aggregated input for tick N (with the guarantee that no more input will come targeting that specific tick).
-> Execute the tick advancement code.
-> Wait until next batch of inputs is received.

Beyond just the match system itself, the common code generally exposes its logic so whatever hosts it can have some insight about the way the game works:
- Game data inspection (dynamic UI elements, AI decision making...)
- Prediction / Extrapolation (client-side smoothing, AI decision making...)
- ...
