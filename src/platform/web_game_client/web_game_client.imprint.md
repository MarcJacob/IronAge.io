# WEB GAME CLIENT: WASM SIDE

The web build of the game client: a thin layer of exports over the client backend (*src/game_client/*), compiled to wasm. The TypeScript side lives in *front/* (see its imprints).

## Rules

- The client backend and this layer are one unity build (*web_game_client_main.cpp* includes the backend first). This layer may use backend internals; the backend knows nothing of this layer.
  Needs that only the web client has (JS-friendly data shapes) are built here, never added to the platform-independent backend.
- No global with a non-trivial constructor (no default member initializers): the build has no single entry point, so such constructors would re-run on every exported call. See *web_game_client.h*.
- Data crosses to JS as bytes at offsets (exports return offsets into wasm memory). Anything JS reads at fixed offsets must be mirrored by hand in *front/src/backend.ts*.
  Prefer Game Data Objects (self-describing) over fixed layouts.

## Inspection

Entity inspection fills one buffer: the entity's Game Data Object, then an optional extra GDO built here for entity types that need data the world sim does not shape for the UI.
The buffer is overwritten by every inspection: JS copies what it needs right away.
