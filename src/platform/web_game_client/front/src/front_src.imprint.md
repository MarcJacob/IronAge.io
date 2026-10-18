# WEB CLIENT FRONT: TYPESCRIPT ARCHITECTURE

How the TypeScript of the web client is organized. See *../front.imprint.md* for how it gets built.

## Layers

- *backend.ts* is the only file that knows about the wasm backend (exports, memory layouts). Everything else sees plain JS values.
  Structures read at fixed offsets mirror the C++ ones by hand: keep both in sync.
- *main.ts* owns the lifecycle of the app: start-up order, the websocket, backend match start / end, canvas visibility.
  It hands every server message to the backend, then to the modes. It does not decide what the player sees or can do.
- *core.ts* holds the shared basics: frame loop, match joined / ended callbacks, message sending, and the Mode system (below).
- *modes/* decides what the player sees and can do at any moment. See below.
- *render.ts* draws the match from the backend's render state and a configuration object. It never decides what to show.
- *ui/* holds DOM widgets (static markup lives in *web/index.html*). They show / hide / refresh themselves from what they are told,
  and only emit signals back (callbacks). They never send input commands or switch modes.
  The bottom bar is called the **action bar** (*ui/settlement_bar_ui.ts*, markup in *web/index.html*, style in *web/style.css*):
  modes show their actions there, hotkeyed ones on the left, committed ones (no hotkey) on the right.
- *input.ts* is a library plus the DOM listener registration. It forwards clicks, keys and Escape to the modes
  and holds input state (selection, held keys, camera input) and the generic command functions (*send_command*, *is_command_valid*).
  It knows no command by name: modes build and send their commands themselves, by command name and property name (see Game Data Objects in the C++ web client sources).
- *camera.ts* is the frontend camera.

## Modes

A mode is an object of a class extending *Mode* (*core.ts*), parameterized through its constructor. Modes live in a stack in *core.ts*.
The bottom one is the base mode, which cannot be escaped out of. *start_mode_stack* replaces the whole stack with a new base,
*push_mode(mode, parent)* enters a child mode, *pop_mode* returns to the previous one (Escape does this by default), *replace_top_mode* swaps the top.
*push_mode* takes the mode that is starting the new one as parent: every mode above the parent is popped first (one at a time, through the normal exit path,
without Escape handling), then the new mode is pushed directly on the parent. A mode therefore never stacks on top of a sibling.

The current tree (extend it, keep it as a tree):
- Lobby base: no match running.
- Game base: a match is running, until it ends. Runs the camera. Has one child at a time:
  - Spawn pick: placement phase.
  - Main play: ordinary gameplay. Has one child at a time:
    - Settlement selected: actions with hotkeys. Has one child at a time:
      - Road building: stays active after an order.
      - Army raising: one click raises one army, then the mode exits.
    - Army selected: committed action (Settle), no hotkey.

### Events

Modes override what they need on *Mode*: entry, exit, suspended (a child was pushed above), resumed (the child above was popped),
frame update, click, context menu, key down, escape, net message.
- Input events (click, context menu, key down, escape) are offered to the modes **from the top of the stack down**. A handler returns true if it handled the event,
  which stops the dispatch. The default handlers return false, so the event falls through to the mode below (Escape defaults to popping the mode and returns true).
  A mode handles only the inputs it owns (its exit key, its clicks) and returns false for the rest: to switch to a sibling mode, the key falls through to the parent,
  which pushes the other mode.
- Frame updates and net messages go to **every mode of the stack, bottom first** (net messages top first).
  Parents keep running while a child is on top. They know their children, so they adapt using *is_top* (kept by the stack, read-only for modes)
  and the suspended / resumed events.
- Because frame updates run bottom first, a child's frame update has the last word on shared state (e.g. render config).

### Rules for writing modes

- A mode sets up what it needs in entry (or resumed) and undoes it in exit (or suspended). Whatever it shows, it hides.
- A mode drives the screen only through: the render config in *render.ts*, the *ui/* functions, and *input.ts* senders.
  It does not touch the DOM or canvas directly.
- Entering and leaving modes follows state when it can. A parent pushes its child when the state says so
  (e.g. a settlement is selected), and the child pops itself when the state stops saying so, rather than relying on every click path.
- Behavior shared by several modes (e.g. left click selection) lives in the lowest mode that owns it (Main play) and reaches the others by fallthrough.
  General helpers several modes need (e.g. ownership checks in *main.ts*) live outside *modes/*, not copied.
- A mode sends the commands for the orders it owns itself (e.g. Army selected sends move and attack on right click).
- A mode that must not be left (base modes, and the modes right above them) overrides Escape to do nothing.
- Hotkeys must stay clear of the camera keys (WASD, arrows, numpad + and -). A hotkey that enters a mode is also passed to it so it can leave with it.
- To know if a command would be accepted before sending it, ask the backend (it reuses the server's validity checks)
  instead of re-implementing the rules in TypeScript.

## Render configuration

*RENDER_CONFIG* in *render.ts* is a plain object modes mutate: which layers and entity types are drawn, ownership tint,
the outlined entity, the path preview, overlay dots. Defaults draw the full game view. Add a field there when a mode needs a new way to
change the picture, instead of letting *render.ts* read mode or input state.

## Practical notes

- The web server preloads every file under the web root into memory and only reloads them on a server hot reload.
  It has a maximum file count: adding TypeScript files adds served files. The deployment copy never deletes files, so stale ones pile up.
- The browser may serve cached modules after a redeploy: hard reload when changes seem to have no effect.
